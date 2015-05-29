/***********************************************************************
 schedule.c

 Author:                 Yunhe Tang
 Complete Time:          10/18/2013
 
 This functions in this file is to implement the ready queue and
 timer queue. The declearation fof the functions are in process.h

************************************************************************/
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             "stdlib.h"
#include	     	 "process.h"


/***********************************************************************

    Dispatcher
		The only way that a running process actively gives up CPU and 
    	transer control to other ready process. If there is no other process
    	ready, then wait. If there is no other process in ready queue as well
    	as in timer queue, and in KILL MODE, then Halt.

************************************************************************/

void Dispatcher(INT32 save_or_kill){
	Ptr_PCB next_PCB = NULL;
	
	/*    Check if there is interrupt occurs.    */
	CALL(RemoveFromReadyQueue(&next_PCB));

	/*    If no other process exsisted, then shut down    */
	if(next_PCB == NULL && NUM_OF_TNODE == 0 && NumOfUnfinishedDiskRequest() == 0){
		Z502Halt();
	}
	/*    Get the next ready process    */
    while(next_PCB == NULL){
    	CALL(RemoveFromReadyQueue(&next_PCB););
    }
	
	/*    Swithch the context    */
    CURRENT_RUNNING_PROCESS = next_PCB->process_id;		
    CALL(Z502SwitchContext( save_or_kill, &(next_PCB->ptr_context) ));

}


/***********************************************************************

    AddToTimerQueue
		Add the new timer node to the timer queue.

************************************************************************/

void AddToTimerQueue(INT32 add_PID, INT32 TimeUnits){
    Ptr_TNode add_TNode = NULL;
	Ptr_TNode curr_TNode;
	INT32 currtime;
	INT32 waketime;

	/*    Check if the timre queue is empty    */
	if(NUM_OF_TNODE >= SIZE_OF_PROCESS_TABLE){
		printf("Error:Too many TNode on timerqueue.\n");
		return;
	}
	
	/*    Make a timer node    */
	MEM_READ( Z502ClockStatus, &currtime );
	waketime = currtime + TimeUnits;

	add_TNode = malloc(sizeof(TNode));
	add_TNode->next = NULL;
	add_TNode->ppcb = PidToPtr(add_PID);
	add_TNode->waketime = waketime;

	/*    Add the timer node into timer queue    */
	if(NUM_OF_TNODE == 0){
		tq.head = add_TNode;
	}
	else {
		if(tq.head->waketime > waketime){
			add_TNode->next = tq.head;
        	tq.head  	= add_TNode;
		}
		else {
			curr_TNode = tq.head;
            while(curr_TNode->next != NULL){
                if(curr_TNode->next->waketime > waketime){
                    add_TNode->next  = curr_TNode->next;
                    curr_TNode->next = add_TNode;
                    break;
				}
                curr_TNode = curr_TNode->next;
            }
			if(curr_TNode->next == NULL)
                curr_TNode->next = add_TNode;
		}		
	}	
	NUM_OF_TNODE++;
	(PidToPtr(add_PID))->ptr_TNode = add_TNode;        //Add to Timer Queue success.
	
	/*    Fresh the timer register if necessary    */
	FreshTimer();	
}


/***********************************************************************

    RemoveFromTimerQueue
		Remove the indicating timer node from the timer queue.

************************************************************************/

void RemoveFromTimerQueue(Ptr_PCB *  next_PCB){
	Ptr_TNode get_PTN;       
	INT32	TimeUnits;
	INT32   currtime;

	/*    Check if the timer is empty    */
	if(NUM_OF_TNODE == 0){
		printf("Error:Remove from timer queue failed.\n");
		return;
	}
	
	/*    Remove the timer queue    */
	get_PTN = tq.head;
	tq.head = get_PTN->next;
	NUM_OF_TNODE--;
	get_PTN->ppcb->ptr_TNode = NULL;

	*next_PCB = get_PTN->ppcb;	

	free(get_PTN);       //remove successfully.	
	
	/*    Fresh the timer register if necessary    */
	FreshTimer();
}


/***********************************************************************

    AddToReadyQueue
		Add the new ready node to the ready queue.

************************************************************************/

void AddToReadyQueue(Ptr_PCB ppcb){
	Ptr_RNode add_RNode;
	Ptr_RNode curr_RNode;	

   	/*    Lock the ready to guarantee the atom operation to ready queue    */
	Z502MemoryReadModify(MEMORY_INTERLOCK_READY_QUEUE,1,TRUE,&LockError);

	/*    Chenck if there is additional space for new node    */
	if(NUM_OF_RNODE >= SIZE_OF_PROCESS_TABLE-1){
        printf("Error:Too many RNode on ready queue.\n");
    }
	else {
		/*    Make a new node    */
		add_RNode = malloc(sizeof(RNode));
		add_RNode->next = NULL;
		add_RNode->ppcb = ppcb;

		/*    Add to timer queue    */
		if(NUM_OF_RNODE == 0)
			rq.head = add_RNode;
		else{
			curr_RNode = rq.head;
			if(curr_RNode->ppcb->priority > ppcb->priority){
				add_RNode->next = curr_RNode;
				rq.head	       = add_RNode;
			}
			else {
				while(curr_RNode->next != NULL){
					if(curr_RNode->next->ppcb->priority > ppcb->priority){
						add_RNode->next = curr_RNode->next;
						curr_RNode->next = add_RNode;
						break;
					}
					curr_RNode = curr_RNode->next;
				}
				curr_RNode->next = add_RNode;
			}
		}	
		NUM_OF_RNODE++;
	
		ppcb->ptr_RNode = add_RNode;
	}

	/*    Unlock the ready to let other process using    */
	Z502MemoryReadModify(MEMORY_INTERLOCK_READY_QUEUE,0,TRUE,&LockError);
}

