/************************************************************************
	
	base.c	

        This code forms the base of the operating system you will
        build.  It has only the barest rudiments of what you will
        eventually construct; yet it contains the interfaces that
        allow test.c and z502.c to be successfully built together.

        Revision History:
        1.0 August 1990
        1.1 December 1990: Portability attempted.
        1.3 July     1992: More Portability enhancements.
                           Add call to sample_code.
        1.4 December 1992: Limit (temporarily) printout in
                           interrupt handler.  More portability.
        2.0 January  2000: A number of small changes.
        2.1 May      2001: Bug fixes and clear STAT_VECTOR
        2.2 July     2002: Make code appropriate for undergrads.
                           Default program start is in test0.
        3.0 August   2004: Modified to support memory mapped IO
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
	4.0  July    2013: Major portions rewritten to support multiple threads
************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include	     "stdlib.h"
#include	     "process.h"
#include	     "message.h"

//temperary way to deal with undefinition of Z502CONTEXT.
#include        	"z502.h"

// These loacations are global and define information about the page table
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;

//extern tq;
//extern rq;

extern void          *TO_VECTOR [];

char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ",
                            "get_pid  ", "create   ", "term_proc",
                            "suspend  ", "resume   ", "ch_prior ",
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };

	
/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
    INT32			   error = 0;
    static INT32       how_many_interrupt_entries = 0;    /** TEMP **/
    Ptr_PCB 	       next_PCB = NULL;	
	PDiskBlock			   targetblock;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    MyInterruptStatus = device_id;
    
    //printf("interrupt handler arrive with disk_id = %d.\n",device_id);
    //PrintDiskQueue(2);
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
    

	if(device_id != -1){

    	switch(device_id){
			case TIMER_INTERRUPT:
	    		RemoveFromTimerQueue(&next_PCB);
	    		AddToReadyQueue(next_PCB);
	    		break;
	  	
			default:
	    		if(device_id >= DISK_INTERRUPT && device_id <= DISK_INTERRUPT_MAX){
	    			targetblock = GetDiskRequest(DISK_ID(device_id));
	    			AddToReadyQueue(targetblock->ppcb);
	    			
	    			targetblock = ReadDiskRequest(DISK_ID(device_id));
	    			if(targetblock != NULL){
						if(targetblock->read_or_write == D_WRITE){
							lastDiskAct[DISK_ID(device_id)] = targetblock->ppcb->process_id;
							//printf("%d\n",targetblock->ppcb->process_id);
							Disk_W(targetblock->disk_id, targetblock->sector, targetblock->data,1);
						}
						else{
							lastDiskAct[DISK_ID(device_id)] = targetblock->ppcb->process_id;
							Disk_R(targetblock->disk_id, targetblock->sector, targetblock->data,1);
						}
					}
	    		}
	    		else{
	    			printf("Error in interrupt handler. device_id = %d\n",device_id);
	    			system("pause");
	    		}
    	}   
		MEM_WRITE(Z502InterruptClear, &Index );
    	//MEM_READ(Z502InterruptDevice, &device_id );	
		
	}
	MyInterruptStatus = -1;
	//printf("Finishing interrupt_handle with disk_id = %d.\n",device_id);
	
}                                       /* End of interrupt_handler */

/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void    fault_handler( void ){
	INT32       device_id;
    INT32       status;
    INT32       Index = 0;

    //printf("into fault_handler!\n");
    //return;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    //printf( "Fault_handler: Found vector type %d with value %d\n",device_id, status);
    switch(device_id){
    	case CPU_ERROR:
    		Z502Halt();
    		break;
    	case INVALID_MEMORY:
    		PageFaultHandler(Z502_CURRENT_CONTEXT, status);
    		break;
    	case INVALID_PHYSICAL_MEMORY:
    		Z502Halt();
    		break;
    	case PRIVILEGED_INSTRUCTION:
    		printf("Error in fault handler with PRIVILEGED_INSTRUCTION.\n");
    		//Z502Halt();
    		Z502_MODE = KERNEL_MODE;
    		break;
    	default:
    		Z502Halt();
    }
    
    
    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
}                                       /* End of fault_handler */

/************************************************************************
    SVC
        The beginning of the OS502.  Used to receive software interrupts.
        All system calls come to this point in the code and are to be
        handled by the student written code here.
        The variable do_print is designed to print out the data for the
        incoming calls, but does so only for the first ten calls.  This
        allows the user to see what's happening, but doesn't overwhelm
        with the amount of data.
************************************************************************/

