typedef struct
{
	long pid;
	char name[140];
	void *context;
	int prior;
	INT32	wakeUpTime;
} PCB;

typedef struct queue
{
	PCB *node;
	struct queue *next;
} *Queue, QUEUE;

#define			MAX_COUNT_OF_PROCESSES			25
#define			ROOT_PID						0L
#define			ROOT_PRIOR						0
#define			ROOT_PNAME						"I am ROOT"
#define         SUCCESS							1
#define         FAIL							0
#define         NO_PCB_NODE_FOUND              -10L

#define                  NUM_RAND_BUCKETS          128
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE