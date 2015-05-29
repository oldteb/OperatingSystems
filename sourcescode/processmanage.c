/************************************************************************

    processmanage.c	

	Author:                 Yunhe Tang
 	Complete Time:          10/18/2013

	This functions in this file is to implement the process managing
	function. The declearation fof the functions are in process.h

************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             "stdlib.h"
#include	     	 "process.h"


/***********************************************************************
    start_timer
		This is the system service routine(a kernel program) that starts 
		the timer and block itself for particular time units till the 
		interrupt occures.

************************************************************************/

void start_timer(INT32 TimeUnits){
	INT32 		Status;
	INT32		curr_PID;
	INT32		error;
	INT32		waketime;

	CALL(MEM_READ(Z502TimerStatus, &Status);)
	MEM_READ( Z502ClockStatus, &waketime );
	waketime+=TimeUnits;

	//enqueues the PCB of the running process onto the timer_queue.
	Get_Process_ID("",&curr_PID,&error);
	AddToTimerQueue(curr_PID,TimeUnits);

	Dispatcher(SWITCH_CONTEXT_SAVE_MODE);
}

/************************************************************************
    CreateProcess
		This routine creates a new process. it prepares kernel data for a 
		new creating process. It will check the	validation of the assigned
		process value, create PCB, mount on the process table. Then run 
		the new process. 
	
************************************************************************/

void CreateProcess(   char * process_name, 
	void * 	starting_address, 
	INT32 	initial_priority,
	INT32 * 	process_id,
	INT32 * 	error){
	void * 		next_context = NULL;
	Ptr_PCB		new_PCB = NULL;
	char *		pname = NULL;
	*error = ERR_SUCCESS;

	//Check the validation of the input value.
	//check the process_name.
	if(strlen(process_name) >= MAXLEN_OF_PROCESS_NAME){
		*error = ERROR_PROCESS_NAME_ILLEGAL;	
		printf("Error:Illegal process name.(The max length of process name is %d\n)",MAXLEN_OF_PROCESS_NAME);
	}
	if(isProcessExistbyName((char *)process_name)){
		*error = ERROR_PROCESS_ALREADY_EXIST;
		printf("Error:Process with assigned name already existed.\n");
	}
	
	/*    Check the initial priority.    */
	if(initial_priority > MAXVALUE_OF_PROCESS_PRIORITY 
	     	|| initial_priority < MINVALUE_OF_PROCESS_PRIORITY ){
		*error = ERROR_PROCESS_PRIORITY_ILLEGAL;
		printf("Error:The initial process priority is illegal. (valid range of priority is between %d and %d.)\n",MINVALUE_OF_PROCESS_PRIORITY,MAXVALUE_OF_PROCESS_PRIORITY);	
	}
	
	/*    All input values are valid.  */
	/*    Check if the process is overload.    */
	if(NUM_OF_PROCESS >= SIZE_OF_PROCESS_TABLE){
		*error = ERROR_PROCESS_OVERLOAD;
		printf("Error:Too much processes running! (Linux can run at most %d processes in the same time.\n)",SIZE_OF_PROCESS_TABLE);
	}

	if(*error == 0){
        /*    Initialize the PCB    */
        new_PCB = (Ptr_PCB)calloc(1,sizeof(PCB));

        /*    Initialize process_name.    */
        pname = (char *)malloc(strlen(process_name)+1);
        strcpy(pname,process_name);
        new_PCB->process_name = pname;

		Z502MakeContext( &new_PCB->ptr_context, starting_address, USER_MODE );
		/*		Initialize page table		*/
		UINT16 * pagetable = (UINT16 *)calloc(VIRTUAL_MEM_PGS,sizeof(UINT16));
		((Z502CONTEXT*)(new_PCB->ptr_context))->page_table_ptr = pagetable;
		((Z502CONTEXT*)(new_PCB->ptr_context))->page_table_len = VIRTUAL_MEM_PGS;

		/*    Initialize process_name.    */
		/*    Initialize other domains.    */
		new_PCB->next = NULL;
		new_PCB->pre  = NULL;
        	new_PCB->starting_address = starting_address;
        	new_PCB->priority = initial_priority;
        	new_PCB->process_id = getMinAvaliablePid();
		new_PCB->parent_pid = CURRENT_RUNNING_PROCESS;
		new_PCB->state = PROCESS_NON_SUSPEND_STATE;
		new_PCB->ptr_TNode = NULL;
		new_PCB->ptr_RNode = NULL;	
		new_PCB->ptr_DNode = NULL;
        AddToProcessTable(new_PCB);
        *process_id = new_PCB->process_id;      //End of PCB initializing.

		if(CURRENT_RUNNING_PROCESS != 0){
			AddToReadyQueue(new_PCB);
		}
    }       //End of PCB creation.

}



