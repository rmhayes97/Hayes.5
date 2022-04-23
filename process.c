#include "config.h"

int currentPid = 0;
int pIndex = 0;
int currentIndex = 0;
int randResource = 0; 
int randInst = 0;
int shmID = 0;
int semID = 0;
unsigned long startingNS;
unsigned long startingSec;
unsigned long waitingNS;
unsigned long waitingSec;
char sBuffer[150];
sharedMemStruct* shmPTR = NULL;


void setSignals() {
    signal(SIGALRM, cleanOSS);
    signal(SIGINT, cleanOSS);
    signal(SIGSEGV, cleanOSS);
    signal(SIGKILL, cleanOSS);
}

/* checks to put in wating second or nanosecond  */
bool systemClock() {
    if (shmPTR->seconds == waitingSec) {
        if (shmPTR->nanoSeconds > waitingNS) {
            return true;
        }
    }
    else if (shmPTR->seconds > waitingSec) {
        return true;
    }
   
    return false;
}

/* cleans shared memory */
void cleanOSS() {
    shmdt(shmPTR);
}

/* checks where a process is indexed */
int checkIndex(int pid) {
    for (int i = 0; i < maxProcesses; i++) {
        if (pid == shmPTR->currentProcPID[i]) {
            return i;
        }
    }
    return -1;
}

/* semaphore wait */
void waitSem(int semWaitID) {
    struct sembuf semaphore;
    semaphore.sem_num = 0;
    semaphore.sem_op = -1;
    semaphore.sem_flg = 0;
    semop(semWaitID, &semaphore, 1);
}

/* semaphore signal */
void signalSem(int semSigID) {
    struct sembuf semaphore;
    semaphore.sem_op = 1;
    semaphore.sem_flg = 0;
    semaphore.sem_num = 0;
    semop(semSigID, &semaphore, 1);
}

int main(int argc, char *argv[]) {
    key_t semKey;
    key_t shmKey;
    currentPid = getpid();
    srand(currentPid * 10);
    setSignals();
    int random_time = rand() % 2500000000;

    if ((semKey = ftok("oss.c", 'a')) == (key_t) -1) {
        perror("IPC error: ftok"); 
        exit(1);
    }
    if ((shmKey = ftok("process.c", 'a')) == (key_t) -1) {
        perror("IPC error: ftok"); 
        exit(1);
    }

    if((semID = semget(semKey, 2, S_IRUSR | S_IWUSR | IPC_CREAT | 0666)) == -1) {
        perror("process.c: semget");
        cleanOSS();
        exit(0);
    }

    if((shmID = shmget(shmKey, (sizeof(resourcesStruct) * maxProcesses) + sizeof(sharedMemStruct), IPC_CREAT|0666)) == -1) {
        perror("process.c: shmget");
        cleanOSS();
        exit(1);
    }

    if((shmPTR = (sharedMemStruct*)shmat(shmID, 0, 0)) == (sharedMemStruct*)-1) {
        perror("process.c: shmat");
        cleanOSS();
        exit(1);
    }
    currentIndex = checkIndex(currentPid);

    shmPTR->finishedProcs[currentIndex] = 0;
    shmPTR->sleepingProcs[currentIndex] = 0;
    shmPTR->needed[currentIndex] = -1;
    shmPTR->blockedProcs[currentIndex] = false;

    for (pIndex = 0; pIndex < maxResources; pIndex++) {
        shmPTR->resAllocated[pIndex].requestArr[currentIndex] = 0;
        shmPTR->resAllocated[pIndex].releasedArr[currentIndex] = 0; 
        shmPTR->resAllocated[pIndex].allocatedArr[currentIndex] = 0;
    }

    waitSem(1);
    signalSem(1);

    startingNS = shmPTR->nanoSeconds;
    startingSec = shmPTR->seconds;
    waitingNS = startingNS;
    waitingSec = startingSec;
    waitingNS += random_time;

    if (waitingNS > 1000000000) {
        waitingNS -= 1000000000;
        waitingSec += 1;
    }

    while(shmPTR->finishedProcs[currentIndex] != 1) {

        while(1) {
            if (systemClock()) {
                break;
            }
        }
        if (shmPTR->blockedProcs[currentIndex] == true || shmPTR->sleepingProcs[currentIndex] == 1) {

        }
        else {
            if (shmPTR->seconds - startingSec > 2) {
                if (shmPTR->nanoSeconds > startingNS ) {
                    if (rand() % 100 > 20) {
                        waitSem(0);
                        shmPTR->finishedProcs[currentIndex] = 2;
                        signalSem(0);
                        break;
                    }
                }
            }
            waitSem(0);
            randResource = rand() % maxResources;
            
            if (shmPTR->resAllocated[randResource].allocatedArr[currentIndex] >= 1) {

                if (rand() % 100 > 50) {
                    shmPTR->blockedProcs[currentIndex] = true;
                    shmPTR->resAllocated[randResource].releasedArr[currentIndex] = shmPTR->resAllocated[randResource].allocatedArr[currentIndex];
                    
                }
            }
            else {

                if (rand() % 100 < 50) {
                    randInst = 1 + (rand() % shmPTR->resAllocated[randResource].instanceCounter);
                    if (randInst > 0) {
                        shmPTR->resAllocated[randResource].requestArr[currentIndex] = randInst;
                        shmPTR->blockedProcs[currentIndex] = true;
                    }
                }
            }
            waitSem(1);
            signalSem(0);
            

            random_time = rand() % 250000000;
            waitingNS = shmPTR->nanoSeconds;
            waitingNS += random_time;
            if (waitingNS >= 1000000000) {
                waitingNS -= 1000000000;
                waitingSec += 1;
            }
            signalSem(1);
        }
    }

    cleanOSS();
    return 0;
}
