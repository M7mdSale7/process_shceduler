Algorithm algo;
int processesCnt;
int quantum;
int totalExcution = 0;
sigset_t intmask;
PCB* runningP;
FILE *pLog;


//forks process
int forkPrcs(int executionTime);    

//creates log file
void createSchedulerLog();

//writes log file
void writeSchedulerLog(PCB* process, int time, char* state);

//initializes a process entry in PCB
void initializePrcs(PCB* prcs1, const ProcessData* rcvd);

//recieve a process if there is any
void checkRcv();

//Write the final performance file
void writeSchedulerPerf();

//calculate standard deviation
float calculateSD(float data[],int n);

//free the dynamically allocated space
void freeMem();

//finalize data of a terminated process
void finishPrcs();

//stop a process by sending sigstop
void stopPrcs();

//clear resources and exit
void clearExit();

/*----------------------Algorithms----------------------*/
void runAlgo();
void runHPF();
void runSRTN();
void runRR();
/*------------------------------------------------------*/


/*-----------------------handlers-----------------------*/
void handleAlarm(int signum);
void handleUser1(int signum);
void handleSigChild(int signum);
/*------------------------------------------------------*/


/*-------------------------Queue------------------------*/
typedef struct Node
{
    PCB *data;
    struct Node *next;
}Node;
Node *front = NULL, *rear = NULL;
int qSize = 0;  //current number of elements in queue
void enqueue(PCB* data);
PCB* dequeue();
PCB* peak();
void _printQueue(); //internal function for testing purposes
/*------------------------------------------------------*/


/*----------------------deadQueue-----------------------*/
PCB** deadQ;
int dQSize = 0;
/*------------------------------------------------------*/


/*--------------------------PQ--------------------------*/
PCB** prQueue; //shares same functions with Queue
void _heapifySRTN(int index);
void _heapifyHPF(int index);
void _printQ();
void _swap(int ind1, int ind2);
/*------------------------------------------------------*/







/*--------------------------Functions implementation----------------------------*/

void handleAlarm(int signum) //used with RR to wakeup after (time = quantum) has passed
{
    checkRcv(); //so that recieved signals at this time step are placed before the one has just stopped
    
    runningP->remainingTime -= quantum; //decrement remaining time by quantum
    
    if(runningP->remainingTime > 0)
        if(qSize > 0)   //do not stop if it is the only one in Q  
            stopPrcs();
    runAlgo();

}

void handleUser1(int signum) //a process or more have been sent && algorithm is SRTN
{
    checkRcv();
    runAlgo();
}

void handleSigChild(int signum)
{
    alarm(0);   //clear any alarm for RR
    checkRcv();
    if(runningP == NULL)
        return;
    if(runningP->recentStart == getClk())
        sleep(1);
    finishPrcs();
    if(dQSize == processesCnt)
        clearExit();
    runAlgo();
}

void runAlgo()
{
    if(algo == HPF)
        runHPF();
    else if(algo == SRTN)
        runSRTN();
    else if(algo == RR)
        runRR();
}
void runHPF()
{
    signal(SIGUSR1, handleUser1);
    runningP = dequeue();
    if(runningP == NULL)
        return;
    sigignore(SIGUSR1);
    runningP->startTime = getClk();
    runningP->processId =  forkPrcs(runningP->executionTime);
    writeSchedulerLog(runningP, getClk(), "started");
}   

void runSRTN()
{
    if(runningP)
    {
        runningP->remainingTime -= (getClk() - runningP->recentStart);
        if(peak() && (peak()->remainingTime < runningP->remainingTime))
        {
            stopPrcs();
            runningP = dequeue();
        }
        else
        {
            runningP->recentStart = getClk();
            return;
        }
    }
    else
        runningP = dequeue();


    if(runningP == NULL)
        return;


    if(runningP->startTime==-1) //process running for the first time
    {
        runningP->startTime= getClk();
        runningP->processId =  forkPrcs(runningP->executionTime);
        writeSchedulerLog(runningP, getClk(), "started");
    }
    else
    {
        kill(runningP->processId,SIGCONT);
        writeSchedulerLog(runningP, getClk(), "resumed");
    }
    runningP->recentStart = getClk();
}

void runRR()
{
    signal(SIGUSR1, handleUser1); //activating signal sent by process generator on recieving

    if(runningP == NULL)
    {
        runningP = dequeue();
        if(runningP == NULL)
            return;

        if(runningP->startTime==-1) //process running for the first time
        {
            runningP->startTime= getClk();
            runningP->processId =  forkPrcs(runningP->executionTime);
            writeSchedulerLog(runningP, getClk(), "started");
        }
        else //process has run before
        {
            kill(runningP->processId, SIGCONT);
            writeSchedulerLog(runningP, getClk(), "resumed");
        }
        runningP->recentStart = getClk();
    }



    sigignore(SIGUSR1); //ignoring signal sent by process generator on recieving
    alarm(quantum);//wake up after quantum seconds
}

