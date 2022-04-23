#include "config.h"


char sBuffer[150];
FILE* logPTR = NULL;
sharedMemStruct* shmPTR = NULL;

/* variables for time */
unsigned long nanoSecondsElapsed = 0;
unsigned long currentResTime = 0;
unsigned long tfSeconds = 0;
unsigned long tfNanSeconds = 0;
unsigned long catchDeadlockTime = 0;

/* variables for counting*/ 
int pNumber = 0;
int logLineCounter = 0;
int forkCounter = 0;
int pCounter = 0;
int grantedCounter = 0;
int instantAllocated = 0;
int procsThatWaited = 0;
int deadlockKillCounter = 0;
int finishedProcs = 0;
int deadlockFuncRunCounter = 0;
int shmID = 0;
int semID = 0;
int timerCounter = 0;

/* for terminating the program after 10000 lines */
void logOutput(char * string) { 
    logLineCounter++;
    if (logLineCounter <= 10000) {
        fputs(string, logPTR);
    }
    else {
        fputs("Over 10,000 lines, terminating program.\n", logPTR);
        printf("Over 10,000 lines, terminating program.\n");
        cleanOSS();
    }
    
}

/* for forking, terminates program if forked over 40 times */
void forkFunc() {
    int forkIndex = 0;
    int forkPid = 0;
    unsigned long nsForkTime = 0;
    if (forkCounter > 39) {
        printf("oss.c: 40 children forked, terminating\n");
        logOutput("oss.c: 40 children forked, terminating\n");
        cleanOSS();
    }

    for (forkIndex = 0; forkIndex < maxProcesses; forkIndex++) {
        if(shmPTR->currentProcPID[forkIndex] == 0) {
            forkPid = fork();
            pCounter++;
            forkCounter++;
            if(forkPid == 0) {
                execl("./process", "./process", NULL);
            } else {
                shmPTR->currentProcPID[forkIndex] = forkPid;
                tfNanSeconds = shmPTR->nanoSeconds + (rand() % 500000000);
                nsForkTime = tfNanSeconds;
                if (nsForkTime >= 1000000000) {
                    tfSeconds += 1;
                    tfNanSeconds -= 1000000000;
                }
                return;
            }
        }
    }
}

/* cleans up shared memory */
void cleanOSS() {
    currentResources();
    programSummary();
    fclose(logPTR);
    sleep(1);
    shmdt(shmPTR);
    shmctl(shmID, IPC_RMID, NULL);
    semctl(semID, 0, IPC_RMID, NULL);
    exit(0);
}

/* semaphore signal */
void signalSem(int sem_id) {

    struct sembuf sem;
    sem.sem_num = 0;
    sem.sem_op = 1;
    sem.sem_flg = 0;
    semop(sem_id, &sem, 1);
}

/* semaphore wait */
void waitSem(int sem_id) {

    struct sembuf sem;
    sem.sem_num = 0;
    sem.sem_op = -1;
    sem.sem_flg = 0;
    semop(sem_id, &sem, 1);
}

/* for putting time into seconds or nanoseconds */
bool timeFunc() {


    if(tfSeconds < shmPTR->seconds) {
        return true;

    } else if(tfSeconds == shmPTR->seconds) {
        if(tfNanSeconds <= shmPTR->nanoSeconds) {
            return true;
        }

    } else {

        return false;
    }
}

/* signals */
void setSignals() {
    signal(SIGINT, controlC);

    signal(SIGALRM, alarmCaught);

    signal(SIGKILL, cleanOSS);
}

/* when a signal is triggered */
void alarmCaught() {
    printf("\nProgram ran out of time. See ya!\n");
    cleanOSS();
}

void controlC() {
    printf("\nProgram killed by ctrl+c.\n");
    cleanOSS();
}


/* resource table */
void currentResources() {
    int i = 0;
    fputs("\nResource Allocation:\n\n", logPTR);
    fputs("    P0  P1  P2  P3  P4  P5  P6  P7  P8  P9  P10 P11 P12 P13 P14 P15 P16 P17\n", logPTR);
    for (i = 0; i < maxResources; i++) {
        if (i > 9) {
            sprintf(sBuffer, "R%d  ", i);
        } else {
            sprintf(sBuffer, "R%d   ", i);
        }
        
        fputs(sBuffer, logPTR);
        for (int j = 0; j < maxProcesses; j++) {
            sprintf(sBuffer,"%d   ", shmPTR->resAllocated[i].allocatedArr[j]);
            fputs(sBuffer, logPTR);
        }
        fputs("\n", logPTR);
    }
}

