/*
 * G8RTOS_Scheduler.c
 */

/*********************************************** Dependencies and Externs *************************************************************/

#include "msp.h"
#include "BSP.h"
#include "G8RTOS.h"
#include <string.h>
#include <stdio.h>

/*
 * G8RTOS_Start exists in asm
 */
extern void G8RTOS_Start();

/* System Core Clock From system_msp432p401r.c */
extern uint32_t SystemCoreClock;

/*
 * Pointer to the currently running Thread Control Block
 */
extern tcb_t *CurrentlyRunningThread;

/*********************************************** Dependencies and Externs *************************************************************/


/*********************************************** Defines ******************************************************************************/

/* Status Register with the Thumb-bit Set */
#define THUMBBIT 0x01000000

/*********************************************** Defines ******************************************************************************/


/*********************************************** Data Structures Used *****************************************************************/

/* Thread Control Blocks
 *	- An array of thread control blocks to hold pertinent information for each thread
 */
static tcb_t threadControlBlocks[MAX_THREADS];

/* Thread Stacks
 *	- An array of arrays that will act as individual stacks for each thread
 */
static int32_t threadStacks[MAX_THREADS][STACKSIZE];

/* Periodic Event Threads
 * - An array of periodic events to hold pertinent information for each thread
 */
static ptcb_t Pthread[MAXPTHREADS];

/*********************************************** Data Structures Used *****************************************************************/


/*********************************************** Private Variables ********************************************************************/

/*
 * Current Number of Threads currently in the scheduler
 */
static uint32_t NumberOfThreads;

/*
 * Current Number of Periodic Threads currently in the scheduler
 */
static uint32_t NumberOfPthreads;

/*
 * ID for thread
 */
static uint16_t IDCounter;
/*********************************************** Private Variables ********************************************************************/


/*********************************************** Private Functions ********************************************************************/

/*
 * Initializes the Systick and Systick Interrupt
 * The Systick interrupt will be responsible for starting a context switch between threads
 * Param "numCycles": Number of cycles for each systick interrupt
 */
static void InitSysTick(uint32_t numCycles)
{
    SysTick_Config(numCycles); //Configures SysTick overflow by amount of cycles
}


void G8RTOS_Scheduler()
{
    //Prospective thread and setting lowest priority
    tcb_t *tempNextThread = CurrentlyRunningThread->next;
    uint16_t currentMaxPriority = 256;

    //Iterates through all threads
    for(uint8_t i = 0; i < NumberOfThreads; ++i)
    {
        //If currently running thread is not blocked AND not asleep, then
        if((!tempNextThread->blocked) && (!tempNextThread->asleep))
        {
            //If possible next thread has a higher priority than highest priority
            if(tempNextThread->priority < currentMaxPriority)
            {
                CurrentlyRunningThread = tempNextThread;
                currentMaxPriority = CurrentlyRunningThread->priority;
            }
        }
        //Switches possible next thread to next
        tempNextThread = tempNextThread->next;
    }
}


/*
 * SysTick Handler
 * The Systick Handler now will increment the system time,
 * set the PendSV flag to start the scheduler,
 * and be responsible for handling sleeping and periodic threads
 */
void SysTick_Handler()
{
    //Increments system time
    SystemTime++;

    //Checks periodic threads to see if they are to be dealt with
    for(uint8_t i = 0; i < NumberOfPthreads; ++i)
    {
        //If execute time is equal to system time
        if(Pthread[i].executeTime == SystemTime)
        {
            //Defines new execute time to be "period" ms from now
            Pthread[i].executeTime = Pthread[i].period + SystemTime + Pthread[i].currentTime;

            //Runs Periodic thread
            (*(Pthread[i].Handler))();
        }
    }

    //Wakes up threads that need to be woken up
    tcb_t *temp = CurrentlyRunningThread;
    for(uint8_t i = 0; i < NumberOfThreads; ++i)
    {
        //If thread is asleep
        if(temp->asleep)
        {
            //If thread reaches wake up time, then wake it up threadControlBlocks[i].sleepCount == SystemTime
            if(temp->sleepCount <= SystemTime)
            {
                //Thread woken up and sleep count made 0
                temp->asleep = false;
                temp->sleepCount = 0;
            }
        }
        //Points to the next thread
        temp = temp->next;
    }

    //Sets PendSV flag
    SCB->ICSR |= (1<<28);
}

/*
 * Finds the current time for a periodic thread to not have muliples of periods
 */
