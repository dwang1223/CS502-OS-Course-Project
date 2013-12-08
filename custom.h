#define			MAX_COUNT_OF_PROCESSES			25
#define			ROOT_PID						0L
#define			ROOT_PRIOR						20
#define			ROOT_PNAME						"I am ROOT"
#define         SUCCESS							1
#define         FAIL							0
#define         NO_PCB_NODE_FOUND              -10L
#define         ADD_BY_END							1
#define         ADD_BY_PRIOR						2
#define         MAX_LEGAL_PID						10L
#define         MAX_LEGAL_PRIOR						99
#define         MAX_MSG_LENGTH						100
#define         MAX_MSG_COUNT						37

#define         ILLEGAL_PID							-1
#define         ALREADY_SUSPENDED					-2
#define         PCB_NOT_SUSPENDED					-3
#define         SLEF_SUSPENDED_ERR					-4
#define         PID_NOT_FOUND						-5
#define         TOO_SMALL_BUF_SIZE					-6
#define         NO_MSG_FOUND						-7


#define         ILLEGAL_PRIOR						-1

#define                  NUM_RAND_BUCKETS          128
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE

#define					DISK_READ						0
#define					DISK_WRITE						1

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

typedef struct
{
	long sid;
	long tid;
	INT32 length;
	char msg[MAX_MSG_LENGTH];
} MSG;

typedef struct message
{
	MSG *node;
	struct message *next;
} *MsgQueue, MESSAGE;

typedef struct
{
	long frameID;
	long pageID;
	long pid;
	int isAvailable;
} FRM;

typedef struct frame
{
	FRM *node;
	struct frame *next;
} *FrmQueue, FRAME;


typedef struct disk
{
	int readOrWrite;
	long diskID;    // diskID may not be used here
	long sectorID;
	PCB *PCB;
	//char buffer[PGSIZE];
	struct disk *next;
} *DiskQueue, DiskNode;
