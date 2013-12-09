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

		DEC. 2013 Implemented by Hao        hzhou@wpi.edu
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

void memory_printer();
void schedule_printer();
void ready_queue_print();
void timer_queue_print();
void suspend_queue_print();
void total_queue_print();
void current_statue_print();
int start_timer(long *);
void dispatcher();
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
void append_to_frameQueue(FRM *);
void interrupt_handler( void );
void fault_handler( void );
void svc( SYSTEM_CALL_DATA * );
void osInit( int , char **  );
void frameInit( void );
void diskInit(void);
void disk_readOrWrite(long , long , char* , int );
int check_disk_status(long);
void append_currentPCB_to_diskQueue(long , long , char* , int);
void add_currentPCB_to_diskQueue_head(long , long , char* , int);
void disk_queue_print();
void shadowTableInit( void );
void disk_node_transfer( INT32 );

// These locations are global and define information about the page table
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;
extern char			 MEMORY[PHYS_MEM_PGS * PGSIZE ];
extern void          *TO_VECTOR [];
char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ",
                            "get_pid  ", "create   ", "term_proc",
                            "suspend  ", "resume   ", "ch_prior ",
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };

// totalQueue is a global queue which contains all current nodes in the program
// current totalQueue = current readyQueue + current timerQueue + current suspendQueue
// totalQueue is useful to do GET_PID function
Queue totalQueue; 
Queue timerQueue;
Queue readyQueue;
Queue suspendQueue;
MsgQueue msgQueue;
FrmQueue frmQueue;
DiskQueue diskQueue[MAX_NUMBER_OF_DISKS];
static long increamentPID = 1; //store the maximum pid for all process
static long frameMaxCurrentID = 1; //store the maximum pid for all process
PCB *pcb;
static int currentCountOfProcess = 0;
static PCB *currentPCBNode;
INT32 LockResult, LockResult2, LockResultPrinter, TimeLockResult;
int globalAddType = ADD_BY_PRIOR; //ADD_BY_END | ADD_BY_PRIOR
char action[SP_LENGTH_OF_ACTION];
INT32 currentTime = 0;
int enablePrinter = 0;
static INT32 victim = 0;
shadowTable SHADOW_TBL[1024];