/***********************************************************************

    RemoveFromReadyQueue
		Remove the ready node from ready queue.

************************************************************************/

void RemoveFromReadyQueue(Ptr_PCB * next_PPCB){
	Ptr_RNode rmv_RNode;	

   	/*    Lock the ready to guarantee the atom operation to ready queue    */	
	Z502MemoryReadModify(MEMORY_INTERLOCK_READY_QUEUE,1,TRUE,&LockError);
	if(LockError == FALSE){
		printf("Error on Ready queue get lock.\n");
		system("pause");
	}

	/*    Chenck if there is additional space for new node    */
	if(NUM_OF_RNODE == 0){
		*next_PPCB = NULL;
	}

	else {
		/*    Remove from ready queue    */
        *next_PPCB = rq.head->ppcb;
        rmv_RNode = rq.head;
        rq.head = rmv_RNode->next;
        NUM_OF_RNODE--;

        (*next_PPCB)->ptr_RNode = NULL;

        if(NUM_OF_RNODE == 0)
           	rq.rear = NULL;
        free(rmv_RNode);
	
	}

	/*    Unlock the ready to let other process using    */
	Z502MemoryReadModify(MEMORY_INTERLOCK_READY_QUEUE,0,TRUE,&LockError);
	if(LockError == FALSE){
		printf("Error on Ready queue release lock.\n");
		system("pause");
	}

}


/***********************************************************************

    ClearTimerQueue
		Take out indicating timer node from timer queue.

************************************************************************/

void ClearTimerQueue(Ptr_TNode ptr_TNode){
	Ptr_TNode curr_TNode = NULL;
	if(ptr_TNode == NULL){
		return;
	}
	curr_TNode = tq.head;
	if(curr_TNode == NULL){
		return;
	}	
	if(curr_TNode == ptr_TNode){
		tq.head = NULL;
		free(ptr_TNode);
		NUM_OF_TNODE--;
		return;
	}
	do{
		if(curr_TNode->next == ptr_TNode){
			curr_TNode->next = ptr_TNode->next;	
			free(ptr_TNode);
			NUM_OF_TNODE--;
			return;
		}
		curr_TNode = curr_TNode->next;
	}while(curr_TNode != NULL);

	printf("Error:Failed to find the tnode.\n");	
}

/***********************************************************************

    ClearReadyQueue
		Take off indicating ready node from ready queue.

************************************************************************/

void ClearReadyQueue(Ptr_RNode ptr_RNode){
	Ptr_RNode curr_RNode = NULL;
	Z502MemoryReadModify(MEMORY_INTERLOCK_READY_QUEUE,1,TRUE,&LockError);
    curr_RNode = rq.head;
	
	if(ptr_RNode == NULL){}
    else if(curr_RNode == NULL){
        printf("Error:No RNode in ReadyQueue.\n");
    }
    else if(curr_RNode == ptr_RNode){
        rq.head = curr_RNode->next;
        free(ptr_RNode);
        NUM_OF_RNODE--;
    }
	else {
		do{
            if(curr_RNode->next == ptr_RNode){
                curr_RNode->next = ptr_RNode->next;
                free(ptr_RNode);
                NUM_OF_RNODE--;
				break;
            }
            curr_RNode = curr_RNode->next;
        }while(curr_RNode != NULL);
	}

	Z502MemoryReadModify(MEMORY_INTERLOCK_READY_QUEUE,0,TRUE,&LockError);
}


/***********************************************************************

    OrderReadyQueue
		Reorder the ready queue to guarantee the process with higher
    	priority can be scheduled first.

************************************************************************/

void OrderReadyQueue(Ptr_RNode mdf_RNode){
    Ptr_PCB mdf_PCB = mdf_RNode->ppcb;
    ClearReadyQueue(mdf_RNode);
    AddToReadyQueue(mdf_PCB);
}


/***********************************************************************

    FreshTimer
		Whenever the timer queue is changed, this routine is called to
    	check if it is necessary to reset the timer.

************************************************************************/

void FreshTimer(){
	INT32 currtime; 
	INT32 TimeUnits;
	
	if(NUM_OF_TNODE == 0){
        return;
    }
	
	/*    Get the current time    */
    MEM_READ( Z502ClockStatus, &currtime );

	/*    Chenck if reset the timer    */
    if(currtime >= tq.head->waketime){
        TimeUnits = 0;
        MEM_WRITE(Z502TimerStart, &TimeUnits);
    }
    else {
        TimeUnits = tq.head->waketime - currtime;
		MEM_WRITE(Z502TimerStart, &TimeUnits);
    }
}


void PrintReadyQueue(){
	Ptr_RNode p;
	p = rq.head;
	printf("Ready queue:\n");
	while(p != NULL){
		printf("	<pid = %d>\n",p->ppcb->process_id);
		p = p->next;
	}
}





