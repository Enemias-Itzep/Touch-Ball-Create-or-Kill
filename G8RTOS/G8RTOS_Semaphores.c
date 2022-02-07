/*
 * G8RTOS_Semaphores.c
 */

/*********************************************** Dependencies and Externs *************************************************************/

#include "msp.h"
#include "G8RTOS.h"

/*********************************************** Dependencies and Externs *************************************************************/

extern tcb_t * CurrentlyRunningThread;
/*********************************************** Public Functions *********************************************************************/

/*
 * Initializes a semaphore to a given value
 * Param "s": Pointer to semaphore
 * Param "value": Value to initialize semaphore to
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_InitSemaphore(semaphore_t *s, int32_t value)
{
    //Disables interrupts
    int32_t priMask = StartCriticalSection();

    //Initialize semaphore
    *s = value;

    //Enable interrupts
    EndCriticalSection(priMask);
}

/*
 * No longer waits for semaphore
 *  - Decrements semaphore
 *  - Blocks thread is semaphore is unavailable
 * Param "s": Pointer to semaphore to wait on
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_WaitSemaphore(semaphore_t *s)
{
    //Disable Interrupts
    int32_t priMask = StartCriticalSection();

    //Decrement Semaphore since it is available
    (*s)--;

    //If the semaphore is less than zero, then thread is blocked since it is unavailable
    if((*s) < 0)
    {
        //Block current thread
        CurrentlyRunningThread->blocked = s;

        //Enable Interrupts
        EndCriticalSection(priMask);

        //Sets PendSV flag, to yield CPU
        SCB->ICSR |= (1<<28);
    }
    else
    {
        //Enable Interrupts
        EndCriticalSection(priMask);
    }
}

/*
 * Signals the completion of the usage of a semaphore
 *  - Increments the semaphore value by 1
 *  - Unblocks any threads waiting on that semaphore
 * Param "s": Pointer to semaphore to be signaled
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_SignalSemaphore(semaphore_t *s)
{
    //Disables interrupts
    int32_t priMask = StartCriticalSection();

    //Increment semaphore, to make it available
    (*s)++;

    /*
     * IF the semaphore is less than or equal to zero, then
     * that means other threads are waiting on the semaphore.
     * Other threads must be unblocked before proceeding.
     */
    if((*s) <= 0)
    {
        //Creates pointer to next thread
        tcb_t *pt = CurrentlyRunningThread->next;

        //While pointer is is not blocked, change thread to next
        while(pt->blocked != s)
        {
            pt = pt->next;
        }

        //Once next block thread is found, it is unblocked
        pt->blocked = 0;
    }

    //Enables interrupts
    EndCriticalSection(priMask);
}

/*********************************************** Public Functions *********************************************************************/
