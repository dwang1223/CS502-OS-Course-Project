/************************************************************************

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

		Oct. 2013 Implemeted by Hao        hzhou@wpi.edu
************************************************************************/

#include			 "malloc.h"
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             "stdlib.h"
#include             "custom.h"

//#include             "z502.h"
#define         ILLEGAL_PRIORITY                -3
#define         NAME_DUPLICATED					-4

void schedule_printer();
void ready_queue_print();
void timer_queue_print();
void suspend_queue_print();
void total_queue_print();
void current_statue_print();
int start_timer(long *);
int dispatcher();
long get_pid_by_name(char *);
PCB * PCB_item_generator(SYSTEM_CALL_DATA *);
void new_node_add_to_readyQueue(Queue , int );
long process_creater(PCB *);
void myself_teminator( );
int process_teminator_by_pid(long );
int suspend_by_PID(long );
int resume_by_PID(long );
int priority_changer(long , int );
int msg_sender(long , long , char *, int );
int msg_receiver(long , char *, int , long *, long *);
void interrupt_handler( void );
void fault_handler( void );
void svc( SYSTEM_CALL_DATA * );
void osInit( int , char **  );

// These loacations are global and define information about the page table
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;
extern void          *TO_VECTOR [];
char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ",
                            "get_pid  ", "create   ", "term_proc",
                            "suspend  ", "resume   ", "ch_prior ",
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };
Queue totalQueue;
Queue timerQueue;
Queue readyQueue;
Queue suspendQueue;
MsgQueue msgQueue;
static long increamentPID = 1; //store the maximum pid for all process
PCB *pcb;
static int currentCountOfProcess = 0;
static PCB *currentPCBNode;
INT32 LockResult,LockResult2,LockResultPrinter;
int globalAddType = ADD_BY_PRIOR; //ADD_BY_END | ADD_BY_PRIOR
char action[SP_LENGTH_OF_ACTION];
INT32 currentTime = 0;