void memory_printer()
{
	Queue queueCursor;
	//DiskQueue diskQueueCursor;
	int count = 0;
	//int diskIndex = 1;
	if (enablePrinter == 0)
	{
		return;
	}
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 12, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResultPrinter);
	printf("\n");
	SP_print_header();
	SP_setup_action(SP_ACTION_MODE, action);
	SP_setup(SP_RUNNING_MODE, currentPCBNode->pid);

	// print information of readyQueue, if the nodes in readyQueue are more than 10, just print the first 10
	queueCursor = readyQueue;
	while (queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup(SP_READY_MODE, queueCursor->node->pid);
		if (count >= 10)
		{
			count = 0;
			break;
		}
	}

	// print information of timerQueue, if the nodes in timerQueue are more than 10, just print the first 10
	queueCursor = timerQueue;
	while (queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup(SP_WAITING_MODE, queueCursor->node->pid);
		if (count >= 10)
		{
			count = 0;
			break;
		}
	}

	// print information of suspendQueue, if the nodes in suspendQueue are more than 10, just print the first 10

	queueCursor = suspendQueue;
	while (queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup(SP_SUSPENDED_MODE, queueCursor->node->pid);
		if (count >= 10)
		{
			count = 0;
			break;
		}
	}

	SP_print_line();
	// reset action to NULL
	memset(action, '\0', 8);
	printf("\n");
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 12, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResultPrinter);
}
void schedule_printer()
{
	Queue queueCursor;
	int count = 0;
	//int diskIndex = 1;
	if(enablePrinter == 0)
	{
		return;
	}
	READ_MODIFY(MEMORY_INTERLOCK_BASE+11, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResultPrinter);
	printf("\n");
	SP_print_header();
	SP_setup_action(SP_ACTION_MODE, action);

	if(currentPCBNode != NULL)
	{
		SP_setup(SP_RUNNING_MODE, currentPCBNode->pid);
	}
	else
	{
		SP_setup(SP_RUNNING_MODE, 99);
	}
	

	// print information of readyQueue, if the nodes in readyQueue are more than 10, just print the first 10
	queueCursor = readyQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup(SP_READY_MODE, queueCursor->node->pid);
		if(count >= 10)
		{
			count = 0;
			break;
		}
	}

	// print information of timerQueue, if the nodes in timerQueue are more than 10, just print the first 10
	queueCursor = timerQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
	{
		queueCursor = queueCursor->next;
		count++;
		SP_setup(SP_WAITING_MODE, queueCursor->node->pid);
		if(count >= 10)
		{
			count = 0;
			break;
		}
	}

	// print information of suspendQueue, if the nodes in suspendQueue are more than 10, just print the first 10
	
	queueCursor = suspendQueue;
	while(queueCursor != NULL && queueCursor->next != NULL)
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

	SP_print_line();
	// reset action to NULL
	memset(action,'\0',8);
	printf("\n");
	disk_queue_print();
	READ_MODIFY(MEMORY_INTERLOCK_BASE+11, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResultPrinter);
}
void disk_queue_print()
{
	DiskQueue diskQueueCursor;
	int i = 1;
	for(i = 1; i < 4; i++)
	{
		diskQueueCursor = diskQueue[i]->next;
		printf("\tDisk %d:\t", i);
		while(diskQueueCursor != NULL)
		{
			printf("%ld(%d)  ", diskQueueCursor->PCB->pid, diskQueueCursor->alreadyGetDisk);
			diskQueueCursor = diskQueueCursor->next;
		}
		printf("\n");

	}
	printf("\n");
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
	//READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult2);
	MEM_READ( Z502ClockStatus, &currentTime );
	printf("timerQueue(%d):\t",currentTime);
	//printf("\ntimerQueue:\t");
	while(timerQueueCursor->next != NULL)
	{
		timerQueueCursor = timerQueueCursor->next;
		printf("%ld(%d)  ",timerQueueCursor->node->pid, timerQueueCursor->node->wakeUpTime);
	}
	printf("\n");
	//READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult2);
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
	//READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
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
	
	//READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
}
int start_timer(long *sleep_time)
{

	long _wakeUpTime;
	Queue timerQueueCursor,preTimerQueueCursor, nodeTmp;

	//READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	//get current absolute time, and set wakeUpTime attribute for currentPCBNode
	CALL(MEM_READ( Z502ClockStatus, &currentTime ));
	_wakeUpTime = currentTime + (INT32)*sleep_time;
	currentPCBNode->wakeUpTime = _wakeUpTime;
	
	// add current node into timer queue
	nodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	nodeTmp->node = currentPCBNode;
	timerQueueCursor = timerQueue;

	if(timerQueueCursor->next == NULL)
	{
		// if the timerQueue is NULL now,, just add it
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
				break; // break when the node is inserted, to save time
			}
		}

		// if the wakeUpTime of current node is longer than anynode in timerQueue, just add it to the end
		if(currentPCBNode->wakeUpTime > timerQueueCursor->node->wakeUpTime)
		{
			timerQueueCursor->next = nodeTmp;
			nodeTmp->next = NULL;
		}
	}
	
	CALL(MEM_WRITE(Z502TimerStart, sleep_time));
	//READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);

	// after the current node is inserted into timerQueue, just do dispatcher() to get a new node for current node
	dispatcher();
	return 0;
}