void initializePrcs(PCB* prcs1, const ProcessData* rcvd)
{
    prcs1->id = rcvd->id;
    prcs1->arrivalTime =  rcvd->arrivalTime;
    prcs1->priority = rcvd->priority;
    prcs1->executionTime = rcvd->executionTime;
    prcs1->remainingTime = rcvd->executionTime;
    prcs1->startTime = -1; //meaning that process has never run before
    totalExcution += prcs1->executionTime;
}

void enqueue(PCB* data)
{
    if(algo == RR)
    {
        qSize++;
        Node *newNode = malloc(sizeof(Node));
        newNode->next = NULL;
        newNode->data = data;
        
        //First node to be added
        if(front == NULL && rear == NULL)
        {
            //make both front and rear points to newNode
            front = newNode;
            rear = newNode;
        }
        else //not the first
        {
            //add newNode in rear->next
            rear->next = newNode;

            //make newNode as the rear Node
            rear = newNode;
        }
        return;
    }

    prQueue[++qSize] = data; //ignoring 0 index by post-increment
    if(qSize == 1)
        return;
    if(algo == SRTN)
        for(int i = qSize/2; i > 0; i--)
            _heapifySRTN(i); //heapify from parent
    else if(algo == HPF)
        for(int i = qSize/2; i > 0; i--)
            _heapifyHPF(i); //heapify from parent

}

PCB* dequeue()
{
    if(algo == RR)
    {
        if(front == NULL)
            return NULL;
        if(front == rear)
            rear = NULL;
        Node* tempNode = front;
        PCB* tempData = front->data;
        
        front = front->next;
        free(tempNode);
        qSize--;
        return tempData;
    }

    if(qSize == 0)
        return NULL;
    PCB* prcs = prQueue[1];
    prQueue[1] = prQueue[qSize--];

    if(qSize > 1)
        if(algo == SRTN)
            _heapifySRTN(1);
        else if(algo == HPF)
            _heapifyHPF(1);

    return prcs;

}

int forkPrcs(int executionTime)
{
    pid_t schdPid = getpid();
    pid_t prcsPid = fork();

    if(prcsPid == -1) //error happened when forking
    {
        perror("Error forking process");
        exit(EXIT_FAILURE);
    }
    else if(prcsPid == 0) //execv'ing to process
    {
        char sExecutionTime[5] = {0};
        char sPid[7] = {0};
        char sClk[7] = {0};
        sprintf(sPid, "%d", schdPid);
        sprintf(sExecutionTime, "%d", executionTime);
        sprintf(sClk, "%d", getClk());
        char *const paramList[] = {"./process.out", sExecutionTime, sPid, sClk,NULL};
        execv("./process.out", paramList);
        
        //if it executes what is under this line, then execv has failed
        perror("Error in execv'ing to clock");
        exit(EXIT_FAILURE);
    }
    return prcsPid;
}

void writeSchedulerLog(PCB* process, int time, char* state)
{
    int waitingTime = time-process->arrivalTime-(process->executionTime-process->remainingTime);
    if(state == "finished")
        fprintf(pLog, "At time %-2d process %-2d %-8s arr %-2d total %-2d remain %-2d wait %-2d TA %-2d WTA %.2f\n"
         ,time , process->id, state, process->arrivalTime, process->executionTime, process->remainingTime, waitingTime,
         process->TA, process->WTA);
    else
        fprintf(pLog, "At time %-2d process %-2d %-8s arr %-2d total %-2d remain %-2d wait %-2d\n"
         ,time , process->id, state, process->arrivalTime, process->executionTime, process->remainingTime, waitingTime);
         

}

void createSchedulerLog()
{
    pLog = fopen("Scheduler.log.txt", "w");
    if (pLog ==NULL)
    {
        perror("Error creating/ openning Scheduler.log!");
        exit(EXIT_FAILURE);
    }
    fprintf(pLog, "#At time x process y state arr w total z remain y wait k\n");
    fclose(pLog);
    pLog = fopen("Scheduler.log.txt", "a");
}