void schedule_printer()
{
	Queue queueCursor;
	int count = 0;
	
	READ_MODIFY(MEMORY_INTERLOCK_BASE+3, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResultPrinter);
	printf("\n");
	SP_print_header();
	CALL(MEM_READ( Z502ClockStatus, &currentTime ));
	SP_setup( SP_TIME_MODE, (long)currentTime );
	if(readyQueue != NULL && readyQueue->next != NULL)
	{
		SP_setup( SP_TARGET_MODE, readyQueue->next->node->pid );
	}
	else
	{
		SP_setup( SP_TARGET_MODE, currentPCBNode->pid );
	}
	//strncpy(action,"Schedule",8);
	SP_setup_action( SP_ACTION_MODE, action );
	SP_setup( SP_RUNNING_MODE, currentPCBNode->pid );

	queueCursor = readyQueue;
	while(queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup( SP_READY_MODE, queueCursor->node->pid );
		if(count >= 10)
		{
			count = 0;
			break;
		}
	}

	queueCursor = timerQueue;
	while(queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup( SP_WAITING_MODE, queueCursor->node->pid );
		if(count >= 10)
		{
			count = 0;
			break;
		}
	}

	queueCursor = suspendQueue;
	while(queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup( SP_SUSPENDED_MODE, queueCursor->node->pid );
		if(count >= 10)
		{
			count = 0;
			break;
		}
	}
	CALL(SP_print_line());
	memset(action,'\0',8);
	printf("\n");
	READ_MODIFY(MEMORY_INTERLOCK_BASE+3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResultPrinter);
}
void ready_queue_print()
{
	Queue readyQueueCursor = readyQueue;
	printf("\nreadyQueue:\t");
	while(readyQueueCursor->next != NULL)
	{
		readyQueueCursor = readyQueueCursor->next;
		printf("%ld(%d)  ",readyQueueCursor->node->pid, readyQueueCursor->node->prior);
	}
	printf("\n");
}
void timer_queue_print()
{
	//get current absolute time, and set wakeUpTime attribute for currentPCBNode
	Queue timerQueueCursor = timerQueue;
	READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult2);
	CALL(MEM_READ( Z502ClockStatus, &currentTime ));
	printf("timerQueue(%d):\t",currentTime);
	//printf("\ntimerQueue:\t");
	while(timerQueueCursor->next != NULL)
	{
		timerQueueCursor = timerQueueCursor->next;
		printf("%ld(%d)  ",timerQueueCursor->node->pid, timerQueueCursor->node->wakeUpTime);
	}
	printf("\n");
	READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult2);
}
void suspend_queue_print()
{
	Queue suspendQueueCursor = suspendQueue;
	printf("suspendQueue:\t");
	while(suspendQueueCursor->next != NULL)
	{
		suspendQueueCursor = suspendQueueCursor->next;
		printf("%ld  ",suspendQueueCursor->node->pid);
	}
	printf("\n");
}
void total_queue_print()
{
	Queue totalQueueCursor = totalQueue;
	printf("\ntotalQueue:\t");
	while(totalQueueCursor->next != NULL)
	{
		totalQueueCursor = totalQueueCursor->next;
		printf("%ld  ",totalQueueCursor->node->pid);
	}
	printf("\n");
}
void current_statue_print()
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	ready_queue_print();
	suspend_queue_print();
	timer_queue_print();
	if(currentPCBNode != NULL)
	{
		printf("Running node = %ld\n\n",currentPCBNode->pid);
	}
	else
	{
		printf("Running node is NULL\n\n");
	}
	
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
}
int start_timer(long *sleep_time)
{
	//INT32 currentTime;
	long _wakeUpTime;
	Queue timerQueueCursor,preTimerQueueCursor, nodeTmp;
	//current_statue_print(); 
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	//get current absolute time, and set wakeUpTime attribute for currentPCBNode
	CALL(MEM_READ( Z502ClockStatus, &currentTime ));
	_wakeUpTime = currentTime + (INT32)*sleep_time;
	currentPCBNode->wakeUpTime = _wakeUpTime;
	
	nodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	nodeTmp->node = currentPCBNode;
	timerQueueCursor = timerQueue;
	if(timerQueueCursor->next == NULL)
	{
		nodeTmp->next = NULL;
		timerQueueCursor->next = nodeTmp;
	}
	else
	{
		// add current node in the proper index based on wakeUpTime
		while(timerQueueCursor->next != NULL)
		{
			preTimerQueueCursor = timerQueueCursor;
			timerQueueCursor = timerQueueCursor->next;
			if(currentPCBNode->wakeUpTime <= timerQueueCursor->node->wakeUpTime)
			{
				nodeTmp->next = timerQueueCursor;
				preTimerQueueCursor->next = nodeTmp;
				break;
			}
		}

		if(currentPCBNode->wakeUpTime > timerQueueCursor->node->wakeUpTime)
		{
			timerQueueCursor->next = nodeTmp;
			nodeTmp->next = NULL;
		}
	}
	
	CALL(MEM_WRITE(Z502TimerStart, sleep_time));
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	dispatcher();
	return 0;
}
int dispatcher()
{
	while(readyQueue->next == NULL)
	{
		//if no process in the whole program, just halt 
		if(totalQueue->next == NULL)
		{
			Z502Halt();
		}
		currentPCBNode = NULL;
		CALL(Z502Idle());
	}
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	currentPCBNode = readyQueue->next->node;
	//free the mode in readyQueue???
	//pop up the first node from readyQueue
	readyQueue->next = readyQueue->next->next;

	strncpy(action,"Dispatch",8);
	schedule_printer();
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(currentPCBNode->context) );
}
long get_pid_by_name(char *name)
{
    Queue totalQueueCursor;
	//current_statue_print();
	if(strcmp(name, "") == 0)
	{
		return currentPCBNode->pid;
	}
    totalQueueCursor = totalQueue;

    while(totalQueueCursor->next != NULL)
    {
        totalQueueCursor = totalQueueCursor->next;
        if(strcmp(totalQueueCursor->node->name,name) == 0)
        {
            return totalQueueCursor->node->pid;
        }
        
    }   
    return NO_PCB_NODE_FOUND;
}