/************************************************************************
    TerminateProcess
        Check if is there a process that have the same name with the new 
        process. If it does, return FAULS. Otherwise, return TRUE. 

************************************************************************/
void TerminateProcess(INT32 free_PID, INT32 * error){
	Ptr_PCB free_PCB = NULL;
	Ptr_PCB pPCB = NULL;
	Z502CONTEXT* pre_context = NULL;
	INT32 ppid = 0;

	//Set the error code ERR_SUCCESS
	*error = ERR_SUCCESS;	

	if(free_PID == -1){
		TerminateProcess(CURRENT_RUNNING_PROCESS,error);
		return;
	}
	
	if(free_PID == -2){
		printf("1\n");
		Z502Halt();
		return;
	}
	
	/*    Check the validation of pid.    */
	if(!isProcessExistbyPid(free_PID)){
		*error = ERROR_PROCESS_NOT_EXIST;
		printf("Error:Process with identified Pid %d does not exist.\n",free_PID);
		return;
	}

	/*    Find target PCB    */
	free_PCB = PidToPtr(free_PID);

	/*    Remove PCB from table    */
    RemoveFromProcessTable(free_PCB);
       
	/*    Deconstruct content of PCB    */
	/*    Free PCB    */
	ClearTimerQueue(free_PCB->ptr_TNode);
	ClearReadyQueue(free_PCB->ptr_RNode);
	
	free(free_PCB->process_name);
    free(free_PCB);

	NUM_OF_PROCESS--;

	if(free_PID == CURRENT_RUNNING_PROCESS){
		Dispatcher(SWITCH_CONTEXT_KILL_MODE);
    }
}


/************************************************************************
    PidToPtr
        Return the addtress of the corresponding PCB. 

************************************************************************/

Ptr_PCB PidToPtr(INT32 Pid){
	Ptr_PCB curr_PCB = pt.next;
        INT32   cmp = 0;

	if(Pid == 0){
		return; 
	}

    if(pt.next != NULL){
      	do{
            if(curr_PCB->process_id == Pid)
                return curr_PCB;
            curr_PCB = curr_PCB->next;
        }
       	while(curr_PCB != NULL);
	}
	else {
		printf("Error:Process with identified Pid %d does not exist.\nYou should never see this error.\n",Pid);
		return NULL;
	}
}

/************************************************************************
    isProcessExistbyName
        Check if is there a process that have the same name with the new 
		process. If it does, return FAULS. Otherwise, return TRUE. 

************************************************************************/

BOOL isProcessExistbyName(char * pname){
	Ptr_PCB curr_PCB = pt.next;
	INT32	cmp = 0;
	if(pt.next == NULL)
		return FALSE;

	do{
		if((cmp = strcmp(curr_PCB->process_name, pname)) == 0)
			return TRUE;
		curr_PCB = curr_PCB->next;
	}
	while(curr_PCB != NULL);
	
	return FALSE;
}

/************************************************************************
    isProcessExistbyPid
        Check if is there a process that have the same Pid with the new 
        process. If it does, return FAULS. Otherwise, return TRUE. 

************************************************************************/

BOOL isProcessExistbyPid(INT32 Pid){
    Ptr_PCB curr_PCB = pt.next;
    INT32   cmp = 0;
    if(pt.next == NULL)
        return FALSE;
    do{
        if(curr_PCB->process_id == Pid)
            return TRUE;
        urr_PCB = curr_PCB->next;
    }
    while(curr_PCB != NULL);

    return FALSE;
}


/************************************************************************
    getTailOfProcessTable
    	Find the last exist PCB in process table, and return its address.
		If there is no PCB in process table, then return NULL.     

************************************************************************/

Ptr_PCB getTailOfProcessTable(){
	Ptr_PCB curr_PCB = pt.next;
	if(curr_PCB == NULL)
		return NULL;
	while(curr_PCB->next != NULL) 
		curr_PCB = curr_PCB->next;
	return curr_PCB;
}


/************************************************************************
    getMinAvaliablePid
        Return the minimum avaliable process id. Since the process table  
		is in pid increasing order, we simply find the minimum pid from 
		the begining of process table. If there is no avaliable pid, which
		means the process is overload, return ERROR_PROCESS_OVERLOAD.  
	
************************************************************************/