void    svc( SYSTEM_CALL_DATA *SystemCallData ) {
    short               call_type;
    static short        do_print = 10;
    short               i;
    INT32		Time;


    call_type = (short)SystemCallData->SystemCallNumber;
    //printf( "SVC handler: %s\n", call_names[call_type]);
    /*printf( "Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
             (unsigned long )SystemCallData->Argument[i],
             (unsigned long )SystemCallData->Argument[i]);
    */
    do_print--;
    
    switch (call_type) {
        // Get time service call
        case SYSNUM_GET_TIME_OF_DAY:   // This value is found in syscalls.h
             CALL(MEM_READ( Z502ClockStatus, &Time ));
            *(INT32 *)SystemCallData->Argument[0] = Time;
            break;
        // terminate system call
        case SYSNUM_TERMINATE_PROCESS:
	    TerminateProcess((INT32)((long)SystemCallData->Argument[0]),
			      	  (INT32 *)SystemCallData->Argument[1]);
            break;
	case SYSNUM_SLEEP:
	    start_timer((long)SystemCallData->Argument[0]);
	    break;
	case SYSNUM_CREATE_PROCESS:
	    CreateProcess((char * )SystemCallData->Argument[0], 
			  (void * )SystemCallData->Argument[1],
			  (INT32)((long)SystemCallData->Argument[2]),
		  	  (INT32 *)SystemCallData->Argument[3],
			  (INT32 *)SystemCallData->Argument[4]);
	    break;
	case SYSNUM_GET_PROCESS_ID:
	    Get_Process_ID((char *)SystemCallData->Argument[0],
			   (INT32 *)SystemCallData->Argument[1],
			   (INT32 *)SystemCallData->Argument[2]);
	    break;
	case SYSNUM_SUSPEND_PROCESS:
	    SuspendProcess((INT32)((long)SystemCallData->Argument[0]),
			   (INT32 *)SystemCallData->Argument[1]);	
	    break;
        case SYSNUM_RESUME_PROCESS:
            ResumeProcess((INT32)((long)SystemCallData->Argument[0]),
                           (INT32 *)SystemCallData->Argument[1]);
            break;
        case SYSNUM_CHANGE_PRIORITY:
            ChangePriority((INT32)((long)SystemCallData->Argument[0]),
                           (INT32)((long)SystemCallData->Argument[1]),
                           (INT32 *)SystemCallData->Argument[2]);
            break;
        case SYSNUM_SEND_MESSAGE:
            SendMessage(   (INT32)((long)SystemCallData->Argument[0]),
                           (char *)((long)SystemCallData->Argument[1]),
                           (INT32)((long)SystemCallData->Argument[2]),
                           (INT32 *)SystemCallData->Argument[3]);
	    break;
        case SYSNUM_RECEIVE_MESSAGE:
            ReceiveMessage((INT32)((long)SystemCallData->Argument[0]),
                           (char *)((long)SystemCallData->Argument[1]),
                           (INT32)((long)SystemCallData->Argument[2]),
			   (INT32 *)SystemCallData->Argument[3],
			   (INT32 *)SystemCallData->Argument[4],
			   (INT32 *)SystemCallData->Argument[5]);
            break;
        case SYSNUM_DISK_READ:
            DiskRead((INT32)((long)SystemCallData->Argument[0]),
                     (INT32)((long)SystemCallData->Argument[1]),
                     (char *)SystemCallData->Argument[2]);
            break;
        case SYSNUM_DISK_WRITE:
            DiskWrite((INT32)((long)SystemCallData->Argument[0]),
                      (INT32)((long)SystemCallData->Argument[1]),
                      (char *)SystemCallData->Argument[2]);
            break;
	default:  
            printf( "ERROR!  call_type not recognized!\n" ); 
            printf( "Call_type is - %i\n", call_type);
    }   

}                                               // End of sssTable[0]