void finishPrcs()
{
    deadQ[dQSize++] = runningP;
    runningP->finishTime = getClk();
    runningP->remainingTime = 0;
    runningP->waitingTime=runningP->finishTime-runningP->arrivalTime-runningP->executionTime;
    runningP->TA = runningP->finishTime - runningP->arrivalTime;
    runningP->WTA = runningP->TA / (float)runningP->executionTime;
    runningP->WTA = round(runningP->WTA * 100)/100;

    writeSchedulerLog(runningP, getClk(), "finished");
    runningP = NULL;
}
void stopPrcs()
{
    if(kill(runningP->processId, SIGSTOP) == -1)
    {
        perror("ERROR IN STP");
        exit(EXIT_FAILURE);
    }
    writeSchedulerLog(runningP, getClk(), "stopped");
    enqueue(runningP);
    runningP = NULL;
}

void checkRcv()
{
    ProcessData rcvd;
    PCB* prcs;
    while (rcvPrcs(&rcvd) != -1)
    {
        prcs = (PCB*) malloc(sizeof(PCB));
        initializePrcs(prcs, &rcvd);
        enqueue(prcs);
    }
}

void clearExit()
{
    writeSchedulerPerf();   //out the Perf file

    fclose(pLog);   //close file stream

    freeMem();  //free dynamically allocated mem

    destroyClk(false);  //upon termination release the clock resources

    exit(EXIT_SUCCESS);
}

void _heapifyHPF(int index)
{
    int least = index, left = 2 * index, right = left + 1;

    if((left <= qSize) && (prQueue[left]->priority < prQueue[least]->priority))
		least = left;
	
	if((right <= qSize) && (prQueue[right]->priority < prQueue[least]->priority))
		least = right;

	if(least == index)
		return;

	_swap(least, index);
	_heapifyHPF(least);
}

void _heapifySRTN(int index)
{
    int least = index, left = 2 * index, right = left + 1;

    if((left <= qSize) && (prQueue[left]->remainingTime < prQueue[least]->remainingTime))
		least = left;
	
	if((right <= qSize) && (prQueue[right]->remainingTime < prQueue[least]->remainingTime))
		least = right;

	if(least == index)
		return;
    
	_swap(least, index);
	_heapifySRTN(least);
}
void _swap(int ind1, int ind2)
{
    PCB* tmp = prQueue[ind1];
    prQueue[ind1] = prQueue[ind2];
    prQueue[ind2] = tmp;
}

PCB* peak()
{
    if(qSize == 0)
        return NULL;
    return prQueue[1];
}

void writeSchedulerPerf()
{
    FILE *pPerf;
    PCB* finishedP = NULL;
    pPerf = fopen("Scheduler.perf.txt", "w");
    if (pPerf ==NULL)
    {
        perror("Error creating/ openning Scheduler.perf!");
        exit(EXIT_FAILURE);
    }
    int utilization=( 1.0 * totalExcution / (getClk() - 1)) * 100;
    float avgWTA =0;
    float avgWaiting=0;
    int TotalWaiting=0;
    float stdWTA=0;
    float TA=0;
    float WTA =0;
    float excTime = 0;
    float* arrWTA= malloc(sizeof(float)*processesCnt);

    for(int i = 0 ; i < processesCnt ; i++)
    {
        finishedP = deadQ[i];
         if(finishedP == NULL)
            break;

         TotalWaiting = TotalWaiting + finishedP->waitingTime;
         TA = TA + finishedP->TA;
         excTime = excTime + finishedP->executionTime;
         WTA = WTA + finishedP->WTA;
         arrWTA[i]= finishedP->WTA;
    }

    avgWaiting = (float)(TotalWaiting)/processesCnt;
    avgWaiting = round(avgWaiting*100)/100;

    avgWTA = WTA / processesCnt;
    avgWTA = round(avgWTA*100)/100;

    stdWTA = calculateSD(arrWTA,processesCnt);
    stdWTA = round(stdWTA*100)/100;

    fprintf(pPerf, "CPU utilization =  %d\n",utilization);
    fprintf(pPerf, "Avg WTA = %.2f \n",avgWTA);
    fprintf(pPerf, "Avg Waiting = %.2f\n",avgWaiting);
    fprintf(pPerf, "Std WTA = %.2f\n", stdWTA);
    fclose(pPerf);
}

float calculateSD(float data[],int n)
{
    float sum = 0.0, mean, SD = 0.0;
    int i;
    for (i = 0; i < n; ++i)
    {
        sum += data[i];
    }
    mean = sum / n;
    for (i = 0; i < n; ++i)
    {
        SD += ((data[i] - mean) * (data[i] - mean));
    }
    return sqrt(SD / (n-1));
}

void freeMem()
{
    for(int i = 0; i < processesCnt; i++)
        free(deadQ[i]);
    if(algo != RR)
        free(prQueue);
}