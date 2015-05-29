/**************************************************************************

    message.c

    Author:                 Yunhe Tang
    Complete Time:          10/19/2013
    
    This functions in this file is to implement the message function.
    The declearation fof the functions are in message.h


**************************************************************************/
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             "stdlib.h"
#include             "process.h"
#include             "message.h"




/**************************************************************************

    SendMessage
        Send a message from one process to another. If the target pid equals 
        -1, then broadcast to all process.

**************************************************************************/
void SendMessage(INT32 target_pid, char * message, INT32 send_length, INT32 * error ){
    Ptr_MNode new_MNode = NULL;
    Ptr_PCB curr_PCB = NULL;
    *error = ERR_SUCCESS;

    /*    Check the validation of the inputs    */
    if(!isProcessExistbyPid(target_pid) && target_pid != -1){
        printf("Error:target process not found.\n");
        *error = ERROR_ILLEGAL_MSG_PID;
    }
    else if(send_length > MAX_MESSAGE_LENGTH 
        || send_length < 0){
        printf("Error:Illegal message length.\n");
        *error = ERROR_ILLEGAL_MSG_LENGTH;
    }
    else if(NUM_OF_MNODE >= MAX_MESSAGE_BUFFER_SIZE){
        printf("Error:Message buffer overload.\n");
        *error = ERROR_MSG_BUF_OVERLOAD;
    }
    else {
        /*    make a new message node and add to the message queue    */
        if(target_pid == -1){
            /*    Check if it is a broadcasting message   */
            for(curr_PCB = pt.next; curr_PCB != NULL; curr_PCB = curr_PCB->next){
                if(curr_PCB->process_id == CURRENT_RUNNING_PROCESS){
                    continue;
                }
                else {
                    new_MNode = malloc(sizeof(MNode));
                    new_MNode->next = NULL;
                    new_MNode->source_PID = CURRENT_RUNNING_PROCESS;
                    new_MNode->target_PID = curr_PCB->process_id;
                    new_MNode->msg_length = send_length;
                    strcpy(new_MNode->message,message);
                    EnqueueMessage(new_MNode);  
                }
            
                if(NUM_OF_MNODE >= MAX_MESSAGE_BUFFER_SIZE){
                    printf("Error:Message buffer overload.\n");
                    *error = ERROR_MSG_BUF_OVERLOAD;
                    break;
                }       
            }
        }
        else {
            /*    Normal case    */
            new_MNode = malloc(sizeof(MNode));
            new_MNode->next = NULL;
            new_MNode->source_PID = CURRENT_RUNNING_PROCESS;
            new_MNode->target_PID = target_pid;
            new_MNode->msg_length = send_length;
            strcpy(new_MNode->message,message); 
            EnqueueMessage(new_MNode);
        }
    }
}


/**************************************************************************

    ReceiveMessage
        Receive a message from indicating source process. If there is no message
        found in message queue, then return NULL(later this will be modified as block).
        If the source pid is -1, then get one message from any process.

**************************************************************************/

void ReceiveMessage(INT32 source_pid, char msg_buffer[], INT32 receive_length, INT32 * actual_send_length, INT32 * actual_source_pid, INT32 * error){
    Ptr_MNode target_MNode = NULL;
        
    *error = ERR_SUCCESS;
    /*    Check the validation of the inputs    */
    if((source_pid > SIZE_OF_PROCESS_TABLE 
        || source_pid < 1) && source_pid != -1){
        printf("Error:Illegal source pid.\n");
        *error = ERROR_ILLEGAL_MSG_PID;
    }
    else if(receive_length > MAX_MESSAGE_LENGTH
            || receive_length < 0){
        printf("Error:Illegal message length.\n");
        *error = ERROR_ILLEGAL_MSG_LENGTH;
    }
    else if(NUM_OF_MNODE == 0){}
    else {
        /*    Check if it want to receive from any source process    */
        if(source_pid == -1){
            RemoveMessage(mq.head->source_PID, &target_MNode);
        }
        else {
            RemoveMessage(source_pid, &target_MNode);   
        }
    }
    
    if(target_MNode != NULL){
        /*    found the nessage and assigned to the giving address    */
        if(receive_length < target_MNode->msg_length){
            printf("Error:Receive buffer too small.\n");
            *error = ERROR_MSG_RCV_BUF_ILLEGAL;
        }
        else {
            strcpy(msg_buffer,target_MNode->message);
            *actual_send_length = target_MNode->msg_length;
            *actual_source_pid = target_MNode->source_PID;
            printf("actual source pid:%d\n",*actual_source_pid);
        }
        free(target_MNode);
    }
}


/**************************************************************************

    EnqueueMessage
        Enqueue a message node in the message queue.

**************************************************************************/

void EnqueueMessage(Ptr_MNode new_MNode){
    
    if(NUM_OF_MNODE == 0){
        mq.head = new_MNode;
        mq.rear = new_MNode;
        NUM_OF_MNODE++;
    }
    else {
        mq.rear->next = new_MNode;
        mq.rear = new_MNode;
        NUM_OF_MNODE++;
    }

}


/**************************************************************************

    RemoveMessage
        Take off a message node from the message queue.

**************************************************************************/

void RemoveMessage(INT32 source_pid, Ptr_MNode * target_PMNode){
    Ptr_MNode curr_MNode = NULL;

    curr_MNode = mq.head;
    if(curr_MNode == NULL){}
    else if(curr_MNode->source_PID == source_pid){
        *target_PMNode = curr_MNode;
        mq.head = curr_MNode->next;
        if(mq.head == NULL){
            mq.rear = NULL;
        }
        NUM_OF_MNODE--;
    }
    else {
        while(curr_MNode->next != NULL){
            if(curr_MNode->next->source_PID == source_pid){
                *target_PMNode = curr_MNode->next;
                curr_MNode->next = curr_MNode->next->next;
                if(curr_MNode->next == NULL){
                    mq.rear = curr_MNode;
                }
                NUM_OF_MNODE--;
                break;
            }
            curr_MNode = curr_MNode->next;
        }
    }
}



