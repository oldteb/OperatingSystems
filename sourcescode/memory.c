/**************************************************************************

    memory.c

    Author:                 Yunhe Tang
    Complete Time:          12/10/2013


**************************************************************************/


#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             "stdlib.h"
#include             "z502.h"
#include             "process.h"



/***********************************************************************
    PageFaultHandler
    
    It is a service routine dealing with the page fault occurs in failt 
    handler. It will allocate an avaliable frame the the requesing process. 

************************************************************************/
void PageFaultHandler(Z502CONTEXT* curr_context, INT32 VirtualPageNumber){
    INT32 frameNum32;
    INT32 pid;
    INT32 error;
    INT32 new_FrameValue;
    
    if(VirtualPageNumber >= VIRTUAL_MEM_PGS || VirtualPageNumber < 0){
        printf("Error:Invalid memory address.\n");
        Z502Halt();
    }
    
    Get_Process_ID("",&pid,&error);
    if(error == ERROR_GET_PID_FAILED){
        printf("Error: get pid failed.\n");
        system("pause");
    }
    
    frameNum32 = (INT16)GetAvaliableFrame();
    if(frameNum32 == -1){       
        // need page replaced.
        frameNum32 = SwapPage(pid,VirtualPageNumber);
    }
    else{
        // add reserved bit check.
        curr_context->page_table_ptr[VirtualPageNumber] = ((INT16)frameNum32 | 0x8000);
    }
    
    /* Renew page load table. */
    AddToPageLoadList(frameNum32);
    
    /* Refresh frame table. */
    new_FrameValue = ((pid & 0x0000ffff)<<12) | VirtualPageNumber;          //pid Shift right 12 bits.
    new_FrameValue = new_FrameValue | 0x80000000;
    FrameTable[frameNum32] = new_FrameValue;
    
    MP_setup(frameNum32,pid,VirtualPageNumber,(INT32)4);
}


/***********************************************************************
    SwapPage
    
    When dealing with the page fault, system either simply allocates an 
    empty frame to whom is requesting or runs sub-routine SwapPage to write 
    back an old page and give it out.  

************************************************************************/

INT32 SwapPage(INT32 pid, INT32 VirtualPageNumbe){
    INT32 i;
    INT32 frameNum32;
    ptr_PLNode victim;
    INT32 temp;
    INT32 temp_pid;
    INT32 temp_vpn;
    Ptr_PCB temp_PCB;
    INT16 frameNum16;
    char PageDataRead[PGSIZE];
    char PageDataWrite[PGSIZE];

    do{
        frameNum32 = pageLoadList.head->frameNum;
        temp = FrameTable[frameNum32];
        temp_pid = (temp>>12) & 0x0000ffff;
        temp_vpn = temp & 0x00000fff;
        temp_PCB = PidToPtr(temp_pid);
        frameNum16 = ((Z502CONTEXT*)(temp_PCB->ptr_context))->page_table_ptr[temp_vpn];
        if((frameNum16 & 0x2000) == 0x2000){        // Check the Reference bit.
            ((Z502CONTEXT*)(temp_PCB->ptr_context))->page_table_ptr[temp_vpn] = frameNum16 & 0xdfff;
            RemoveFromPageLoadList(pageLoadList.head);
            AddToPageLoadList(frameNum32);
            continue;
        }
        break;
    }while(TRUE);
    
    // victim points to the swap frame.
    victim = pageLoadList.head;
    // Take the swap frame off the Page load list.
    RemoveFromPageLoadList(pageLoadList.head);
    
    if((frameNum16 & 0x4000) == 0x4000){            // Check the Modified bit.
        // Write back to disk.
        for(i=0; i<PGSIZE ;i++){
            *(PageDataWrite+i) = MEMORY[frameNum32*PGSIZE+i];
        }
        
        DiskWrite(temp_pid, temp_vpn, (char* )PageDataWrite);
        DiskRead(temp_pid, temp_vpn, (char* )PageDataRead);
        Z502_MODE = KERNEL_MODE;
        for(i=0; i<PGSIZE ;i++){
            if(PageDataWrite[i] != PageDataRead[i]){
            printf("data write back fault.\n"); 
            system("pause");
            }
        }
        
        // Set reserved bit in old page entry.
        ((Z502CONTEXT*)(temp_PCB->ptr_context))->page_table_ptr[temp_vpn] = 0x1000;
    }

    // Set new page entry.
    frameNum16 = ((Z502CONTEXT*)(PidToPtr(pid)->ptr_context))->page_table_ptr[VirtualPageNumbe];

    // load the data from the disk.
    if((frameNum16 & 0x1000) == 0x1000){        // this page is stored on disk.
        ((Z502CONTEXT*)(PidToPtr(pid)->ptr_context))->page_table_ptr[VirtualPageNumbe] =  (INT16)frameNum32 | 0x9000;
        DiskRead(pid, VirtualPageNumbe, (char *)PageDataRead);
        
        MEM_WRITE(VirtualPageNumbe * PGSIZE,    PageDataRead);
        MEM_WRITE(VirtualPageNumbe * PGSIZE+4,  PageDataRead+4);
        MEM_WRITE(VirtualPageNumbe * PGSIZE+8,  PageDataRead+8);
        MEM_WRITE(VirtualPageNumbe * PGSIZE+12, PageDataRead+12);
    }
    else{
        ((Z502CONTEXT*)(PidToPtr(pid)->ptr_context))->page_table_ptr[VirtualPageNumbe] =  (INT16)frameNum32 | 0x8000;
    }
    
    return frameNum32;
}