INT32 getMinAvaliablePid(){
	INT32 MinPid;
	Ptr_PCB curr_PCB = pt.next;
	
	/*    Check if the table is empty    */
	if(pt.next == NULL)
		MinPid = 1;
	else {
        while(curr_PCB != NULL){
            if(curr_PCB->next != NULL){
                if(curr_PCB->process_id == curr_PCB->next->process_id-1){
                    curr_PCB = curr_PCB->next;
                    continue;
                }
            }
            MinPid = curr_PCB->process_id+1;
			break;
        }
	}

	/*    Check if the pid is valid    */
	if(MinPid > SIZE_OF_PROCESS_TABLE){
		printf("Error:Too much processes running! (Linux can run at most %d processes in the same time.\n)",SIZE_OF_PROCESS_TABLE);
		MinPid = ERROR_PROCESS_OVERLOAD;	
	}

	return MinPid;
}

/************************************************************************
    AddToProcessTable
        Add a PCB block into the process table. The input is a pointer 
    	to the existing PCB block.

************************************************************************/

void AddToProcessTable(Ptr_PCB add_PCB){
	INT32 	counter;
	INT32	key = add_PCB->process_id;
	Ptr_PCB	curr_PCB = pt.next;

	/*    Check if the table is empty    */
	if(pt.next == NULL){
		pt.next = add_PCB;
		add_PCB->pre  = NULL;
		add_PCB->next = NULL;
		NUM_OF_PROCESS++;			
		return;
	}
	
	/*    Normal case.    */
	do{
		if(curr_PCB->process_id > key){
			/*    Insert as the first node in table    */
			if(curr_PCB->pre == NULL){
				add_PCB->next    = curr_PCB;
                add_PCB->pre     = NULL;
                pt.next          = add_PCB;
                curr_PCB->pre    = add_PCB;
                NUM_OF_PROCESS++;
                return;
			}
			else {
				/*    Normal case    */
                add_PCB->next      = curr_PCB;
                add_PCB->pre       = curr_PCB->pre;
                curr_PCB->pre      = add_PCB;
                add_PCB->pre->next = add_PCB;
                NUM_OF_PROCESS++;
                return;
			}
		}
		curr_PCB = curr_PCB->next;
	}while(curr_PCB != NULL);
	
	/*    Check if the process table is overload.    */
	if(NUM_OF_PROCESS == SIZE_OF_PROCESS_TABLE){
		printf("This should not be happend!\n");
	}
	
	curr_PCB = getTailOfProcessTable();
	add_PCB->next	= NULL;
	add_PCB->pre	= curr_PCB;
	curr_PCB->next  = add_PCB;
	NUM_OF_PROCESS++;

	/*    Exception occurs.    */
}


/************************************************************************
    RemoveFromProcessTable
        Remove the target PCB block in the process table. If there is no 
    	such PCB in the process table, then do nothing.

************************************************************************/

void RemoveFromProcessTable(Ptr_PCB rmv_PCB){

	/*    Check if the table has only one node    */
    if(rmv_PCB->pre == NULL){
		pt.next = rmv_PCB->next;
		if(rmv_PCB->next != NULL)
			rmv_PCB->next->pre = NULL;
	}
    else {
		/*    Normal case    */
		rmv_PCB->pre->next = rmv_PCB->next;
		if(rmv_PCB->next != NULL)
			rmv_PCB->next->pre = rmv_PCB->pre;
	}
}


/************************************************************************
    Get_Process_ID
        Get the process id of indicating process, input the name of the 
    	process, put the pid into the space that process_id pointed to.

************************************************************************/

void Get_Process_ID(char * process_name, INT32 * process_id, INT32 * error){
	*error = ERR_SUCCESS;
	
	/*    Normal case    */
	if(strlen(process_name) != 0){
       	Ptr_PCB curr_PCB = pt.next;
        INT32   cmp = 0;
		/*    Check if the table is empty    */
        if(pt.next == NULL){
			*error =  ERROR_GET_PID_FAILED;
			printf("Error:Process with assigned process name not found.\n");
			*process_id = -1;
        	return;
		}

        do{
			/*    Normal case    */
            if((cmp = strcmp(curr_PCB->process_name, process_name)) == 0){
				*process_id = curr_PCB->process_id;
				return;
			}
            curr_PCB = curr_PCB->next;
       	}
        while(curr_PCB != NULL);
		
		/*    Check if get pid is failed    */
		*error = ERROR_GET_PID_FAILED;
		printf("Error:Process with assigned process name not found.\n");
		*process_id = -1;
        return;
	}
	else {
		/*    Check if a current running process id should be return    */
		*process_id = CURRENT_RUNNING_PROCESS;
	}
}