PCB * PCB_item_generator(SYSTEM_CALL_DATA *SystemCallData)
{
	void *next_context;
	pcb = (PCB*)malloc(sizeof(PCB));
	strcpy(pcb->name, (char*)SystemCallData->Argument[0]);
	Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
	pcb->context = next_context;
	pcb->prior = (int)SystemCallData->Argument[2];
	return pcb;
}
void new_node_add_to_readyQueue(Queue readyNode, int addType)
{
	Queue readyQueueCursor, tmpPreCursor;
	if(addType == ADD_BY_PRIOR)
	{
		if(readyQueue->next == NULL)
		{
			readyQueue->next = readyNode;
		}
		else
		{
			readyQueueCursor = readyQueue;
			while(readyQueueCursor->next != NULL)
			{
				tmpPreCursor = readyQueueCursor;
				readyQueueCursor = readyQueueCursor->next;
				if(readyNode->node->prior <= readyQueueCursor->node->prior)
				{
					readyNode->next = readyQueueCursor;
					tmpPreCursor->next = readyNode;
					break;
				}
			}

			if(readyNode->node->prior > readyQueueCursor->node->prior)
			{
				readyQueueCursor->next = readyNode;
				readyNode->next = NULL;
			}
		}
		
	}
	else 
	{
		readyQueueCursor = readyQueue;
		while(readyQueueCursor->next != NULL)
		{
			readyQueueCursor = readyQueueCursor->next;

		}
		readyQueueCursor->next = readyNode;
	}
}

/* 
 * if priority is legal & process name is unique, 
 * then create this process, and add it to the end of readyQueue 
 * 
 */
long process_creater(PCB *pcbNode)
{
	Queue readyQueueCursor,totalQueueCursor, readyNodeTmp, totalNodeTmp;
	// priority check
	if(pcbNode->prior == ILLEGAL_PRIORITY)
	{
		return ILLEGAL_PRIORITY;
	}
	// duplicate name check (totalQueue used here)
	totalQueueCursor = totalQueue;
	while(totalQueueCursor->next != NULL)
	{
		totalQueueCursor = totalQueueCursor->next;
		if(strcmp(totalQueueCursor->node->name,pcbNode->name) == 0)
		{
			return NAME_DUPLICATED;
		}
		
	}

	//Since everything is OK, now, we can append this node to the readyQueue
	/* * * * * * * * * * To create new node below * * * * * * * * * * */
	//Create two nodes, one for readyQueue, one for totalQueue
	readyNodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	totalNodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	pcbNode->pid = increamentPID++;
	readyNodeTmp->node = pcbNode;
	totalNodeTmp->node = pcbNode;
	readyNodeTmp->next = NULL;
	totalNodeTmp->next = NULL;

	new_node_add_to_readyQueue(readyNodeTmp, globalAddType);
	/*
	readyQueueCursor = readyQueue;
	while(readyQueueCursor->next != NULL)
	{
		readyQueueCursor = readyQueueCursor->next;

	}
	readyQueueCursor->next = readyNodeTmp;
	*/
	//Add the new node into the end of totalQueue
	/**************************************************/
	totalQueueCursor = totalQueue;
	while(totalQueueCursor->next != NULL)
	{
		totalQueueCursor = totalQueueCursor->next;
	}
	totalQueueCursor->next = totalNodeTmp;
	/**************************************************/
	currentCountOfProcess++;
	return pcbNode->pid;
}
void myself_teminator( )
{
	Queue tmpQueueCursor;
	Queue totalQueueCursor = totalQueue;

	while(totalQueueCursor->next != NULL)
	{
		tmpQueueCursor = totalQueueCursor;
		totalQueueCursor = totalQueueCursor->next;
		if(totalQueueCursor->node->pid == currentPCBNode->pid)
		{
			//printf("\PID is found!\n");
			tmpQueueCursor->next = totalQueueCursor->next;
			// free the node, which has been teminated from & removed from the Queue
			free(totalQueueCursor);
			break;
			//return SUCCESS;
		}
	}

	currentPCBNode = NULL;
	dispatcher();

}