/************************************************************************
    osInit
        This is the first routine called after the simulation begins.  This
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/

void    osInit( int argc, char *argv[]  ) {
    void                *next_context;
    INT32               i;
    INT32		pid = 0;
    INT32		error = NO_ERROR_OCURR;
    CURRENT_RUNNING_PROCESS = 0;


    /*  Initialize the global variables  */
    /*  Initialize the TimerQueue  */
    tq.head = NULL;
    tq.NumOfTNode = 0;

    rq.head = NULL;
    rq.rear = NULL;
    rq.NumOfRNode = 0;
    
    /*  Initialize the ProcessTable  */
    pt.NumOfProcess = 0;
    pt.next = NULL;

    /*  Initialize the Message Queue.    */
    mq.head = NULL;
    mq.rear = NULL;
    mq.NumOfMNode = 0;


	/*  Initialize the Disk Queue.    */
    for(i = 0; i <= MAX_NUMBER_OF_DISKS; i++){
    	dq[i] = (DiskQueue *)calloc(1,sizeof(DiskQueue));
    	lastDiskAct[i] = 0;
    }
    
    MyInterruptStatus = -1;
    
    /*  Initialize the Frame Table.    */
	memset(FrameTable,0,PHYS_MEM_PGS);
	
	/*  Initialize the Page load list.    */
	pageLoadList.head = NULL;
	pageLoadList.rear = NULL;

    /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", argc );
    for ( i = 0; i < argc; i++ )
        printf( " %s", argv[i] );
    printf( "\n" );
    //printf( "Note: Calling with argument 'sample' executes the sample program.\n\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;

    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (( argc > 1 ) && ( strcmp( argv[1], "sample" ) == 0 ) ) {
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
    }                   /* This routine should never return!!           */

    /*  This should be done by a "os_make_process" routine, so that
        test0 runs on a process recognized by the operating system.    */
    
	//we cut these 2 line codes and paste it in function os_CreateProcess.

    /*    Determine which test routines to run.    */
    if(argv[1][4] == '0'){
        CreateProcess("proc-test0", (void *)test0, DEFAULT_PROCESS_PRIORITY, &pid, &error);
        CURRENT_RUNNING_PROCESS = pid;                 
        CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));	
    }    
    else if(argv[1][4] == '1'){
        switch(argv[1][5]){
   
            case 'a':
                CreateProcess("proc-test1a", (void *)test1a, DEFAULT_PROCESS_PRIORITY, &pid, &error);
		CURRENT_RUNNING_PROCESS = pid;	
		CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));		
                break;
   
            case 'b':
		CreateProcess("proc-test1b", (void *)test1b, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;                 
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'c':
                CreateProcess("proc-test1c", (void *)test1c, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;
            
            case 'd':
                CreateProcess("proc-test1d", (void *)test1d, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'e':
                CreateProcess("proc-test1e", (void *)test1e, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'f':
                CreateProcess("proc-test1f", (void *)test1f, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'g':
                CreateProcess("proc-test1g", (void *)test1g, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'h':
                CreateProcess("proc-test1h", (void *)test1h, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'i':
                CreateProcess("proc-test1i", (void *)test1i, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'j':
                CreateProcess("proc-test1j", (void *)test1j, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

            case 'k':
                CreateProcess("proc-test1k", (void *)test1k, DEFAULT_PROCESS_PRIORITY, &pid, &error);
                CURRENT_RUNNING_PROCESS = pid;
                CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
                break;

	    default:
                printf("Error:Unknown test routine: %s.\n",argv[1]);
        }
    }
    else {
		switch(argv[1][5]){
		    case 'a':
	            CreateProcess("proc-test2a", (void *)test2a, DEFAULT_PROCESS_PRIORITY, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	            break;
	        case 'b':
	            CreateProcess("proc-test2b", (void *)test2b, DEFAULT_PROCESS_PRIORITY, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	            break;
	        case 'c':
	            CreateProcess("proc-test2c", (void *)test2c, 1, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	            break;
	        case 'd':
	            CreateProcess("proc-test2d", (void *)test2d, DEFAULT_PROCESS_PRIORITY, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	            break;
	        case 'e':
	            CreateProcess("proc-test2e", (void *)test2e, DEFAULT_PROCESS_PRIORITY, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	        	break;
	        case 'f':
	            CreateProcess("proc-test2f", (void *)test2f, DEFAULT_PROCESS_PRIORITY, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	        	break;
	        case 'g':
	            CreateProcess("proc-test2g", (void *)test2g, DEFAULT_PROCESS_PRIORITY, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            printf("%d\n",(INT32)Z502_MODE);
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	        	break;
	        /*case 'z':
	            CreateProcess("proc-test2z", (void *)test2z, DEFAULT_PROCESS_PRIORITY, &pid, &error);
	            CURRENT_RUNNING_PROCESS = pid;
	            CALL(Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(PidToPtr(pid)->ptr_context) ));
	            break;*/
		}
    }
}                                               // End of osInit


