/***********************************************************************
    GetAvaliableFrame
    
    Return the avalaible frame number. If there is no empty frame existed,
    return -1.  

************************************************************************/
INT32 GetAvaliableFrame(){
    INT32 i;    
    for(i = 0; i < PHYS_MEM_PGS; i++){
        if((FrameTable[i] & 0x80000000) == 0x0)  return i;
    }   
    return -1;
}

/***********************************************************************
    AddToPageLoadList
    
    Add the latest reference frame to the pageload list to record the order
    of which frame comes into the main memory.  

************************************************************************/
void AddToPageLoadList(INT32 frameNum){
    ptr_PLNode new_PLNode;
    
    new_PLNode = (ptr_PLNode)malloc(sizeof(PLNode));
    new_PLNode->frameNum = frameNum;
    new_PLNode->next = NULL;

    if(pageLoadList.head == NULL){
        pageLoadList.head = new_PLNode;
        pageLoadList.rear = new_PLNode;
        return;
    }
    else{
        pageLoadList.rear->next = new_PLNode;
        pageLoadList.rear = new_PLNode;
    }
}

/***********************************************************************
    RemoveFromPageLoadList
    
    Remove the latest reference frame to the pageload list to record the order
    of which frame comes into the main memory.  

************************************************************************/
void RemoveFromPageLoadList(ptr_PLNode rmv_PLNode){
    ptr_PLNode temp;

    if(pageLoadList.head == pageLoadList.rear){
        if(rmv_PLNode == pageLoadList.head){
            pageLoadList.head = NULL;
            pageLoadList.rear = NULL;
        }
        else{
            printf("Error in RemoveFromPageLoadList.\n");
            system("pause");
        }
    }
    else{
        if(rmv_PLNode == pageLoadList.head){
            pageLoadList.head = rmv_PLNode->next;
            rmv_PLNode->next = NULL;
        }
        else{
            temp = pageLoadList.head;
            while(temp->next != NULL){
                if(temp->next == rmv_PLNode){
                    temp->next = rmv_PLNode->next;
                    if(rmv_PLNode == pageLoadList.rear){
                        pageLoadList.rear = temp;
                    }
                    rmv_PLNode->next = NULL;
                    break;
                }
                temp = temp->next;
            }
            // may be wrong here...
        }
    }
}




/***********************************************************************
    DiskRead
    
    RThis is a system call service routine, which invoked by the interrupt
    handler.  

************************************************************************/
void DiskRead(INT32 disk_id, INT32 sector, char * data){
    INT32 Temp;
    INT32 error = 0;
    int pid;
    Get_Process_ID("",&pid,&error);
    if(dq[disk_id]->head == NULL && MyInterruptStatus != disk_id+4){
        lastDiskAct[disk_id] = pid;
        Disk_R(disk_id, sector, data,0);
        addToDiskQueue(disk_id, sector, data, D_READ);
    }
    else{
        addToDiskQueue(disk_id, sector, data, D_READ);
    }
    Dispatcher(SWITCH_CONTEXT_SAVE_MODE);
}