int process_teminator_by_pid(long pID)
{
	Queue tmpQueueCursor;
	Queue readyQueueCursor = readyQueue;
	Queue timerQueueCursor = timerQueue;
	Queue totalQueueCursor = totalQueue;

	while(totalQueueCursor->next != NULL)
	{
		tmpQueueCursor = totalQueueCursor;
		totalQueueCursor = totalQueueCursor->next;
		if(totalQueueCursor->node->pid == pID)
		{
			//printf("\PID is found!\n");
			tmpQueueCursor->next = totalQueueCursor->next;
			// free the node, which has been teminated from & removed from the Queue
			free(totalQueueCursor);
			//return SUCCESS;
			break;
		}
	}
	/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	while(readyQueueCursor->next != NULL)
	{
		tmpQueueCursor = readyQueueCursor;
		readyQueueCursor = readyQueueCursor->next;
		if(readyQueueCursor->node->pid == pID)
		{
			//printf("\PID is found!\n");
			tmpQueueCursor->next = readyQueueCursor->next;
			// free the node, which has been teminated from & removed from the Queue
			free(readyQueueCursor);
			return SUCCESS;
		}
	}

	while(timerQueueCursor->next != NULL)
	{
		tmpQueueCursor = timerQueueCursor;
		timerQueueCursor = timerQueueCursor->next;
		if(timerQueueCursor->node->pid == pID)
		{
			//printf("\PID is found!\n");
			tmpQueueCursor->next = timerQueueCursor->next;
			// free the node, which has been teminated from & removed from the Queue
			free(timerQueueCursor);
			return SUCCESS;
		}
	}
	//printf("\PID is not found!\n");
	return FAIL;
}
int suspend_by_PID(long pid)
{
	Queue suspendQueueCursor, queueCursor, preQueueCursor;
	// check whether it tries to suspend itself, it will not be legal here, in my program
	if(pid == -1)
	{
		return SLEF_SUSPENDED_ERR;
	}

	// check whether the pid is legal or not
	if(pid > MAX_LEGAL_PID)
	{
		return ILLEGAL_PID;
	}
	// then check this pid is not in suspendQueue, and get the end point of suspendQueue: suspendQueueCursor
	suspendQueueCursor = suspendQueue;
	while(suspendQueueCursor != NULL && suspendQueueCursor->next != NULL)
	{
		suspendQueueCursor = suspendQueueCursor->next;
		if(suspendQueueCursor->node->pid == pid)
		{
			return ALREADY_SUSPENDED;
		}
	}
	// Now everything is OK, next step is to suspend the very node
	// check readyQueue
	queueCursor = readyQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		preQueueCursor = queueCursor;
		queueCursor = queueCursor->next;
		if(queueCursor->node->pid == pid)
		{
			// remove the node from readyQueue
			preQueueCursor->next = queueCursor->next; 
			// add the node to suspendQueue
			suspendQueueCursor->next = queueCursor;
			// reset the next point to end of suspendQueue to NULL
			suspendQueueCursor->next->next = NULL;

			return SUCCESS;
		}
	}
	// check timerQueue
	queueCursor = timerQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		preQueueCursor = queueCursor;
		queueCursor = queueCursor->next;
		if(queueCursor->node->pid == pid)
		{
			// remove the node from readyQueue
			preQueueCursor->next = queueCursor->next; 
			// add the node to suspendQueue
			suspendQueueCursor->next = queueCursor;
			// reset the next point to end of suspendQueue to NULL
			suspendQueueCursor->next->next = NULL;

			return SUCCESS;
		}
	}
	return PID_NOT_FOUND;
}
int resume_by_PID(long pid)
{
	Queue suspendQueueCursor, preQueueCursor, queueNode;
	// first check whether the pid is legal or not
	if(pid > MAX_LEGAL_PID)
	{
		return ILLEGAL_PID;
	}

	// resume ALL
	if(pid == -1)
	{
		suspendQueueCursor = suspendQueue;
		while(suspendQueueCursor != NULL && suspendQueueCursor->next != NULL)
		{
			preQueueCursor = suspendQueueCursor;
			suspendQueueCursor = suspendQueueCursor->next;

			// generate a new, which will be added to readyQueue
			queueNode = (QUEUE *)malloc(sizeof(QUEUE));
			queueNode->node = suspendQueueCursor->node;
			queueNode->next = NULL;
			// add the node to readyQueue
			new_node_add_to_readyQueue(queueNode, globalAddType);
			
			// now, we will remove the node from suspendQueue
			preQueueCursor->next = suspendQueueCursor->next;

		}
	}

	// then check this pid is not in suspendQueue, and get the end point of suspendQueue: suspendQueueCursor
	suspendQueueCursor = suspendQueue;
	while(suspendQueueCursor != NULL && suspendQueueCursor->next != NULL)
	{
		preQueueCursor = suspendQueueCursor;
		suspendQueueCursor = suspendQueueCursor->next;
		if(suspendQueueCursor->node->pid == pid)
		{
			// generate a new, which will be added to readyQueue
			queueNode = (QUEUE *)malloc(sizeof(QUEUE));
			queueNode->node = suspendQueueCursor->node;
			queueNode->next = NULL;
			// add the node to readyQueue
			new_node_add_to_readyQueue(queueNode, globalAddType);

			// now, we will remove the node from suspendQueue
			preQueueCursor->next = suspendQueueCursor->next;
			return SUCCESS;
		}
	}
	return PCB_NOT_SUSPENDED;
}
int priority_changer(long pid, int priority)
{
	Queue queueCursor,preQueueCursor, queueNode;
	// check whether the pid is legal or not
	if(priority > MAX_LEGAL_PRIOR)
	{
		return ILLEGAL_PRIOR;
	}

	// check whether the node needs change is current running Node
	if(pid == -1 || pid == currentPCBNode->pid)
	{
		currentPCBNode->prior = priority;
		return SUCCESS;
	}
	// modify totalQueue
	queueCursor = totalQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		if(queueCursor->node->pid == pid)
		{
			queueCursor->node->prior = priority;
			break;
		}
	}

	// check readyQueue
	// after priority is changed, we should resort readyQueue
	queueCursor = readyQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		preQueueCursor = queueCursor;
		queueCursor = queueCursor->next;
		if(queueCursor->node->pid == pid)
		{
			queueCursor->node->prior = priority;
			queueNode = queueCursor;
			preQueueCursor->next = queueCursor->next; // remove the node from readyQueue
			queueNode->next = NULL;
			new_node_add_to_readyQueue(queueNode, globalAddType); // add the node into readyQueue
			return SUCCESS;
		}
	}
	// check timerQueue
	queueCursor = timerQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		if(queueCursor->node->pid == pid)
		{
			queueCursor->node->prior = priority;
			return SUCCESS;
		}
	}
	// check suspendQueue
	queueCursor = suspendQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		if(queueCursor->node->pid == pid)
		{
			queueCursor->node->prior = priority;
			return SUCCESS;
		}
	}

	

	return PID_NOT_FOUND;
}
int msg_sender(long sid, long tid, char *msg, int msgLength)
{
	MSG *msgNode;
	MsgQueue msgCursor, msgQueueNode;
	resume_by_PID(sid);
	// check whether the pid is legal or not
	if(tid > MAX_LEGAL_PID)
	{
		return ILLEGAL_PID;
	}
	/*
	// target id = -1 means a broadcast, not means the target to itself
	// so, in msg queue, it is legal that the target id = -1
	if(tid == -1L)
	{
		tid = sid;
	}
	*/
	msgCursor = msgQueue;
	while(msgCursor != NULL && msgCursor->next != NULL)
	{
		msgCursor = msgCursor->next;
	}
	msgNode = (MSG*)malloc(sizeof(MSG));
	msgNode->sid = sid;
	msgNode->tid = tid;
	msgNode->length = msgLength;
	strncpy(msgNode->msg,msg,msgLength);

	msgQueueNode = (MESSAGE *)malloc(sizeof(MESSAGE));
	msgQueueNode->node = msgNode;
	msgQueueNode->next = NULL;

	msgCursor->next = msgQueueNode;
	// resume target pid node, if it is suspended
	resume_by_PID(tid);

	return SUCCESS;
}

