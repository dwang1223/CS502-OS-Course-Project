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
************************************************************************/

#include			 "malloc.h"
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             "custom.h"

//#include             "z502.h"
#define         ILLEGAL_PRIORITY                -3
#define         NAME_DUPLICATED					-4

int dispatcher();

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

// test whether my struct declare is available
Queue totalQueue;
Queue timerQueue;
Queue readyQueue;
static long increamentPID = 1; //store the maximum pid for all process
PCB *pcb;
static int currentCountOfProcess = 0;
static PCB *currentPCBNode;

int start_timer(long *sleep_time)
{
	Queue timerQueueCursor,nodeTmp;
	timerQueueCursor = timerQueue;
	while(timerQueueCursor->next != NULL)
	{
		timerQueueCursor = timerQueueCursor->next;
	}
	nodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	nodeTmp->node = currentPCBNode;
	nodeTmp->next = NULL;
	timerQueueCursor->next = nodeTmp;

	MEM_WRITE(Z502TimerStart, sleep_time);
	dispatcher();
	return 0;
}

int dispatcher()
{
	while(readyQueue->next == NULL)
	{
		currentPCBNode = NULL;
		Z502Idle();
	}
	currentPCBNode = readyQueue->next->node;
	//pop up the first node from readyQueue
	readyQueue->next = readyQueue->next->next;

	Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(currentPCBNode->context) );
}
long get_pid_by_name(char *name)
{
    Queue readyQueueCursor;
	if(strcmp(name, "") == 0)
	{
		return currentPCBNode->pid;
	}
    readyQueueCursor = readyQueue;

    while(readyQueueCursor->next != NULL)
    {
        readyQueueCursor = readyQueueCursor->next;
        if(strcmp(readyQueueCursor->node->name,name) == 0)
        {
            return readyQueueCursor->node->pid;
        }
        
    }   
    return NO_PCB_NODE_FOUND;
}

PCB * PCB_item_generator(SYSTEM_CALL_DATA *SystemCallData)
{
	void *next_context;
	pcb = (PCB*)malloc(sizeof(PCB));
	strcpy(pcb->name, (char*)SystemCallData->Argument[0]);
	CALL(Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE ));
	pcb->context = next_context;
	pcb->prior = (int)SystemCallData->Argument[2];
	return pcb;
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
		printf("\tILLEGAL_PRIORITY\n");
		return ILLEGAL_PRIORITY;
	}
	// duplicate name check
	readyQueueCursor = readyQueue;
	while(readyQueueCursor->next != NULL)
	{
		readyQueueCursor = readyQueueCursor->next;
		if(strcmp(readyQueueCursor->node->name,pcbNode->name) == 0)
		{
			printf("\tNAME_DUPLICATED\n");
			return NAME_DUPLICATED;
		}
		
	}
	//Since everything is OK, now, we can append this node to the readyQueue
	readyNodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	totalNodeTmp = (QUEUE *)malloc(sizeof(QUEUE));
	pcbNode->pid = increamentPID++;
	readyNodeTmp->node = pcbNode;
	totalNodeTmp->node = pcbNode;
	readyNodeTmp->next = NULL;
	totalNodeTmp->next = NULL;
	readyQueueCursor->next = readyNodeTmp;

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

int process_teminator_by_pid(long pID)
{
	Queue tmpQueueCursor;
	Queue readyQueueCursor = readyQueue;

	while(readyQueueCursor->next != NULL)
	{
		tmpQueueCursor = readyQueueCursor;
		readyQueueCursor = readyQueueCursor->next;
		if(readyQueueCursor->node->pid == pID)
		{
			printf("\PID is found!\n");
			tmpQueueCursor->next = readyQueueCursor->next;
			// free the node, which has been teminated from & removed from the Queue
			free(readyQueueCursor);
			return SUCCESS;
		}
	}
	printf("\PID is not found!\n");
	return FAIL;
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
    static BOOL        remove_this_in_your_code = TRUE;   /** TEMP **/
    static INT32       how_many_interrupt_entries = 0;    /** TEMP **/
	Queue queueCursor;
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
    
	//add the first node from timerQueue to the end of readyQueue
	if(timerQueue->next != NULL)
	{
		queueCursor = readyQueue;
		while(queueCursor->next != NULL)
		{
			queueCursor = queueCursor->next;
		}
		queueCursor->next = timerQueue->next;
		timerQueue->next = timerQueue->next->next;
		queueCursor->next->next = NULL;
	}
	
	
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
	INT32				Time;
	INT32				Temp;
	PCB					*pcb;
	static long			pid;
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
		    CALL( MEM_READ( Z502ClockStatus, &Time ) );
		    printf("time = %ld\n",Time);
            *(INT32 *)SystemCallData->Argument[0] = Time;
            break;
		
		//SLEEP CALL
		case SYSNUM_SLEEP:
			start_timer(&(INT32*)SystemCallData->Argument[0]);
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
        // terminate system call
        case SYSNUM_TERMINATE_PROCESS:
			//printf("PID = %ld\n", (long)SystemCallData->Argument[0]);
			process_teminator_by_pid((long)SystemCallData->Argument[0]);
			if((long)SystemCallData->Argument[0] < 0)
			{
				Z502Halt();
			}
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
	PCB *rootPCB;
	rootPCB = (PCB*)malloc(sizeof(PCB));
	totalQueue = (QUEUE *)malloc(sizeof(QUEUE));
	readyQueue = (QUEUE *)malloc(sizeof(QUEUE));
	timerQueue = (QUEUE *)malloc(sizeof(QUEUE));
	totalQueue->next = NULL;
	readyQueue->next = NULL;
	timerQueue->next = NULL;

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

    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (( argc > 1 ) && ( strcmp( argv[1], "sample" ) == 0 ) ) {
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
    }                   /* This routine should never return!!           */

    /*  This should be done by a "os_make_process" routine, so that
        test0 runs on a process recognized by the operating system.    */
    Z502MakeContext( &next_context, (void *)test1c, USER_MODE );

	// generate current node (now it is the root node)
	rootPCB->pid = ROOT_PID;
	strcpy(rootPCB->name, ROOT_PNAME);
	rootPCB->context = next_context;
	rootPCB->prior = ROOT_PRIOR;
	totalQueue->node = rootPCB;
	currentPCBNode = rootPCB;


    Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
	
	
}                                               // End of osInit