static uint32_t findCurrTime(uint32_t period)
{
    //If first thread being added
    if(NumberOfPthreads == 0)
    {
        return 0;
    }
    uint32_t curr = 0;
    int i;
    for(i = 0; i < NumberOfPthreads; ++i)
    {
        if(((Pthread[i].period % period) == 0 ) || ((period % Pthread[i].period) == 0))
        {
            return 1;
        }
    }
    return curr;
}
/*********************************************** Private Functions ********************************************************************/


/*********************************************** Public Variables *********************************************************************/

/* Holds the current time for the whole System */
uint32_t SystemTime;

/*********************************************** Public Variables *********************************************************************/


/*********************************************** Public Functions *********************************************************************/

/*
 * Sets variables to an initial state (system time and number of threads)
 * Enables board for highest speed clock and disables watchdog
 */
void G8RTOS_Init()
{
    //Sets system time to 0
    SystemTime = 0;

    //Sets number of threads to 0
    NumberOfThreads = 0;

    //Sets number of periodic threads to 0
    NumberOfPthreads = 0;

    //Sets ID to 0
    IDCounter = 0;

    //Initializes board
    BSP_InitBoard();

    //Relocates the ISR interrupt vector table to 0x20000000
    uint32_t newVTORTable = 0x20000000;
    memcpy((uint32_t *)newVTORTable, (uint32_t *)SCB->VTOR, 57*4); // 57 interrupt vectors to copy
    SCB->VTOR = newVTORTable;
}

/*
 * Starts G8RTOS Scheduler
 * 	- Initializes the Systick
 * 	- Sets Context to thread with highest priority
 * Returns: Error Code for starting scheduler. This will only return if the scheduler fails
 */
int G8RTOS_Launch()
{
    //Sets currently running thread to first tcb
    CurrentlyRunningThread = &threadControlBlocks[0];

    //Makes currently running thread to thread with highest priority
    for(uint32_t i = 1; i < NumberOfThreads; ++i)
    {
        if(threadControlBlocks[i].priority < CurrentlyRunningThread->priority)
        {
            CurrentlyRunningThread = &threadControlBlocks[i];
        }
    }

    //Gets clock frequency
    uint32_t clkFreq = ClockSys_GetSysFreq();

    //Initializes SysTick
    InitSysTick((clkFreq/1000));

    //Sets priorities for PENDSV and SysTick
    NVIC_SetPriority(PendSV_IRQn, 7);

    //Call G8RTOS_Start
    G8RTOS_Start();

    return ERROR;
}

/*
 * Funtion that finds the closest left index
 * param: current index
 * return: closest left index
 */
static uint32_t findLeftNode(uint16_t index)
{
    for(uint32_t i = index - 1; i <= MAX_THREADS - 1; --i)
    {
        //Looks for left neighbor
        if(threadControlBlocks[i].isAlive)
        {
            return i;
        }

        //If index 0, point to rightmost index
        if(!i)
        {
            i = MAX_THREADS;
        }
    }
    return MAX_THREADS + 1;
}

/*
 * Funtion that finds the closest right index
 * param: current index
 * return: closest right index
 */
static uint32_t findRightNode(uint16_t index)
{
    //If index is last and index 0 is alive, return 0
    if(index == 22)
    {
        if(threadControlBlocks[0].isAlive)
        {
            return 0;
        }
        index = 0;
    }

    for(uint32_t i = index + 1; i <= MAX_THREADS - 1; ++i)
    {
        //Looks for right neighbor
        if(threadControlBlocks[i].isAlive)
        {
            return i;
        }

        //If index 0, point to leftmost index
        if(i == MAX_THREADS - 1)
        {
            //Looks for right neighbor
            if(threadControlBlocks[0].isAlive)
            {
                return 0;
            }
            i = 0;
        }
    }
    return MAX_THREADS + 1;
}


/*
 * Adds threads to G8RTOS Scheduler
 * 	- Checks if there are stil available threads to insert to scheduler
 * 	- Initializes the thread control block for the provided thread
 * 	- Initializes the stack for the provided thread to hold a "fake context"
 * 	- Sets stack tcb stack pointer to top of thread stack
 * 	- Sets up the next and previous tcb pointers in a round robin fashion
 * Param "threadToAdd": Void-Void Function to add as preemptable main thread
 * Param "priority": specified priority of new thread
 * Param "name": Name given to thread
 * Returns: Error code for adding threads
 */
