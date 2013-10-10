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
