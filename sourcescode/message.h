/**************************************************************************

	message.h

	Author:                 Yunhe Tang
 	Complete Time:          10/19/2013


**************************************************************************/

#define		MAX_MESSAGE_LENGTH	512
#define		MAX_MESSAGE_BUFFER_SIZE	16

#define		NUM_OF_MNODE		mq.NumOfMNode

typedef struct MNode{
	struct MNode * next;
	INT32	source_PID;
	INT32	target_PID;
	INT32	msg_length;
	char	message[MAX_MESSAGE_LENGTH];
}MNode, * Ptr_MNode;

typedef	struct MessageQueue{
	Ptr_MNode head;
	Ptr_MNode rear;
	INT32	NumOfMNode;
}MessageQueue;


void SendMessage(INT32 , char * , INT32 , INT32 * );
void ReceiveMessage(INT32 , char buffer[] , INT32 , INT32 *, INT32 * ,INT32 *);
void EnqueueMessage(Ptr_MNode );
void RemoveMessage(INT32 ,Ptr_MNode * );

MessageQueue  mq;