/***********************************************************************
    Disk_R
    
    This is the routine that actually perform the disk read.  

************************************************************************/
void Disk_R(INT32 disk_id, INT32 sector, char * data,int mark){
    INT32 Temp;
    INT32 error;

    // Add lock
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK,1,TRUE,&error);
    if(error == FALSE){
        printf("Error on disk read lock.\n");
        system("pause");
    }

    MEM_WRITE(Z502DiskSetID, &disk_id);
    MEM_WRITE(Z502DiskSetSector, &sector);
    MEM_WRITE(Z502DiskSetBuffer, (INT32 * )data);
    Temp = 0;                        // Specify a write
    MEM_WRITE(Z502DiskSetAction, &Temp);
    Temp = 0;                        // Must be set to 0
    MEM_WRITE(Z502DiskStart, &Temp);
    
    /* Disk should now be started */
    MEM_WRITE(Z502DiskSetID, &disk_id);
    MEM_READ(Z502DiskStatus, &Temp);
    if (Temp == DEVICE_FREE){
        if(mark == 1)  printf("Ocurr in interrupt handler.\n");
        printf("Error:Disk read failed to start.\n");
        printf("last act on disk 2 is pid %d.\n",lastDiskAct[2]);
        printf("Disk id = %d\n",disk_id);
        PrintDiskQueue(disk_id);
        PrintReadyQueue();
        system("pause");
    }   

    // Release the lock
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK,0,TRUE,&error);
    if(error == FALSE){
        printf("Error on disk read lock.\n");
        system("pause");
    }
}

/***********************************************************************
    DiskWrite
    
    RThis is a system call service routine, which invoked by the interrupt
    handler.  

************************************************************************/
void DiskWrite(INT32 disk_id, INT32 sector, char * data){
    INT32 Temp;
    /* Do the hardware call to put data on disk */
    int error;
    int pid;

    Get_Process_ID("",&pid,&error);
    if(dq[disk_id]->head == NULL && MyInterruptStatus != disk_id+4){
                lastDiskAct[disk_id] = pid;
        Disk_W(disk_id, sector, data,0);
        addToDiskQueue(disk_id, sector, data, D_WRITE);
    }
    else{
        addToDiskQueue(disk_id, sector, data, D_WRITE);
    }

    Dispatcher(SWITCH_CONTEXT_SAVE_MODE);
}

/***********************************************************************
    Disk_W
    
    This is the routine that actually perform the disk write.  

************************************************************************/
void Disk_W(INT32 disk_id, INT32 sector, char * data, int mark){
    INT32 Temp;
    INT32 error;

    // Add lock
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK,1,TRUE,&LockError);
    if(error == FALSE){
        printf("Error on disk write lock.\n");
        system("pause");
    }

    MEM_WRITE(Z502DiskSetID, &disk_id);
    MEM_WRITE(Z502DiskSetSector, &sector);
    MEM_WRITE(Z502DiskSetBuffer, (INT32 * )data);
    Temp = 1;                        // Specify a write
    MEM_WRITE(Z502DiskSetAction, &Temp);
    Temp = 0;                        // Must be set to 0
    MEM_WRITE(Z502DiskStart, &Temp);
    
    /* Disk should now be started */
    MEM_WRITE(Z502DiskSetID, &disk_id);
    MEM_READ(Z502DiskStatus, &Temp);
    if (Temp == DEVICE_FREE){
        if(mark == 1)  printf("Ocurr in interrupt handler.\n");
        printf("Error:Disk write failed to start.\n");
        printf("last act on disk 2 is pid %d.\n",lastDiskAct[2]);
        printf("Disk id = %d\n",disk_id);
        PrintDiskQueue(disk_id);
        PrintReadyQueue();
        system("pause");
    }

    // Release the lock
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK,0,TRUE,&LockError);
    if(error == FALSE){
        printf("Error on disk write lock.\n");
        system("pause");
    }
    
}


/***********************************************************************
    addToDiskQueue
    
    Add the new request to the corresponding disk queue.  

************************************************************************/
void addToDiskQueue(INT32 disk_id, INT32 sector, char * data, INT32 read_or_write){
    INT32 pid = 0;
    INT32 error = 0;
    PDiskBlock newblock = (PDiskBlock)calloc(1,sizeof(DiskBlock));

    newblock->read_or_write = read_or_write;
    newblock->disk_id = disk_id;
    newblock->sector = sector;
    newblock->data = data;
    newblock->next = NULL;
    
    Get_Process_ID("", &pid, &error);
    newblock->ppcb = PidToPtr(pid);

    newblock->ppcb->ptr_DNode = newblock;
    
    EnqueueDiskRequest(newblock);
}

