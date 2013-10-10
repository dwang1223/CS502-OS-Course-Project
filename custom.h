typedef struct
{
	long pid;
	char *name;
	void *context;
	int prior;
} PCB;

typedef struct queue
{
	PCB *pcb;
	struct queue *next;
} *Queue, QUEUE;
