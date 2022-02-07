/*
 * G8RTOS_IPC.c
 *
 *  Created on: Jan 10, 2017
 *      Author: Daniel Gonzalez
 */
#include <stdint.h>
#include "msp.h"
#include "BSP.h"
#include "G8RTOS_IPC.h"
#include "G8RTOS_Semaphores.h"

/*********************************************** Defines ******************************************************************************/



/*********************************************** Defines ******************************************************************************/


/*********************************************** Data Structures Used *****************************************************************/

typedef struct FIFO_t
{
    int32_t buffer[FIFOSIZE];
    int32_t *head; //Pointer to the head of the FIFO, oldest spot
    int32_t *tail; //Pointer to the tail of the FIFO, next empty spot
    uint32_t lostData; //Amount of lost data
    semaphore_t currentSize; //Semaphore to act as indicator for size
    semaphore_t mutex; //Semaphore to indicate if FIFO is in use

}FIFO_t;

/* Array of FIFOS */
static FIFO_t FIFOs[4];

/*********************************************** Data Structures Used *****************************************************************/

/*
 * Initializes FIFO Struct
 */
int G8RTOS_InitFIFO(uint32_t FIFOIndex)
{
    if(FIFOIndex < MAX_NUMBER_OF_FIFOS){
        //Initializes heads and tails
        FIFOs[FIFOIndex].head = &FIFOs[FIFOIndex].buffer[0];
        FIFOs[FIFOIndex].tail = &FIFOs[FIFOIndex].buffer[0];

        FIFOs[FIFOIndex].lostData = 0;

        //Current size semaphore set to 0
        G8RTOS_InitSemaphore(&FIFOs[FIFOIndex].currentSize, 0);
        //FIFO binary semaphore set to 1
        G8RTOS_InitSemaphore(&FIFOs[FIFOIndex].mutex, 1);

        return SUCCESS;
    }
    return ERROR;
}

/*
 * Reads FIFO
 *  - Waits until CurrentSize semaphore is greater than zero
 *  - Gets data and increments the head ptr (wraps if necessary)
 * Param: "FIFOChoice": chooses which buffer we want to read from
 * Returns: uint32_t Data from FIFO
 */
uint32_t readFIFO(uint32_t FIFOChoice)
{
    //Wait before reading FIFO in case of being read in another thread
    G8RTOS_WaitSemaphore(&FIFOs[FIFOChoice].mutex);

    //Wait before reading FIFO in case of empty FIFO
    G8RTOS_WaitSemaphore(&FIFOs[FIFOChoice].currentSize);

    //Read Thread
    uint32_t data = *(FIFOs[FIFOChoice].head);

    //If head is at last index in array, then
    if(FIFOs[FIFOChoice].head == &FIFOs[FIFOChoice].buffer[FIFOSIZE - 1])
    {
        //Moves head to beginning
        FIFOs[FIFOChoice].head = &FIFOs[FIFOChoice].buffer[0];
    }
    else
    {
        //Increment head pointer
        FIFOs[FIFOChoice].head++;
    }

    //Release semaphore to indicate its ready to use
    G8RTOS_SignalSemaphore(&FIFOs[FIFOChoice].mutex);

    return data;
}

/*
 * Writes to FIFO
 *  Writes data to Tail of the buffer if the buffer is not full
 *  Increments tail (wraps if ncessary)
 *  Param "FIFOChoice": chooses which buffer we want to read from
 *        "Data': Data being put into FIFO
 *  Returns: error code for full buffer if unable to write
 */
int writeFIFO(uint32_t FIFOChoice, uint32_t Data)
{
    //If FIFO is full, then
    if(FIFOs[FIFOChoice].currentSize > FIFOSIZE - 1)
    {
        //Increments lost data because it will not be saved
        FIFOs[FIFOChoice].lostData++;

        return ERROR;
    }
    else
    {
        //Writes data in FIFO
        *(FIFOs[FIFOChoice].tail) = Data;

        //If tail pointer is pointing to last index, then
        if(FIFOs[FIFOChoice].tail == &FIFOs[FIFOChoice].buffer[FIFOSIZE-1])
        {
            //Moves head to beginning
            FIFOs[FIFOChoice].tail = &FIFOs[FIFOChoice].buffer[0];
        }
        else
        {
            //Moves head to beginning
            FIFOs[FIFOChoice].tail++;
        }
    }

    //Signal current size semaphore
    G8RTOS_SignalSemaphore(&FIFOs[FIFOChoice].currentSize);

    return SUCCESS;
}

