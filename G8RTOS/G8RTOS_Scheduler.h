/*
 * G8RTOS_Scheduler.h
 */
#ifndef G8RTOS_SCHEDULER_H_
#define G8RTOS_SCHEDULER_H_

/*********************************************** Sizes and Limits *********************************************************************/
#define MAX_THREADS 23
#define MAXPTHREADS 6
#define MAXBALLS 20
#define STACKSIZE 512
#define OSINT_PRIORITY 7
/*********************************************** Sizes and Limits *********************************************************************/

/*********************************************** Public Variables *********************************************************************/

/* Holds the current time for the whole System */
extern uint32_t SystemTime;

/*********************************************** Public Variables *********************************************************************/
typedef uint32_t threadID_t;

/*
 * Error Codes for Scheduler
 */
typedef enum
{
    NO_ERROR                  = 0,
    THREAD_LIMIT_REACHED      = -1,
    NO_THREADS_SCHEDULED      = -2,
    THREADS_INCORRECTLY_ALIVE = -3,
    THREAD_DOES_NOT_EXIST     = -4,
    CANNOT_KILL_LAST_THREAD   = -5,
    IRQn_INVALID              = -6,
    HWI_PRIORITY_INVALID      = -7
}sched_ErrCode_t;
/*********************************************** Public Functions *********************************************************************/

/*
 * Initializes variables and hardware for G8RTOS usage
 */
void G8RTOS_Init();

/*
 * Starts G8RTOS Scheduler
 * 	- Initializes Systick Timer
 * 	- Sets Context to first thread
 * Returns: Error Code for starting scheduler. This will only return if the scheduler fails
 */
int32_t G8RTOS_Launch();

/*
 * Adds threads to G8RTOS Scheduler
 * 	- Checks if there are stil available threads to insert to scheduler
 * 	- Initializes the thread control block for the provided thread
 * 	- Initializes the stack for the provided thread
 * 	- Sets up the next and previous tcb pointers in a round robin fashion
 * Param "threadToAdd": Void-Void Function to add as preemptable main thread
 * Returns: Error code for adding threads
 */
int32_t G8RTOS_AddThread(void (*threadToAdd)(void), uint8_t priority, char* name);

/*
 * Adds periodic threads to G8RTOS Scheduler
 * Function will initialize a periodic event struct to represent event.
 * The struct will be added to a linked list of periodic events
 * Param Pthread To Add: void-void function for P thread handler
 * Param period: period of P thread to add
 * Returns: Error code for adding threads
 */
int G8RTOS_AddPeriodicEvent(void (*PthreadToAdd)(void), uint32_t period);

/*
 * Puts the current thread into a sleep state.
 *  param durationMS: Duration of sleep time in ms
 */
void G8RTOS_Sleep(uint32_t durationMS);

/*
 * Returns currently running threads ID
 */
threadID_t G8RTOS_GetThreadID(void);

/*
 * Initializes aperiodic registers
 * param: Funtion pointer to serve as ISR
 * param: Priority of thread
 * param: IRQ interrupt number
 *
 * return: Returns error code
 */
sched_ErrCode_t G8RTOS_AddAPeriodicEvent(void (*AthreadToAdd)(void), uint8_t priority,  IRQn_Type IRQn);

/*
 * Takes in a threadID for the thread to be Killed
 *  param: ID of thread to be killed
 *
 *  return: Returns error code
 */
sched_ErrCode_t G8RTOS_KillThread(threadID_t threadID);

/*
 * Kills currently running thread
 *
 * return: Returns error code
 */
sched_ErrCode_t G8RTOS_KillSelf(void);

/*********************************************** Public Functions *********************************************************************/

#endif /* G8RTOS_SCHEDULER_H_ */
