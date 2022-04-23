#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h> 
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <getopt.h>

/* constants */
#define maxProcesses 18
#define maxResources 20

/* structure for resources */
typedef struct {
    int requestArr[maxProcesses];
    int allocatedArr[maxProcesses];
    int releasedArr[maxProcesses];
    int instanceCounter;
    int freeInstances;

} resourcesStruct;

/* structure for shared memory */
typedef struct {
    int sleepingProcs[maxProcesses];
    int finishedProcs[maxProcesses];
    int currentProcPID[maxProcesses];
    int needed[maxProcesses];
    bool blockedProcs[maxProcesses];
    unsigned int seconds;
    unsigned int nanoSeconds;

    resourcesStruct resAllocated[maxResources];

} sharedMemStruct;

unsigned long addNS(unsigned long);

int checkIndex(int);

bool systemClock();
bool timeFunc();

void logOutput(char *);
void signalSem(int);
void resRelease();
void catchDeadlock();
void forkFunc();
void programSummary();
void signalHandler();
void procIsDone();
void setSignals();
void resAllocate();
void zombieKiller(int);
void currentResources();
void waitSem(int);
void cleanOSS();
void alarmCaught();
void controlC();