void dispatcher()
{
	// if no node in readyQueue now, just do Idle() to wait for sleeping node wake up by interruption 
	while(readyQueue->next == NULL)
	{
		// if no process in the whole program, just halt, as it meaningless to wait to forever
		if(totalQueue->next == NULL)
		{
			Z502Halt();
		}
		if(timerQueue->next == NULL && suspendQueue->next != NULL)
		{
			currentPCBNode = suspendQueue->next->node;
			suspendQueue->next = suspendQueue->next->next;
			/*strncpy(action,"Dispath",8);
			schedule_printer();*/
			return;
		}
		currentPCBNode = NULL;
		CALL(Z502Idle());
		// Test use: print current time
		/*MEM_READ( Z502ClockStatus, &currentTime );
		printf("time: %5d\n", currentTime % 100000 );*/
		strncpy(action,"IDEL",8);
		schedule_printer();
	}
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);

	// reset current node with the first node in readyQueue
	currentPCBNode = readyQueue->next->node;
	//free the mode in readyQueue???
	//pop up the first node from readyQueue
	readyQueue->next = readyQueue->next->next;
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	strncpy(action,"Dispath",8);
	schedule_printer();
	// switch to current node process
	Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(currentPCBNode->context) );
}
long get_pid_by_name(char *name)
{
    Queue totalQueueCursor;

    // if name == "", just return current node's PID
	if(strcmp(name, "") == 0)
	{
		return currentPCBNode->pid;
	}

	// otherwise, go through totalQueue to find the PID for the very name 
    totalQueueCursor = totalQueue;

    while(totalQueueCursor->next != NULL)
    {
        totalQueueCursor = totalQueueCursor->next;
        if(strcmp(totalQueueCursor->node->name,name) == 0)
        {
            return totalQueueCursor->node->pid;
        }
        
    }   

    // if not find the name, just return error
    return NO_PCB_NODE_FOUND;
}
PCB * PCB_item_generator(SYSTEM_CALL_DATA *SystemCallData)
{
	void *next_context;
	// generate a PCB node with the information we know
	pcb = (PCB*)malloc(sizeof(PCB));
	strcpy(pcb->name, (char*)SystemCallData->Argument[0]);
	Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
	pcb->context = next_context;
	pcb->prior = (int)SystemCallData->Argument[2];
	return pcb;
}
// according the addType, add the node into readyQueue [append | based on priority]
void new_node_add_to_readyQueue(Queue readyNode, int addType)
{
	Queue readyQueueCursor, tmpPreCursor;
	// add node into readyQueue based on priority, samllest one is the first node in the readyQueue 
	if(addType == ADD_BY_PRIOR)
	{
		if(readyQueue->next == NULL)
		{
			readyQueue->next = readyNode;
		}
		else
		{
			// if the readQueue is not NULL, just find the proper place to insert the new node
			readyQueueCursor = readyQueue;
			while (readyQueueCursor != NULL && readyQueueCursor->next != NULL)
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
		// append the node at the end of readyQueue
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

	// add new node into readyQueue according globalAddType
	new_node_add_to_readyQueue(readyNodeTmp, globalAddType);

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

	//resume_by_PID(-1);

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

	// after terminator itself, it must find other to replace itself
	currentPCBNode = NULL;
	dispatcher();

}
int process_teminator_by_pid(long pID)
{
	Queue tmpQueueCursor;
	Queue readyQueueCursor = readyQueue;
	Queue timerQueueCursor = timerQueue;
	Queue suspendQueueCursor = suspendQueue;
	Queue totalQueueCursor = totalQueue;


	// remove the node from totaoQueue to keep the totalQueue up-to-date
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
	// if the PID is the running PCB's pid
	if(pID == currentPCBNode->pid)
	{
		myself_teminator();
		return SUCCESS;
	}
	

	// search the node whether in readyQueue, if found, just remove it, then return
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

	// if not found in readyQueue, search it in timerQueue, if found, just remove it, then return
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

	// if not found in timerQueue, either, search it in suspendQueue, if found, just remove it, then return
	while(suspendQueueCursor->next != NULL)
	{
		tmpQueueCursor = suspendQueueCursor;
		suspendQueueCursor = suspendQueueCursor->next;
		if(suspendQueueCursor->node->pid == pID)
		{
			//printf("\PID is found!\n");
			tmpQueueCursor->next = suspendQueueCursor->next;
			// free the node, which has been teminated from & removed from the Queue
			free(suspendQueueCursor);
			return SUCCESS;
		}
	}
	//printf("\PID is not found!\n");
	return FAIL;
}
// suspend self is illegal
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

	// (not)resume ALL
	// change 10-05-2013 20:26, resume the first node in suspendQueueif the readyQueue is NULL, when target = -1
	// 
	if(pid == -1 && readyQueue->next == NULL)
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

			break; // add 10-15-2013 20:27
		}
		return SUCCESS;
	}
	else
	{
		// resume specified node
		// check whether this node is in suspendQueue or not, if found, just resume it, otherwise return error
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
			new_node_add_to_readyQueue(queueNode, globalAddType); // add the node into readyQueue based on addType
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

	// check whether the pid is legal or not
	if(tid > MAX_LEGAL_PID)
	{
		return ILLEGAL_PID;
	}

	// add the msg node at the end of msgQueue
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
			// check buffer size, whether it is large enought to buffer the message
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
		//dispatcher();
	}
	else
	{
		// no message found for current ruunning node
		// then add current node into suspendQueue
		// at last, do switch context

		//READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
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
		//READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
		// assign new node for currentPCBNode
	}
	dispatcher();
	// recursive, until it finds a message for it
	msg_receiver(sid, msg, msgLength, actualLength, actualSid);
}
void append_to_frameQueue(FRM *frmNode)
{
	FrmQueue frameQueueCursor;
	FrmQueue frmQueueNode;

	// get the end of frmQueue
	frameQueueCursor = frmQueue;
	while (frameQueueCursor->next != NULL)
	{
		frameQueueCursor = frameQueueCursor->next;
	}

	// setup new node in frmQueue
	frmQueueNode = (FRAME *)malloc(sizeof(FRAME));
	frmQueueNode->node = frmNode;
	frmQueueNode->next = NULL;
	
	// add the new node into frmQueue
	frameQueueCursor->next = frmQueueNode;
}
void append_currentPCB_to_diskQueue(long diskID, long sectorID, char* buffer, int readOrWrite)
{
	DiskQueue diskQueueCursor;
	DiskQueue diskNode = (DiskNode *)malloc(sizeof(DiskNode));

	diskNode->diskID = diskID;
	diskNode->sectorID = sectorID;
	diskNode->readOrWrite = readOrWrite;
	diskNode->alreadyGetDisk = 0;
	diskNode->PCB = (PCB*)malloc(sizeof(PCB));
	diskNode->PCB->pid = currentPCBNode->pid;
	strcpy(diskNode->PCB->name, currentPCBNode->name);
	diskNode->PCB->context = currentPCBNode->context;
	diskNode->PCB->prior = currentPCBNode->prior;
	//diskNode->PCB->diskID = currentPCBNode->diskID;
	//diskNode->PCB->sectorID = currentPCBNode->sectorID;
	//strcpy(diskNode->buffer, buffer);
	diskNode->next = NULL;

	diskQueueCursor = diskQueue[(int)diskID];
	while (diskQueueCursor->next != NULL)
	{
		diskQueueCursor = diskQueueCursor->next;
	}

	diskQueueCursor->next = diskNode;
}
void add_currentPCB_to_diskQueue_head(long diskID, long sectorID, char* buffer, int readOrWrite)
{
	DiskQueue diskQueueCursor;
	DiskQueue diskNode = (DiskNode *)malloc(sizeof(DiskNode));

	diskNode->diskID = diskID;
	diskNode->sectorID = sectorID;
	diskNode->readOrWrite = readOrWrite;
	diskNode->alreadyGetDisk = 1;

	diskNode->PCB = (PCB*)malloc(sizeof(PCB));
	diskNode->PCB->pid = currentPCBNode->pid;
	strcpy(diskNode->PCB->name, currentPCBNode->name);
	diskNode->PCB->context = currentPCBNode->context;
	diskNode->PCB->prior = currentPCBNode->prior;
	//diskNode->PCB->diskID = currentPCBNode->diskID;
	//diskNode->PCB->sectorID = currentPCBNode->sectorID;
	//strcpy(diskNode->buffer, buffer);
	diskNode->next = diskQueue[(int)diskID]->next;
	diskQueue[(int)diskID]->next = diskNode;
}
int check_disk_status(long diskID)
{
	INT32 diskStatus;
	MEM_WRITE(Z502DiskSetID, &diskID);
	MEM_READ(Z502DiskStatus, &diskStatus);
	return diskStatus;
}
void disk_readOrWrite(long diskID, long sectorID, char* buffer, int readOrWrite)
{
	INT32 diskStatus;
	Queue tmpNode = (QUEUE*)malloc(sizeof(QUEUE));
	diskStatus = check_disk_status(diskID);

	if (diskStatus == DEVICE_FREE)        // Disk hasn't been used - should be free
	{
		// TODO: It can be blank here
		//printf("Got expected result for Disk Status\n");
	}
	else //DEVICE_IN_USE
	{
		while(diskStatus == DEVICE_IN_USE)
		{
			append_currentPCB_to_diskQueue(diskID, sectorID, buffer, readOrWrite);
			dispatcher();
			diskStatus = check_disk_status(diskID);
		}
		
	}

	CALL(MEM_WRITE(Z502DiskSetID, &diskID));
	CALL(MEM_WRITE(Z502DiskSetSector, &sectorID));
	CALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)buffer));

	diskStatus = readOrWrite;                     
	CALL(MEM_WRITE(Z502DiskSetAction, &diskStatus));
	diskStatus = 0;                        // Must be set to 0
	CALL(MEM_WRITE(Z502DiskStart, &diskStatus));
	
	add_currentPCB_to_diskQueue_head(diskID, sectorID, buffer, readOrWrite);
	dispatcher();
}

