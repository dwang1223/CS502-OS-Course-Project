typedef struct
{
	long pid;
	char name[140];
	void *context;
	int prior;
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