/***********************************************************************
    EnqueueDiskRequest
    
    It is a sub routine that act enqueue function. It will be invoked 
    by addToDiskQueue().

************************************************************************/
void EnqueueDiskRequest(PDiskBlock newblock){
    int error;

    // Add lock
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,1,TRUE,&error);
    if(error == FALSE){
        printf("Error on disk queue lock.\n");
        system("pause");
    }

    DiskQueue * targetQueue = dq[newblock->disk_id];
    if(targetQueue->head == NULL){
        targetQueue->head = newblock;
        targetQueue->rear = newblock;
    }
    else{
        newblock->next = targetQueue->head;
        targetQueue->head = newblock;
    }
    targetQueue->NumOfDNode++;

    // Release the lock
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,0,TRUE,&error);
    if(error == FALSE){
        printf("Error on disk queue lock.\n");
        system("pause");
    }
}


/***********************************************************************
    GetDiskRequest
    
    Get and remove a node from the rear of the queue.

************************************************************************/
PDiskBlock GetDiskRequest(INT32 disk_id){
    DiskQueue * targetQueue;
    PDiskBlock p;
    int error;

    // Add lock
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,1,TRUE,&error);
    if(error == FALSE){
        printf("Error on disk queue lock.\n");
        system("pause");
    }
    
    targetQueue = dq[disk_id];
    if(targetQueue->head == NULL){
        Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,0,TRUE,&error);
        if(error == FALSE){
            printf("Error on disk queue lock.\n");
            system("pause");
        }
        return NULL;    
    }
    else{
        p = targetQueue->head;
        if(targetQueue->head == targetQueue->rear){
            targetQueue->head = targetQueue->rear = NULL;
            p->ppcb->ptr_DNode = NULL;
            targetQueue->NumOfDNode--;

            // Release the lock
            Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,0,TRUE,&error);
            if(error == FALSE){
                printf("Error on disk queue lock.\n");
                system("pause");
            }
            return p;
        }
        while(p->next != NULL){
            if(p->next == targetQueue->rear){
                targetQueue->rear = p;
                p = p->next;
                p->ppcb->ptr_DNode = NULL;
                targetQueue->rear->next = NULL;
                targetQueue->NumOfDNode--;
                
                // Release the lock
                Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,0,TRUE,&error);
                if(error == FALSE){
                    printf("Error on disk queue lock.\n");
                    system("pause");
                }
                return p;
            }
            p = p->next;
        }
        printf("Error in GetDiskRequest.\n");
    }
    
}


/***********************************************************************
    ReadDiskRequest
    
    Read a node from the rear of the queue. If the queue is empty, it 
    returns NULL.

************************************************************************/
PDiskBlock ReadDiskRequest(INT32 disk_id){
    DiskQueue targetQueue;
    INT32 error;
    
    Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,1,TRUE,&error);
    if(error == FALSE){
        printf("Error on disk queue lock.\n");
        system("pause");
    }
    targetQueue = *(dq[disk_id]);
    if(targetQueue.head == NULL){
        MyInterruptStatus = -1;
        Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,0,TRUE,&error);
        if(error == FALSE){
            printf("Error on disk queue lock.\n");
            system("pause");
        }
        return NULL;    
    }
    else{
        Z502MemoryReadModify(MEMORY_INTERLOCK_DISK_QUEUE,0,TRUE,&error);
        if(error == FALSE){
            printf("Error on disk queue lock.\n");
            system("pause");
        }
        return targetQueue.rear;
    }
}


/***********************************************************************
    NumOfUnfinishedDiskRequest
    
    Return the total number of the node in all disk queue.

************************************************************************/
INT32 NumOfUnfinishedDiskRequest(){
    int count = 0;
    int i;
    
    for(i = 1; i <= MAX_NUMBER_OF_DISKS; i++){
        count += dq[i]->NumOfDNode;

    }
    return count;
}


/***********************************************************************
    PrintDiskQueue
    
    It is a test routine aiming to print out the details in specific disk
    queue.

************************************************************************/
void PrintDiskQueue(INT32 disk_id){
    PDiskBlock p = dq[disk_id]->head;
    printf("Disk queue %d:\n",disk_id);
    while(p != NULL){
        printf("    <pid = %d, RW = %d>\n",p->ppcb->process_id,p->read_or_write);
        p = p->next;
    }
}