/* stats of the program */
void programSummary() {
    double percent = 0;
    fputs("\nProgram Summary:\n", logPTR);
    sprintf(sBuffer, "Processes immediately granted resources: %d\n", instantAllocated);
    fputs(sBuffer, logPTR);

    sprintf(sBuffer, "Processes that waited to be granted resources: %d\n", procsThatWaited);
    fputs(sBuffer, logPTR);

    sprintf(sBuffer, "Processes terminated successfully: %d\n", finishedProcs);
    fputs(sBuffer, logPTR);

    percent = (double)deadlockKillCounter / (double)deadlockFuncRunCounter;
    percent *= 100;
    sprintf(sBuffer, "Deadlock detection caught deadlocks %.8f percent of times ran\n", percent);
    fputs(sBuffer, logPTR);
}

void zombieKiller(int sig) {
    pid_t kidPID;
    while ((kidPID = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {

    }
}

/* deadlock detection */
void catchDeadlock() {
    int res = 0;
    deadlockFuncRunCounter++;
    for (int i = 0; i < maxProcesses; i++) {
        /* if process is sleeping */
        if (shmPTR->sleepingProcs[i] == 1) { 
            res = shmPTR->needed[i];
            while(1) {
                for (int j = 0; j < maxProcesses; j++) {
                    if (i != j) {
                        if (shmPTR->resAllocated[res].allocatedArr[j] > 0 && shmPTR->sleepingProcs[j] == 1) {

                            sprintf(sBuffer, "Master has caught deadlock, killing Process P%d to free R%d for P%d\n", j, res, i);
                            logOutput(sBuffer);
                                   
                            /* freeing up resources */
                            shmPTR->resAllocated[res].freeInstances += shmPTR->resAllocated[res].allocatedArr[j];
                            shmPTR->resAllocated[res].releasedArr[j] = 0;
                            shmPTR->resAllocated[res].requestArr[j] = 0;
                            shmPTR->resAllocated[res].allocatedArr[j] = 0;
                            
                            shmPTR->currentProcPID[j] = 0;
                            shmPTR->finishedProcs[j] = 1;
                            pCounter--;
                            deadlockKillCounter++;

                            /* check for deadlock fixed */
                            if (shmPTR->resAllocated[res].requestArr[i] <= shmPTR->resAllocated[res].freeInstances) {
                                shmPTR->resAllocated[res].freeInstances -= shmPTR->resAllocated[res].requestArr[i];
                                shmPTR->resAllocated[res].allocatedArr[i] += shmPTR->resAllocated[res].requestArr[i];
                                shmPTR->resAllocated[res].requestArr[i] = 0;

                                sprintf(sBuffer, "Master granting P%d request R%d at time %d:%d\n", i, res, shmPTR->seconds/100, shmPTR->nanoSeconds);
                                logOutput(sBuffer);
                                procsThatWaited++;

                                shmPTR->sleepingProcs[i] = 0;
                                shmPTR->blockedProcs[i] = false;
                                return;
                            }
                        }
                    }
                }
                break;
            }
        }
    }
}

/* for allocating resources */
void resAllocate() {
    for (int i = 0; i < maxProcesses; i++) {
        if (shmPTR->sleepingProcs[i] == 0) {
            for (int j = 0; j < maxResources; j++) {
                if (shmPTR->resAllocated[j].requestArr[i] > 0) {
                    sprintf(sBuffer, "Master has detected Process P%d requesting R%d at time %d:%d \n", i, j, shmPTR->seconds/100, shmPTR->nanoSeconds);
                    logOutput(sBuffer);

                    if (shmPTR->resAllocated[j].requestArr[i] <= shmPTR->resAllocated[j].freeInstances) {
                        shmPTR->resAllocated[j].freeInstances -= shmPTR->resAllocated[j].requestArr[i];
                        shmPTR->resAllocated[j].allocatedArr[i] = shmPTR->resAllocated[j].requestArr[i];
                        shmPTR->resAllocated[j].requestArr[i] = 0;
                        shmPTR->sleepingProcs[i] = 0;
                        shmPTR->blockedProcs[i] = false;
                        instantAllocated++;
                        grantedCounter += shmPTR->resAllocated[j].allocatedArr[i];
                        sprintf(sBuffer, "Process P%d granted access to R%d at time %d:%d\n", i, j, shmPTR->seconds/100, shmPTR->nanoSeconds);
                        logOutput(sBuffer);
                    }
                    else {
                        sprintf(sBuffer, "Process P%d put to sleep at time %d:%d. Too few instances of R%d\n", i, shmPTR->seconds/100, shmPTR->nanoSeconds, j);
                        logOutput(sBuffer);
                        shmPTR->needed[i] = j;
                        shmPTR->sleepingProcs[i] = 1;
                    }
                }
            }
        }
    }
}

/* looking for process that were able to be completed */
void procIsDone() {
    for (int i = 0; i < maxProcesses; i++) {
            if (shmPTR->finishedProcs[i] == 2) {
                finishedProcs++;
                sprintf(sBuffer, "Master has acknowledged Process P%d releasing resources at time %d:%d\n", i, shmPTR->seconds/100, shmPTR->nanoSeconds);
                logOutput(sBuffer);
                shmPTR->currentProcPID[i] = 0;
                for (int j = 0; j < maxResources; j++) {
                    if (shmPTR->resAllocated[j].allocatedArr[i] > 0) {
                        shmPTR->resAllocated[j].freeInstances += shmPTR->resAllocated[j].allocatedArr[i];
                    }
                }
            }
        }
}

/* looking for resources that need to be released and releasing them */
void resRelease() {
    for (int i = 0; i < maxProcesses; i++) {
        for (int j = 0; j < maxResources; j++) {
            if (shmPTR->resAllocated[j].releasedArr[i] > 0) {
                sprintf(sBuffer, "Master has acknowledged Process P%d releasing R%d at time %d:%d\n", i, j, shmPTR->seconds/100, shmPTR->nanoSeconds);
                logOutput(sBuffer);
                shmPTR->resAllocated[j].releasedArr[i] = 0;
                shmPTR->resAllocated[j].freeInstances += shmPTR->resAllocated[j].allocatedArr[i];
                shmPTR->resAllocated[j].allocatedArr[i] -= shmPTR->resAllocated[j].allocatedArr[i];
                shmPTR->blockedProcs[i] = false;
                pCounter--;
                finishedProcs++;
            }
        }
    }
}

/* tracks number of instances */
void instanceFunc() {
    int i = 0;
    int j = 0;
    for (i = 0; i < maxResources; i++) {
        shmPTR->resAllocated[i].instanceCounter = 1 + (rand() % 10);
        shmPTR->resAllocated[i].freeInstances = shmPTR->resAllocated[i].instanceCounter;
        for (j = 0; j < maxProcesses; j++) {
            shmPTR->resAllocated[i].allocatedArr[j] = 0;
            shmPTR->resAllocated[i].releasedArr[j] = 0;
            shmPTR->resAllocated[i].requestArr[j] = 0;
        }
    }
}

unsigned long addNS(unsigned long nanoseconds) {
    nanoseconds = 1 + (rand() % 100000000);

    return nanoseconds;
} 

int main(int argc, char *argv[]) {
    key_t semKey;
    key_t shmKey;
    setSignals();

    logPTR = fopen("output", "w");
    struct sigaction sAction;
    memset(&sAction, 0, sizeof(sAction));
    sAction.sa_handler = zombieKiller;
    sigaction(SIGCHLD, &sAction, NULL);

  
    if ((semKey = ftok("oss.c", 'a')) == (key_t) -1) {
        perror("IPC error: ftok"); 
        exit(1);
    }
    if ((shmKey = ftok("process.c", 'a')) == (key_t) -1) {
        perror("IPC error: ftok"); 
        exit(1);
    }


    if((semID = semget(semKey, 2, IPC_CREAT | 0666)) == -1) {
        perror("oss.c: semget");
        cleanOSS();
    }
    semctl(semID, 0, SETVAL, 1);
    semctl(semID, 1, SETVAL, 1);

    if((shmID = shmget(shmKey, (sizeof(resourcesStruct) * maxProcesses) + sizeof(sharedMemStruct), IPC_CREAT | 0666)) == -1) {
        perror("oss.c: shmget");
        cleanOSS();
    }

    if((shmPTR = (sharedMemStruct*)shmat(shmID, NULL, 0)) == (sharedMemStruct*)-1) {
        perror("oss.c: shmat");
        cleanOSS();
    }

    instanceFunc();

    shmPTR->seconds = 0;
    shmPTR->nanoSeconds = 0;
    alarm(5);

    for (int i = 0; i < maxProcesses; i++) {
        shmPTR->currentProcPID[i] = 0;
    }

    while(1) {
        waitSem(0);

        if(pCounter == 0) {
            forkFunc();
        } else if(pCounter < maxProcesses) {
            if(timeFunc()) {
                forkFunc();
            }
        }

        if(pCounter > 0) {
            procIsDone();
            resRelease();
            resAllocate();
            catchDeadlock();
            
        }
        
        signalSem(0);
        waitSem(1);
        nanoSecondsElapsed = addNS(nanoSecondsElapsed);

        shmPTR->nanoSeconds += nanoSecondsElapsed;

        if (shmPTR->nanoSeconds >= 1000000000) {
            shmPTR->seconds += 1;
            shmPTR->nanoSeconds -= 1000000000;
        }

        signalSem(1);
        if (shmPTR->seconds - currentResTime >= 100) {
            currentResources();
            currentResTime = shmPTR->seconds;
        }
    }

    return 0;
}
