#include "msp.h"
#include "LCDLib.h"
#include "BSP.h"
#include <G8RTOS.h>
#include "threads.h"

void main(void)
{
    //Initialize G8RTOS
    G8RTOS_Init();

    //Initialize LCD_Tap as new Handler
    G8RTOS_AddAPeriodicEvent(LCD_Tap, 125, PORT4_IRQn);

    //Initialize LCD and TP
    LCD_Init(true);

    //Create ball FIFO
    G8RTOS_InitFIFO(BALLFIFO);

    //Initializing Semaphores
    G8RTOS_InitSemaphore(&sensorMutex, 1);
    G8RTOS_InitSemaphore(&LCDMutex, 1);

    //Creating threads
    char name2[] = "READ";
    G8RTOS_AddThread(readAccelerometer, 125, name2);
    char name1[] = "WAIT";
    G8RTOS_AddThread(waitForTap, 125, name1);
    char name3[] = "IDLE";
    G8RTOS_AddThread(idle, 255, name3);

    //Start GatorOS
    G8RTOS_Launch();
}