int msg_receiver(long sid, char *msg, int msgLength, long *actualLength, long *actualSid)
{
	MsgQueue msgCursor, preMsgQueue;
	Queue queueCursor, queueNode, tmpCurrentNode;
	//current_statue_print();
	// check whether the pid is legal or not
	if(sid > MAX_LEGAL_PID)
	{
		return ILLEGAL_PID;
	}
	memset(msg,'\0',msgLength);
	msgCursor = msgQueue;
	while(msgCursor != NULL && msgCursor->next != NULL)
	{
		preMsgQueue = msgCursor;
		msgCursor = msgCursor->next;
		// match the specified source pid and target pid
		if((sid == -1 || msgCursor->node->sid == sid) && ((msgCursor->node->tid == -1 && msgCursor->node->sid != sid) || msgCursor->node->tid == currentPCBNode->pid))
		{
			if(msgCursor->node->length > msgLength)
			{
				return TOO_SMALL_BUF_SIZE;
			}
			else
			{
				strncpy(msg, msgCursor->node->msg, msgCursor->node->length);
				*actualLength = (long)msgCursor->node->length;
				*actualSid = msgCursor->node->sid;

				//remove the node from msgQueue
				preMsgQueue->next = msgCursor->next;

				return SUCCESS;
			}
		}
	}

	// check whether timerQueue is NULL or not
	// if not NULL, current PCB go to IDLE until interrupt
	//queueCursor = timerQueue;
	if(timerQueue->next != NULL)
	{
		while(timerQueue->next != NULL)
		{
			CALL(Z502Idle());
		}
		tmpCurrentNode = (QUEUE*)malloc(sizeof(QUEUE));
		tmpCurrentNode->node = currentPCBNode;
		tmpCurrentNode->next = NULL;
		new_node_add_to_readyQueue(tmpCurrentNode, ADD_BY_END);
		dispatcher();
	}
	else
	{
		// no message found for current ruunning node
		// then add current node into suspendQueue
		// at last, do switch context

		READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
		queueCursor = suspendQueue;
		while(queueCursor != NULL && queueCursor->next != NULL)
		{
			queueCursor = queueCursor->next;
		}
		// create a new node for suspendQueue, then do the insertion
		queueNode = (QUEUE*)malloc(sizeof(QUEUE));
		queueNode->node = currentPCBNode;
		queueNode->next = NULL;
		queueCursor->next = queueNode;
		READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
		// assign new node for currentPCBNode
		dispatcher();
		//current_statue_print();
	}
	msg_receiver(sid, msg, msgLength, actualLength, actualSid);
}