/************************************************************************
    SuspendProcess
        Suspend the indicating process, if the process is in the ready 
    	queue, then remove it from the ready queue right away. A suspended 
    	process will not be scheduled only after it has been waked up.

************************************************************************/

void SuspendProcess(INT32 process_id, INT32 * error){
	Ptr_PCB sus_PCB;
	*error = ERR_SUCCESS;	

	/*    Check the validation of inputs    */
	if(process_id == -1)
		process_id = CURRENT_RUNNING_PROCESS;
	if(!isProcessExistbyPid(process_id)){
		printf("Error:Invalid Pid when suspend process.\n");
		*error = ERROR_PID_SUSPEND_ILLEGAL;
		return;
	}

	/*    Suspend the process    */
	/*    Check if the state of the process is suspend    */
	sus_PCB = PidToPtr(process_id);
	if(sus_PCB->state == PROCESS_SUSPEND_STATE){
		printf("Error:Suspend already suspend process.\n");
		*error = ERROR_PROCESS_ALREADY_SUSPEND;
	}
	/*    Check if the state of process is non-suspend    */
	if(sus_PCB->state == PROCESS_NON_SUSPEND_STATE
		&& sus_PCB->process_id == CURRENT_RUNNING_PROCESS){
		sus_PCB->state = PROCESS_SUSPEND_STATE;
		Dispatcher(SWITCH_CONTEXT_SAVE_MODE);
		return;
	}
	else if(
		sus_PCB->state == PROCESS_NON_SUSPEND_STATE){
   		ClearReadyQueue(sus_PCB->ptr_RNode);
		sus_PCB->state = PROCESS_SUSPEND_STATE;
    }
}


/************************************************************************
    ResumeProcess
        Wake up the indicating process. If the process is not in a suspended 
    	state, then do nothing. Otherwise, wake up the process and put it into 
    	the ready queue.

************************************************************************/

void ResumeProcess(INT32 process_id, INT32 * error){
	Ptr_PCB rsm_PCB;
	*error = ERR_SUCCESS;
	/*    Check the validation of inputs    */
        if(!isProcessExistbyPid(process_id)){
                printf("Error:Invalid Pid when suspend process.\n");
                *error = ERROR_PID_SUSPEND_ILLEGAL;
                return;
        }

	/*    Resume the indicating process    */
	rsm_PCB = PidToPtr(process_id);
	if(rsm_PCB->ptr_TNode != NULL){
		printf("Error:Resume timer blocked process.\n");
		*error = ERROR_RESUME_TIMER_BLOCK_PROCESS;
		return;
	}
	if(rsm_PCB->state == PROCESS_NON_SUSPEND_STATE){
		printf("Error:Resume non-suspend process.\n");
		*error = ERROR_RESUME_NON_SUSPEND_PROCESS;
		return;
	}
	if(rsm_PCB->state == PROCESS_SUSPEND_STATE){
		AddToReadyQueue(rsm_PCB);	
		rsm_PCB->state = PROCESS_NON_SUSPEND_STATE;	
	}	
}

/************************************************************************
    ChangePriority
        Change the priority of the indicating process, if the process is 
    	currently in the ready queue, adjust its position to where it should 
    	be to guarantee the process with higher rpiority can be scheduled first.

************************************************************************/

void ChangePriority(INT32 process_id, INT32 new_priority, INT32 *error){
    Ptr_PCB chg_PCB = NULL;
    *error = ERR_SUCCESS;
	/*    Check the validation of inputs    */
    if(process_id == -1){
        ChangePriority(CURRENT_RUNNING_PROCESS,new_priority,error);
        return;
    }
    if(!isProcessExistbyPid(process_id)){
        printf("Error:Invalid Pid when change process priority.\n");
        *error = ERROR_CHANGE_PRIORITY_PID_ILLEGAL;
        return;
    }
    if(new_priority > MAXVALUE_OF_PROCESS_PRIORITY ||
        new_priority < MINVALUE_OF_PROCESS_PRIORITY){

        printf("Error:Illegal new priority.\n");
        *error = ERROR_CHANGE_PRIORITY_ILLEGAL;
        return;
    }

	/*    Change the priority of the process and reorder the ready queue    */
    chg_PCB = PidToPtr(process_id);
    chg_PCB->priority = new_priority;
    if(chg_PCB->ptr_RNode != NULL){
        OrderReadyQueue(chg_PCB->ptr_RNode);
    }
}