int G8RTOS_AddThread(void (*threadToAdd)(void), uint8_t priority, char* name)
{
    int32_t priMask = StartCriticalSection();

    //If number of threads is less than max threads
    if(NumberOfThreads < MAX_THREADS)
    {
        //Looks for the first dead thread
        uint32_t index;
        for(index = 0; index < MAX_THREADS; ++index)
        {
            if(!threadControlBlocks[index].isAlive)
            {
                threadControlBlocks[index].threadID =
                        ((IDCounter++) << 16) | (uint32_t)&threadControlBlocks[index];
                break;
            }
        }

        //When no threads exist
        if (NumberOfThreads == 0)
        {
            //Makes new thread point to itself
            threadControlBlocks[index].prev = &threadControlBlocks[index];
            threadControlBlocks[index].next = &threadControlBlocks[index];
        }
        else
        {
            //Finds closest left and right nodes
            uint32_t leftIndex = findLeftNode(index);
            uint32_t rightIndex = findRightNode(index);

            //Inserts into linked list
            threadControlBlocks[index].prev = &threadControlBlocks[leftIndex];
            threadControlBlocks[index].next = &threadControlBlocks[rightIndex];


            threadControlBlocks[leftIndex].next = &threadControlBlocks[index];
            threadControlBlocks[rightIndex].prev = &threadControlBlocks[index];

        }
        //Makes thread start awake
        threadControlBlocks[index].asleep = 0;

        //Makes thread sleep count equal 0
        threadControlBlocks[index].sleepCount = 0;

        //Makes blocked semaphore 0
        threadControlBlocks[index].blocked = 0;

        //Initializes priority
        threadControlBlocks[index].priority = priority;

        //Initializes alive status
        threadControlBlocks[index].isAlive = true;

        //Sets name to be assigned for tcb
        strcpy(threadControlBlocks[index].threadName, name);

        //Sets thumbbit in xPSR
        threadStacks[index][STACKSIZE - 1] = THUMBBIT;

        //Sets PC to function pointer
        threadStacks[index][STACKSIZE - 2] = (uint32_t)threadToAdd;

        //Fills R0 to R14 with dummy values
        for (uint8_t i = 3; i <= 16; i++)
            threadStacks[index][STACKSIZE - i] = 1;

        //Sets sp pointer to point to top of stack pointer address
        threadControlBlocks[index].sp = &threadStacks[index][STACKSIZE - 16];

        //Increments number of threads
        NumberOfThreads++;

        EndCriticalSection(priMask);

        return SUCCESS;
    }
    else
    {
        EndCriticalSection(priMask);
        return ERROR;
    }
}


/*
 * Adds periodic threads to G8RTOS Scheduler
 * Function will initialize a periodic event struct to represent event.
 * The struct will be added to a linked list of periodic events
 * Param Pthread To Add: void-void function for P thread handler
 * Param period: period of P thread to add
 * Returns: Error code for adding threads
 */
int G8RTOS_AddPeriodicEvent(void (*PthreadToAdd)(void), uint32_t period)
{
    int32_t priMask = StartCriticalSection();

    //If number of periodic threads is not equal to maximum, then
    if(NumberOfPthreads != MAXPTHREADS)
    {
        //If number of periodic threads
        if(NumberOfPthreads == 0)
        {
            //Adds a new periodic thread to point to itself
            Pthread[NumberOfPthreads].Handler = PthreadToAdd;
            Pthread[NumberOfPthreads].period = period;
            Pthread[NumberOfPthreads].currentTime = findCurrTime(period);
            Pthread[NumberOfPthreads].executeTime = period + NumberOfPthreads;
            Pthread[NumberOfPthreads].next = &Pthread[NumberOfPthreads];
            Pthread[NumberOfPthreads].prev = &Pthread[NumberOfPthreads];
        }
        else
        {
            //Adds a new periodic thread to point to itself
            Pthread[NumberOfPthreads].Handler = PthreadToAdd;
            Pthread[NumberOfPthreads].period = period;
            Pthread[NumberOfPthreads].currentTime = NumberOfPthreads;
            Pthread[NumberOfPthreads].executeTime = period + NumberOfPthreads;

            //Makes new periodic thread point to previous and head
            Pthread[NumberOfPthreads].prev = &Pthread[NumberOfPthreads - 1];
            Pthread[NumberOfPthreads].next = &Pthread[0];

            //Makes previous root and head point to new periodic thread
            Pthread[NumberOfPthreads - 1].next = &Pthread[NumberOfPthreads];
            Pthread[0].prev = &Pthread[NumberOfPthreads];
        }
        NumberOfPthreads++;
        EndCriticalSection(priMask);
        return SUCCESS;
    }
    EndCriticalSection(priMask);
    //Returns 0 if more threads cannot be added
    return THREAD_LIMIT_REACHED;
}