/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
	INT32				sleepTime;
    //static BOOL        remove_this_in_your_code = TRUE;   /** TEMP **/
    //static INT32       how_many_interrupt_entries = 0;    /** TEMP **/
	Queue readyQueueCursor, timerQueueCursor, preTmpCursor, queueNode;
	//INT32 currentTime;
	

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    /** REMOVE THE NEXT SIX LINES **/
    //how_many_interrupt_entries++;                         /** TEMP **/
    /*if ( remove_this_in_your_code && ( how_many_interrupt_entries < 20 ) )
        {
        printf( "Interrupt_handler: Found device ID %d with status %d\n",
                        device_id, status );
    }
	*/
    // Clear out this device - we're done with it
    

	//get current absolute time
	
	//get the end of readyQueue
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	/*
	readyQueueCursor = readyQueue;
	while(readyQueueCursor->next != NULL)
	{
		readyQueueCursor = readyQueueCursor->next;
	}
	*/
	//add the first node from timerQueue to the end of readyQueue
	timerQueueCursor = timerQueue;

	while(timerQueueCursor != NULL && timerQueueCursor->next != NULL)
	{
		preTmpCursor = timerQueueCursor;
		timerQueueCursor = timerQueueCursor->next;

		// get current time 
		MEM_READ( Z502ClockStatus, &currentTime );

		if(timerQueueCursor->node->wakeUpTime <= currentTime)
		{
			//clone a queue node
			queueNode = (QUEUE *)malloc(sizeof(QUEUE));
			queueNode->node = timerQueueCursor->node;
			queueNode->next = NULL;

			new_node_add_to_readyQueue(queueNode, globalAddType);

			//readyQueueCursor->next = timerQueueCursor;
			preTmpCursor->next = timerQueueCursor->next;

			timerQueueCursor = timerQueueCursor->next;

			//readyQueueCursor = readyQueueCursor->next;
 			//readyQueueCursor->next = NULL;
		}
		else
		{
			break;
		}
	}

	//reset sleep time
	
	if(timerQueue->next != NULL)
	{
		CALL(MEM_READ( Z502ClockStatus, &currentTime ));
		sleepTime = timerQueue->next->node->wakeUpTime-currentTime;
		CALL(MEM_WRITE(Z502TimerStart, &sleepTime));
	}
	
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);

	MEM_WRITE(Z502InterruptClear, &Index );
}                                       /* End of interrupt_handler */
/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void    fault_handler( void )
    {
    INT32       device_id;
    INT32       status;
    INT32       Index = 0;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    printf( "Fault_handler: Found vector type %d with value %d\n",
                        device_id, status );

	if(device_id == 4 && status == 0)
	{
		printf("@@@@@Illegal hardware instruction\n");
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

void    svc( SYSTEM_CALL_DATA *SystemCallData ) 
{
    short               call_type;
    static short        do_print = 10;
    short               i;
	//INT32				Time;
	INT32				Temp;
	PCB					*pcb;
	static long			pid;
	int					returnStatus;
	int					priority;
	char				message[MAX_MSG_LENGTH];
	int					msgLength;
	long				actual_send_length;
	long				actual_source_pid;
	static int			msgCount = 0;
	//extern long			Z502_REG1;

    call_type = (short)SystemCallData->SystemCallNumber;
    if ( do_print > 0 ) 
	{
        printf( "SVC handler: %s\n", call_names[call_type]);
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++ )
		{
        	 //Value = (long)*SystemCallData->Argument[i];
             printf( "Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
             (unsigned long )SystemCallData->Argument[i],
             (unsigned long )SystemCallData->Argument[i]);
        }
		do_print--;
    }
	switch(call_type)
    {
	    
	    // Get time service call
	    case SYSNUM_GET_TIME_OF_DAY:
		    CALL(MEM_READ( Z502ClockStatus, &currentTime ));
            *(INT32 *)SystemCallData->Argument[0] = currentTime;
            break;
		
		//SLEEP CALL
		case SYSNUM_SLEEP:
			start_timer(&(INT32*)SystemCallData->Argument[0]);
			//strncpy(action,"Sleep",8);
			//schedule_printer();
			break;
		
		//Create Process
		case SYSNUM_CREATE_PROCESS:
			pcb = PCB_item_generator(SystemCallData);
			pid = process_creater(pcb);
			if(pid == ILLEGAL_PRIORITY || pid == NAME_DUPLICATED)
			{
				*(long *)SystemCallData->Argument[3] = pid;
				*(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
			}
			else
			{
				*(long *)SystemCallData->Argument[3] = pid;
				*(long *)SystemCallData->Argument[4] = ERR_SUCCESS;
			}


			if(currentCountOfProcess > MAX_COUNT_OF_PROCESSES)
			{
				*(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
			}
			strncpy(action,"Create",8);
			schedule_printer();
			break;

		case SYSNUM_GET_PROCESS_ID:
			//Get process ID in this section
			pid = get_pid_by_name((char*)SystemCallData->Argument[0]);
			*(long*)SystemCallData->Argument[1] = pid;
			if(pid == NO_PCB_NODE_FOUND)
			{
				*(long *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}
			else
			{
				*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
			}
			break;

		// suspend system call
		case SYSNUM_SUSPEND_PROCESS:
			pid =(long)SystemCallData->Argument[0];
			returnStatus = suspend_by_PID(pid);
			if(returnStatus == SUCCESS)
			{
				*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else
			{
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			strncpy(action,"Suspend",8);
			schedule_printer();
			break;

		case SYSNUM_RESUME_PROCESS:
			pid =(long)SystemCallData->Argument[0];
			returnStatus = resume_by_PID(pid);
			if(returnStatus == SUCCESS)
			{
				*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else
			{
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			strncpy(action,"Resume",8);
			schedule_printer();
			break;

		case SYSNUM_CHANGE_PRIORITY:
			pid = (long)SystemCallData->Argument[0];
			priority = (int)SystemCallData->Argument[1];
			returnStatus = priority_changer(pid, priority);
			if(returnStatus == SUCCESS)
			{
				*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
			}
			else
			{
				*(long *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}
			strncpy(action,"Chg_Pior",8);
			schedule_printer();
			break;

		case SYSNUM_SEND_MESSAGE:
			msgCount++;
			if(msgCount > MAX_MSG_COUNT)
			{
				*(long *)SystemCallData->Argument[3] = ERR_BAD_PARAM;
				break;
			}
			pid = (long)SystemCallData->Argument[0];
			msgLength = (int)SystemCallData->Argument[2];
			// check msgLength
			if(msgLength > MAX_MSG_LENGTH)
			{
				*(long *)SystemCallData->Argument[3] = ERR_BAD_PARAM;
			}
			else
			{
				strncpy(message, (char*)SystemCallData->Argument[1],msgLength);
				returnStatus = msg_sender(currentPCBNode->pid,pid, message, msgLength);
				if(returnStatus == SUCCESS)
				{
					*(long *)SystemCallData->Argument[3] = ERR_SUCCESS;
				}
				else
				{
					*(long *)SystemCallData->Argument[3] = ERR_BAD_PARAM;
				}
			}
			strncpy(action,"MsgSend",8);
			schedule_printer();
			break;

		case SYSNUM_RECEIVE_MESSAGE:
			//current_statue_print();
			pid = (long)SystemCallData->Argument[0];
			msgLength = (int)SystemCallData->Argument[2];
			// check msgLength
			if(msgLength > MAX_MSG_LENGTH)
			{
				*(long *)SystemCallData->Argument[5] = ERR_BAD_PARAM;
			}
			else
			{
				//strncpy(message, (char*)SystemCallData->Argument[1],msgLength);
				returnStatus = msg_receiver(pid, (char*)SystemCallData->Argument[1], msgLength,SystemCallData->Argument[3], SystemCallData->Argument[4]);
				if(returnStatus == SUCCESS)
				{
					*(long *)SystemCallData->Argument[5] = ERR_SUCCESS;
				}
				else
				{
					*(long *)SystemCallData->Argument[5] = ERR_BAD_PARAM;
				}
			}
			//strncpy(action,"MsgRecv",8);
			//schedule_printer();
			break;
		
		// this is not used in phase 1
		case SYSNUM_MEM_READ:
			if(SystemCallData->Argument[0] != Z502ClockStatus)
			{
				printf("Illegal hardware instruction\n");
				Z502Halt();
			}
			break;
        // terminate system call
        case SYSNUM_TERMINATE_PROCESS:
			if((long)SystemCallData->Argument[0] == -2L)
			{
				Z502Halt();
			}
			else if((long)SystemCallData->Argument[0] == -1L)
			{
				//printf("Current %ld is killed!\n", currentPCBNode->pid ); 
				myself_teminator();
			}
			else
			{
				returnStatus = process_teminator_by_pid((long)SystemCallData->Argument[0]);
			
				if(returnStatus == SUCCESS)
				{
					*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
				}
				else
				{
					*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
				}
			}
			strncpy(action,"Teminate",8);
			schedule_printer();
            break;
        default:  
            printf( "ERROR!  call_type not recognized!\n" ); 
            printf( "Call_type is - %i\n", call_type);
    }
}                                               // End of svc

/************************************************************************
    osInit
        This is the first routine called after the simulation begins.  This
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/

void    osInit( int argc, char *argv[]  ) {
    void                *next_context;
    INT32               i;

	static char test[21];
	// Init the global queues
	PCB *rootPCB;
	Queue totalNodeTmp;
	totalNodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	rootPCB = (PCB*)malloc(sizeof(PCB));
	totalQueue = (QUEUE *)malloc(sizeof(QUEUE));
	readyQueue = (QUEUE *)malloc(sizeof(QUEUE));
	timerQueue = (QUEUE *)malloc(sizeof(QUEUE));
	suspendQueue = (QUEUE *)malloc(sizeof(QUEUE));
	msgQueue = (MESSAGE *)malloc(sizeof(MESSAGE));
	totalQueue->next = NULL;
	readyQueue->next = NULL;
	timerQueue->next = NULL;
	suspendQueue->next = NULL;
	msgQueue->next = NULL;
	globalAddType = ADD_BY_PRIOR;

    /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", argc );
    for ( i = 0; i < argc; i++ )
        printf( " %s", argv[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;

	if( argc > 1 )
	{
		strncpy(test,argv[1],6);
	}
	else
	{
		fgets (test, 20, stdin);
	}

    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (strncmp( test, "sample", 6 ) == 0 ) 
	{
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
    }                   /* This routine should never return!!           */
	else if (strncmp( test, "test0", 5 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test0, USER_MODE );
	}
	else if (strncmp( test, "test1a", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1a, USER_MODE );
	}
	else if (strncmp( test, "test1b", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1b, USER_MODE );
	}
	else if (strncmp( test, "test1c", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1c, USER_MODE );
		globalAddType = ADD_BY_END; //ADD_BY_END | ADD_BY_PRIOR
	}
	else if (strncmp( test, "test1d", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1d, USER_MODE );
	}
	else if (strncmp( test, "test1e", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1e, USER_MODE );
	}
	else if (strncmp( test, "test1f", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1f, USER_MODE );
	}
	else if (strncmp( test, "test1g", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1g, USER_MODE );
	}
	else if (strncmp( test, "test1h", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1h, USER_MODE );
	}
	else if (strncmp( test, "test1i", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1i, USER_MODE );
	}
	else if (strncmp( test, "test1j", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1j, USER_MODE );
	}
	else if (strncmp( test, "test1k", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1k, USER_MODE );
	}
	else if (strncmp( test, "test1l", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1l, USER_MODE );
	}
	else if (strncmp( test, "test1m", 6 ) == 0 ) 
	{
		Z502MakeContext( &next_context, (void *)test1m, USER_MODE );
	}
	else
	{
		printf("Illegal Input\n");
		exit(0);
	}
	
	// generate current node (now it is the root node)
	rootPCB->pid = ROOT_PID;
	strcpy(rootPCB->name, ROOT_PNAME);
	rootPCB->context = next_context;
	rootPCB->prior = ROOT_PRIOR;
	totalNodeTmp->node = rootPCB;
	totalNodeTmp->next = NULL;
	totalQueue->next = totalNodeTmp;
	currentPCBNode = rootPCB;

    Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
	
	
}                                               // End of osInit

