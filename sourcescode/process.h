/***********************************************************************

 process.h

 Author:                 Yunhe Tang
 Complete Time:          10/19/2013

************************************************************************/
#include             "z502.h"


/*  Definition for System variables  */
#define         NUM_OF_TNODE                tq.NumOfTNode
#define         NUM_OF_RNODE                rq.NumOfRNode

#define         ERROR_TQ_RMV_FAILED             -1
#define         CURRENT_RUNNING_PROCESS         crp
#define         NUM_OF_PROCESS                  pt.NumOfProcess
#define         SIZE_OF_PROCESS_TABLE           10
#define         PCB_NOT_FOUND_ERROR             -1
#define         PCB_OFF_QUEUE_SUCCEED           1
#define         DEFAULT_PROCESS_PRIORITY        0
#define         NO_ERROR_OCURR                  0
#define         MAXLEN_OF_PROCESS_NAME          256
#define         MAXVALUE_OF_PROCESS_PRIORITY    100
#define         MINVALUE_OF_PROCESS_PRIORITY    0

#define		    PROCESS_NON_SUSPEND_STATE		1
#define   	    PROCESS_SUSPEND_STATE			2

/*  Error code for CreateProcess  */
/*  Error code 0 has been used (ERR_SUCCESS, which means no error ocurres.)  */
#define         ERROR_PROCESS_NAME_ILLEGAL          1
#define         ERROR_PROCESS_PRIORITY_ILLEGAL      3
#define         ERROR_PROCESS_ALREADY_EXIST         4
#define         ERROR_PROCESS_NOT_EXIST             5
#define         ERROR_PROCESS_OVERLOAD              -1
#define         ERROR_GET_PID_FAILED                6
#define 	    ERROR_PID_SUSPEND_ILLEGAL	        7
#define		    ERROR_PID_RESUME_ILLEGAL	        8
#define		    ERROR_PROCESS_ALREADY_SUSPEND	    9
#define		    ERROR_PROCESS_ALREADY_RESUME	    10
#define		    ERROR_RESUME_NON_SUSPEND_PROCESS	11
#define		    ERROR_RESUME_TIMER_BLOCK_PROCESS	12
#define         ERROR_CHANGE_PRIORITY_PID_ILLEGAL   13
#define         ERROR_CHANGE_PRIORITY_ILLEGAL       14
#define		    ERROR_ILLEGAL_MSG_PID			    15	
#define	        ERROR_ILLEGAL_MSG_LENGTH		    16
#define		    ERROR_MSG_BUF_OVERLOAD			    17
#define		    ERROR_MSG_RCV_BUF_ILLEGAL		    18

typedef INT32           BOOL;

struct TNode;
struct RNode;
struct DiskBlock;
struct PageLoadList;

/*  Definition PCB  */
typedef struct pcb1{
    struct pcb1 *   	next;
    struct pcb1 *   	pre;
    char *         	    process_name;
    void *         	    starting_address;
    INT32               priority;
    INT32               process_id;
    INT32               parent_pid;
	INT32		        state;
    INT32               error;
    struct TNode *      ptr_TNode;
    struct RNode *      ptr_RNode;
    struct DiskBlock *	ptr_DNode;
    void *              ptr_context;
}PCB, * Ptr_PCB;


/*  Define ProcessTable */
typedef struct {
	INT32               NumOfProcess;
    Ptr_PCB             next;
}ProcessTable;
              


/*  Define Timer Queue Node  */
typedef struct TNode{
    struct TNode *      next;
    Ptr_PCB             ppcb;
    INT32               waketime;
}TNode, * Ptr_TNode;

/*  Define TimerQueue  */
typedef struct {
    Ptr_TNode           head;
    INT32               NumOfTNode;
}TimerQueue;

/*  Define ReadyQueue  */
typedef struct RNode{
    struct RNode *      next;
    Ptr_PCB             ppcb;
}RNode, * Ptr_RNode;

typedef struct {
    Ptr_RNode           head;
    Ptr_RNode           rear;
    INT32               NumOfRNode;
}ReadyQueue;





/*  Prototype of functions  */
void    start_timer(INT32 );
void    CreateProcess(char * , void * , INT32 , INT32 * ,INT32 * );
BOOL    isProcessExistbyName(char * );
BOOL    isProcessExistbyPid(INT32 );
Ptr_PCB getTailOfProcessTable();
INT32   getMinAvaliablePid();
void    AddToProcessTable(Ptr_PCB );
void    RemoveFromProcessTable(Ptr_PCB);
void    TerminateProcess(INT32 , INT32 * );
Ptr_PCB PidToPtr(INT32 );
void    Get_Process_ID(char * , INT32 * , INT32 *);
void    ChangePriority(INT32 , INT32 , INT32 *);
void 	Dispatcher(INT32);
void 	AddToTimerQueue(INT32 , INT32 );
void 	RemoveFromTimerQueue(Ptr_PCB *);
void 	AddToReadyQueue(Ptr_PCB );
void 	RemoveFromReadyQueue(Ptr_PCB * );
void    ClearTimerQueue(Ptr_TNode );
void    ClearReadyQueue(Ptr_RNode );
void    OrderReadyQueue(Ptr_RNode );
void    FreshTimer();
void 	PrintReadyQueue();


//Global virables
ProcessTable    pt;
INT32           crp;
TimerQueue      tq;
ReadyQueue      rq;



INT32 LockError;

extern Z502CONTEXT * Z502_CURRENT_CONTEXT;


/***************************************************************************

  mem.h

****************************************************************************/



/*  Define Disk Queue Node  */
typedef struct DiskBlock{
    struct DiskBlock *   next;
    Ptr_PCB         	 ppcb;
    INT32                read_or_write;
	INT16                disk_id;
	INT16                sector;
	char *               data;
}DiskBlock, * PDiskBlock;


typedef struct DiskQueue{
	INT32	             NumOfDNode;
	PDiskBlock           head;
	PDiskBlock           rear;
}DiskQueue;


typedef struct PLNode{
	INT32	             frameNum;		
	struct               PLNode * next;
}PLNode, * ptr_PLNode;


typedef struct PageLoadList{
	ptr_PLNode 	head;
	ptr_PLNode 	rear;
}PageLoadList;


INT32	FrameTable[PHYS_MEM_PGS];

PageLoadList pageLoadList;

DiskQueue *	dq[MAX_NUMBER_OF_DISKS+1];

INT32	MyInterruptStatus;

INT32	lastDiskAct[13];

extern char MEMORY[PHYS_MEM_PGS * PGSIZE ];

extern INT16 Z502_MODE;

void PageFaultHandler(Z502CONTEXT* curr_context, INT32 VirtualPageNumber);
INT32 SwapPage(INT32 pid, INT32 VirtualPageNumbe);
INT32 GetAvaliableFrame();
void AddToPageLoadList(INT32 frameNum);
void RemoveFromPageLoadList(ptr_PLNode rmv_PLNode);
void DiskRead(INT32 disk_id, INT32 sector, char * data);
void Disk_R(INT32 disk_id, INT32 sector, char * data,int mark);
void DiskWrite(INT32 disk_id, INT32 sector, char * data);
void Disk_W(INT32 disk_id, INT32 sector, char * data,int mark);
void addToDiskQueue(INT32 disk_id, INT32 sector, char * data, INT32 read_or_write);
void EnqueueDiskRequest(PDiskBlock newblock);
PDiskBlock GetDiskRequest(INT32 disk_id);
PDiskBlock ReadDiskRequest(INT32 disk_id);
INT32 NumOfUnfinishedDiskRequest();
void PrintDiskQueue(INT32 disk_id);