/*
 * Puts the current thread into a sleep state.
 *  param durationMS: Duration of sleep time in ms
 */
void G8RTOS_Sleep(uint32_t durationMS)
{
    //Initializes currently running threads sleep count
    CurrentlyRunningThread->sleepCount = SystemTime + durationMS;

    //Puts thread to sleep
    CurrentlyRunningThread->asleep = true;

    //Sets PendSV flag, to yield CPU
    SCB->ICSR |= (1<<28);
}

/*
 * Returns the currently running threads ID
 */
threadID_t G8RTOS_GetThreadID(void)
{
    return CurrentlyRunningThread->threadID;
}

/*
 * Initializes aperiodic registers
 * param: Funtion pointer to serve as ISR
 * param: Priority of thread
 * param: IRQ interrupt number
 */
sched_ErrCode_t G8RTOS_AddAPeriodicEvent(void (*AthreadToAdd)(void), uint8_t priority,  IRQn_Type IRQn)
{
    int32_t priMask = StartCriticalSection();

    //IF IRQn is less priority than last exception and greater priority than PORT6_IRQn
    if((IRQn >PSS_IRQn) && (IRQn < PORT6_IRQn))
    {
        //If priority is lower than 6
        if(priority >= 6)
        {
            __NVIC_SetVector(IRQn, (uint32_t)AthreadToAdd);
            __NVIC_SetPriority(IRQn, priority);
            __NVIC_EnableIRQ(IRQn);
            return NO_ERROR;
        }
        EndCriticalSection(priMask);
        return HWI_PRIORITY_INVALID;
    }
    EndCriticalSection(priMask);
    return IRQn_INVALID;
}

/*
 * Takes in a threadID for the thread to be Killed
 *  param: ID of thread to be killed
 */
sched_ErrCode_t G8RTOS_KillThread(threadID_t threadID)
{
    int32_t priMask = StartCriticalSection();

    //Checks if only one thread running
    if(NumberOfThreads == 1)
    {
        EndCriticalSection(priMask);
        return CANNOT_KILL_LAST_THREAD;
    }

    for(uint32_t i = 0; i < MAX_THREADS; ++i)
    {
        //If thread ID matches the one searched
        if(threadID == threadControlBlocks[i].threadID)
        {
            //Kill thread
            threadControlBlocks[i].isAlive = false;

            uint32_t leftIndex = findLeftNode(i);
            uint32_t rightIndex = findRightNode(i);

            threadControlBlocks[leftIndex].next = &threadControlBlocks[rightIndex];
            threadControlBlocks[rightIndex].prev = &threadControlBlocks[leftIndex];

            //Decreases number of new threads
            NumberOfThreads--;

            //If killed thread is currently running thread, then
            if(&threadControlBlocks[i] == CurrentlyRunningThread)
            {
                EndCriticalSection(priMask);
                //Sets PendSV flag, to yield CPU
                SCB->ICSR |= (1<<28);
            }

            EndCriticalSection(priMask);
            return NO_ERROR;
        }
    }

    EndCriticalSection(priMask);
    return THREAD_DOES_NOT_EXIST;
}

/*
 * Kills currently running thread
 */
sched_ErrCode_t G8RTOS_KillSelf(void)
{
    int32_t priMask = StartCriticalSection();

    //Checks if only one thread running
    if(NumberOfThreads == 1)
    {
        EndCriticalSection(priMask);
        return CANNOT_KILL_LAST_THREAD;
    }

    //Kills thread
    CurrentlyRunningThread->isAlive = false;

    //Sets previous thread's next to current's next
    CurrentlyRunningThread->prev->next = CurrentlyRunningThread->next;

    //Sets next thread's previous to current's previous
    CurrentlyRunningThread->next->prev = CurrentlyRunningThread->prev;

    //Decreases number of new threads
    NumberOfThreads--;

    EndCriticalSection(priMask);

    return NO_ERROR;
}
/*********************************************** Public Functions *********************************************************************/