void disk_node_transfer( INT32 diskID )
{
	Queue queueNode;
	DiskQueue diskQueueCursor;
	READ_MODIFY(MEMORY_INTERLOCK_BASE+7, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	if ( check_disk_status(diskID) == DEVICE_FREE && diskQueue[diskID]->next != NULL )
	{
		diskQueueCursor = diskQueue[diskID]->next;
		while(diskQueueCursor != NULL && diskQueueCursor->alreadyGetDisk == 1 )
		{
			queueNode = (QUEUE*)malloc(sizeof(QUEUE));
			queueNode->node = diskQueueCursor->PCB;
			queueNode->next = NULL;
			new_node_add_to_readyQueue(queueNode, ADD_BY_PRIOR);

			diskQueueCursor = diskQueueCursor->next;
			diskQueue[diskID]->next = diskQueue[diskID]->next->next;
		}

		if (diskQueue[diskID]->next != NULL)
		{
			diskQueueCursor = diskQueue[diskID]->next;

			queueNode = (QUEUE*)malloc(sizeof(QUEUE));
			queueNode->node = diskQueueCursor->PCB;
			queueNode->next = NULL;
			new_node_add_to_readyQueue(queueNode, ADD_BY_PRIOR);
			diskQueue[diskID]->next = diskQueue[diskID]->next->next;
		}
	}
	READ_MODIFY(MEMORY_INTERLOCK_BASE+7, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
}

/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/
void interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
	INT32			   sleepTime;
	INT32			   diskID = 1;
	INT32			   i = 1;
	Queue readyQueueCursor, timerQueueCursor, preTmpCursor, queueNode;
	DiskQueue diskQueueCursor;
	//INT32 currentTime;
	

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
    // Clear out this device - we're done with it

	switch (device_id)
	{
		case TIMER_INTERRUPT:
			READ_MODIFY(MEMORY_INTERLOCK_BASE+10, DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
			//add the first node from timerQueue to the end of readyQueue
			timerQueueCursor = timerQueue;
			// get current time 
			MEM_READ(Z502ClockStatus, &currentTime);
			while (timerQueueCursor != NULL && timerQueueCursor->next != NULL)
			{
				preTmpCursor = timerQueueCursor;
				timerQueueCursor = timerQueueCursor->next;

				if (timerQueueCursor->node->wakeUpTime <= currentTime)
				{
					//clone a queue node
					queueNode = (QUEUE *)malloc(sizeof(QUEUE));
					queueNode->node = timerQueueCursor->node;
					queueNode->next = NULL;

					new_node_add_to_readyQueue(queueNode, globalAddType);
					preTmpCursor->next = timerQueueCursor->next;
					timerQueueCursor = timerQueueCursor->next;
				}
				else
				{
					break;
				}
			}

			//reset sleep time to ensure there will be a interrupt to pop out the node in the timerQueue

			if (timerQueue->next != NULL)
			{
				CALL(MEM_READ(Z502ClockStatus, &currentTime));
				sleepTime = timerQueue->next->node->wakeUpTime - currentTime;
				CALL(MEM_WRITE(Z502TimerStart, &sleepTime));
			}
			READ_MODIFY(MEMORY_INTERLOCK_BASE + 10, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
			break;

		default:
			
			if (device_id < TIMER_INTERRUPT || device_id >= 13) //DISK_INTERRUPT + MAX_NUMBER_OF_DISKS
			{
				break;
			}
			READ_MODIFY(MEMORY_INTERLOCK_BASE+15, DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
			// make the first node in diskQueue be the current one
			diskID = device_id - 4;
			disk_node_transfer(diskID);
			
			for(i = 1; i < MAX_NUMBER_OF_DISKS; i++ )
			{
				if(i != diskID)
				{
					disk_node_transfer(i);
				}
				
			}
			READ_MODIFY(MEMORY_INTERLOCK_BASE + 15, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
			break;

	}
	MEM_WRITE(Z502InterruptClear, &Index );
}                                       /* End of interrupt_handler */
/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void fault_handler( void )
{
    INT32       device_id;
    INT32       status;
    INT32       Index = 0;
	INT32		i = 0;
	FrmQueue	frameQueueCursor;
	long		diskID;
	long		sectorID;
	INT32		isFound = 0;
	long		frameID;
    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

	//printf( "Fault_handler: Found vector type %d with value %d\n", device_id, status );

	if(device_id != 4)
	{
		if (status >= 1024)
		{
			printf("\n@@@@@Page size overflow!\n\n");
			Z502Halt();
		}
		else
		{
			printf("Page ID:%d\t", status);
		}

		if (Z502_PAGE_TBL_ADDR == NULL)
		{
			Z502_PAGE_TBL_LENGTH = 1024;
			Z502_PAGE_TBL_ADDR = (UINT16 *)calloc( sizeof(UINT16), Z502_PAGE_TBL_LENGTH );
			for (i = 0; i < Z502_PAGE_TBL_LENGTH; i++)
			{
				Z502_PAGE_TBL_ADDR[i] = (UINT16)0;
			}
		}

		frameQueueCursor = frmQueue->next;
		// can do some optimization here, as when frame is changed to be used status, it will never return to unused status
		while (frameQueueCursor != NULL && frameQueueCursor->node->isAvailable != 1)
		{
			frameQueueCursor = frameQueueCursor->next;
		}
		if (frameQueueCursor != NULL)
		{
			frameQueueCursor->node->isAvailable = 0; // indicate that this frame is used
			frameQueueCursor->node->pageID = status;
			frameQueueCursor->node->pid = currentPCBNode->pid;
			frameID = frameQueueCursor->node->frameID;
		}
		else  // this means all frames have been used before
		{
			// TODO: replace algorithm
			frameQueueCursor = frmQueue->next;
			while (TRUE)
			{
				if(frameQueueCursor != NULL && (Z502_PAGE_TBL_ADDR[frameQueueCursor->node->pageID] & 0x2000) != 0x2000 )
				{
					// if reference bit is not set, swap it here
					// store the old info into disk
					diskID = 1;//((frameQueueCursor->node->pageID & 0x0018) >> 3) + 1;
					sectorID = frameQueueCursor->node->pageID;// (frameQueueCursor->node->pageID & 0x0FE0) >> 5;

					SHADOW_TBL[frameQueueCursor->node->pageID].diskID = diskID;
					SHADOW_TBL[frameQueueCursor->node->pageID].sectorID = sectorID;
					SHADOW_TBL[frameQueueCursor->node->pageID].frameID = frameQueueCursor->node->frameID;

					Z502_PAGE_TBL_ADDR[frameQueueCursor->node->pageID] &= 0x7FFF; // set the valid bit to 0
					disk_readOrWrite(diskID,sectorID,(char*)&MEMORY[frameQueueCursor->node->frameID * PGSIZE], DISK_WRITE);

					//Z502_PAGE_TBL_ADDR[status] = frameQueueCursor->node->frameID;
					frameQueueCursor->node->pageID = status; // new pageID
					frameQueueCursor->node->pid = currentPCBNode->pid;
					victim = (frameQueueCursor->node->pageID + 1) % Z502_PAGE_TBL_LENGTH;

					frameID = frameQueueCursor->node->frameID;

					break;
				}
				else
				{
					// set reference bit 0
					Z502_PAGE_TBL_ADDR[frameQueueCursor->node->pageID] &= 0xDFFF;

					frameQueueCursor = frameQueueCursor->next;
					if(frameQueueCursor == NULL)
					{
						frameQueueCursor = frmQueue->next;
					}
				}
			}
		}
		// make the page valid
		Z502_PAGE_TBL_ADDR[status] = (UINT16)frameID | 0x8000;

		if(SHADOW_TBL[status].diskID > -1) // pageID = status has its item in shadow table
		{
			// new pageID has content in shadow table
			disk_readOrWrite(SHADOW_TBL[status].diskID,SHADOW_TBL[status].sectorID,(char*)&MEMORY[SHADOW_TBL[status].frameID * PGSIZE], DISK_READ);
			SHADOW_TBL[status].diskID = -1;
			SHADOW_TBL[status].sectorID = -1;
		}
	}

	

	if(device_id == 4 && status == 0)
	{
		printf("\n@@@@@Illegal hardware instruction\n\n");
		//Z502Halt();
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

void svc( SYSTEM_CALL_DATA *SystemCallData ) 
{
    short               call_type;
    static short        do_print = 10;
    short               i;
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
	INT32				diskStatus;
	long				diskID;
	long				sectorID;
	char				buffer[PGSIZE];

    call_type = (short)SystemCallData->SystemCallNumber;
    if ( do_print > 0 ) 
	{
        printf( "SVC handler: %s\n", call_names[call_type]);
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++ )
		{
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
			break;
		
		//Create Process
		case SYSNUM_CREATE_PROCESS:
			resume_by_PID(-1);
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
			strncpy(action,"ChgPior",8);
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
		
		case SYSNUM_DISK_READ:
			/*strncpy(action,"DSKREAD",8);
			schedule_printer();*/
			//READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
			diskID = SystemCallData->Argument[0];
			sectorID = SystemCallData->Argument[1];
			disk_readOrWrite(	diskID,
								sectorID,
								buffer,
								DISK_READ );

			memcpy (SystemCallData->Argument[2], buffer, PGSIZE);
			//READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
			break;
	
		case SYSNUM_DISK_WRITE:
			/*strncpy(action,"DSKWRT",8);
			schedule_printer();*/
			//READ_MODIFY(MEMORY_INTERLOCK_BASE + 4, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
			diskID = SystemCallData->Argument[0];
			sectorID = SystemCallData->Argument[1];
			memcpy(buffer, SystemCallData->Argument[2], PGSIZE);
			//READ_MODIFY(MEMORY_INTERLOCK_BASE + 4, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
			disk_readOrWrite(	diskID,
								sectorID,
								buffer,
								DISK_WRITE );
			
			break;
        // terminate system call
        case SYSNUM_TERMINATE_PROCESS:
			if((long)SystemCallData->Argument[0] == -2L)
			{
				Z502Halt();
			}
			else if((long)SystemCallData->Argument[0] == -1L)
			{
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
			strncpy(action,"Teminat",8);
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

void frameInit( void )
{
	int i = 0;
	FrmQueue frameQueueCursor, frmQueueNode;
	FRM	*frmNode;
	frmQueue = (FRAME *)malloc(sizeof(FRAME));
	frmQueue->next = NULL;

	frameQueueCursor = frmQueue;
	while (i < (int)PHYS_MEM_PGS)
	{
		frmNode = (FRM *)malloc(sizeof(FRM));
		frmNode->frameID = i;
		frmNode->isAvailable = 1;
		frmQueueNode = (FRAME *)malloc(sizeof(FRAME));
		frmQueueNode->node = frmNode;
		frmQueueNode->next = NULL;

		frameQueueCursor->next = frmQueueNode;
		frameQueueCursor = frameQueueCursor->next;
		i++;
	}
	

}
void diskInit(void)
{
	int i = 1;
	for (i = 1; i < MAX_NUMBER_OF_DISKS; i++)
	{
		diskQueue[i] = (DiskNode *)malloc(sizeof(DiskNode));
		diskQueue[i]->next = NULL;
	}

}
void shadowTableInit( void )
{
	int i = 0;
	for(i = 0; i < 1024; i++)
	{
		SHADOW_TBL[i].sectorID = -1;
		SHADOW_TBL[i].diskID = -1;
	}

}
void osInit( int argc, char *argv[]  ) {
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
	srand(time(NULL));

	frameInit();
	diskInit();
	shadowTableInit();
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

	if(argc > 1 )
	{
		strncpy(test,argv[1],6);
		//strncpy(test,"test1l",6);
	}
	else
	{
		//fgets (test, 20, stdin);
		strncpy(test,"test2b",6);
	}

    /*  Determine if the switch was set, and if so go to demo routine.  */
	/*
    if (strncmp( test, "sample", 6 ) == 0 ) 
	{
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
    }                  
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
		enablePrinter = 0;
		Z502MakeContext( &next_context, (void *)test1m, USER_MODE );
	}
	else
	{
		printf("Illegal Input\n");
		exit(0);
	}
	*/
	// generate current node (now it is the root node)
	
	Z502MakeContext( &next_context, (void *)test2e, USER_MODE );
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

