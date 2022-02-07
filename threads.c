/*
 * threads.c
 *
 *  Created on: Mar 23, 2021
 *      Author: enemi
 */
#include "G8RTOS.h"
#include <DriverLib.h>
#include "BSP.h"
#include "LCDLib.h"
#include <time.h>
#include <stdlib.h>
#include "threads.h"


#define BALLSIDE 5
#define HITBOX 20

/*
 * Global values for accelerometer
 */
int16_t accelX;
int16_t accelY;

volatile uint16_t NumberOfBalls = 0; //Holds number of balls
volatile bool touch = false; //If screen is pressed

/*
 * Holds all balls
 */
static ball_t balls[MAXBALLS];


/*
 * Reads accelerometer values and saves them to globals
 */
void readAccelerometer(void)
{
    while(true)
    {
        //Waits for sensor I2C semaphore
        G8RTOS_WaitSemaphore(&sensorMutex);

        //Reads accelerometer
        bmi160_read_accel_x(&accelX);

        //Releases Sensor
        G8RTOS_SignalSemaphore(&sensorMutex);


        //Waits for sensor I2C semaphore
        G8RTOS_WaitSemaphore(&sensorMutex);

        //Reads accelerometer
        bmi160_read_accel_y(&accelY);

        //Negative to account for sensor orientation
        accelY *= -1;

        //Releases Sensor
        G8RTOS_SignalSemaphore(&sensorMutex);

        //Sleeps 100ms
        G8RTOS_Sleep(100);
    }
}

/*
 * Waits for screen to be tapped then attempts to add a ball thread
 */
void waitForTap(void)
{
    //Random seed
    srand(time(NULL));

    //Char array with thread name
    char name[] = "BALL";
    while(1)
    {
        //IF screen is pressed
        if(touch)
        {
            //Turns off flag
            touch = false;

            //Reads coordinates
            Point p = TP_ReadXY();

            //Holds if touch was on a ball
            bool ballTouch = false;

            for (uint16_t i = 0; i < MAXBALLS; ++i)
            {
                //If touch was on a ball
                if (p.x > balls[i].xPos - HITBOX && p.x < balls[i].xPos + BALLSIDE + HITBOX &&
                    p.y > balls[i].yPos - HITBOX && p.y < balls[i].yPos + BALLSIDE + HITBOX &&
                    balls[i].alive)
                {
                    //Touch was on a ball
                    ballTouch = true;

                    //Waits for LCD semaphore in case that thread to be deleted is using it
                    G8RTOS_WaitSemaphore(&LCDMutex);
                    uint8_t code = G8RTOS_KillThread(balls[i].threadID);
                    G8RTOS_SignalSemaphore(&LCDMutex);

                    //If removal was successful then decrement balls and kill it
                    if(!code)
                    {
                        balls[i].alive = false;
                        NumberOfBalls--;
                    }

                    //Draws rectangle
                    G8RTOS_WaitSemaphore(&LCDMutex);
                    LCD_DrawRectangle(balls[i].xPos,
                                      balls[i].xPos + BALLSIDE,
                                      balls[i].yPos,
                                      balls[i].yPos + BALLSIDE,
                                      LCD_BLACK);
                    G8RTOS_SignalSemaphore(&LCDMutex);
                    break;
                }
            }

            //IF touch was not on a ball and on the screen
            if(!ballTouch && (p.x <= MAX_SCREEN_X) && (p.y <= MAX_SCREEN_Y))
            {
                //If adding thread was a success
                if(!G8RTOS_AddThread(ball, 125, name))
                {
                    //Writes the FIFO coordinates to array and increments number
                    writeFIFO(BALLFIFO, p.x);
                    writeFIFO(BALLFIFO, p.y);
                    NumberOfBalls++;
                }
            }
        }
        G8RTOS_Sleep(500);
    }
}

/*
 * Funtion that represents a ball
 */
void ball(void)
{
    //Iterates through ball threads and finds the first dead thread
    uint8_t index;
    for(index = 0; index < MAXBALLS; index++)
    {
        if(!balls[index].alive)
        {
            balls[index].alive = true;
            break;
        }
    }

    //Instantiates new ball to add
    balls[index].xPos = readFIFO(BALLFIFO);
    balls[index].yPos = readFIFO(BALLFIFO);
    balls[index].xVel = (rand() % 10) - 5;
    balls[index].yVel = (rand() % 10) - 5;
    balls[index].color = rand() % 65536;
    balls[index].threadID = G8RTOS_GetThreadID();

    while(1)
    {
        //Saves old coordinates
        int16_t xTemp = balls[index].xPos;
        int16_t yTemp = balls[index].yPos;

        //Finds new position for the ball
        balls[index].xPos += ((accelX/2000) + balls[index].xVel);
        balls[index].yPos += ((accelY/2000) + balls[index].yVel);

        //When out of bounds it wraps around
        if(balls[index].xPos > 320)
        {
            balls[index].xPos = 0;
        }
        else if (balls[index].xPos < 0)
        {
            balls[index].xPos = 320;
        }

        //When out of bounds it wraps around
        if(balls[index].yPos > 240)
        {
            balls[index].yPos = 0;
        }
        else if (balls[index].yPos < 0)
        {
            balls[index].yPos = 240;
        }

        //Erases old rectangle and draws new rectangle
        G8RTOS_WaitSemaphore(&LCDMutex);
        LCD_DrawRectangle(xTemp,
                          xTemp + BALLSIDE,
                          yTemp,
                          yTemp + BALLSIDE,
                          LCD_BLACK);
        LCD_DrawRectangle(balls[index].xPos,
                          balls[index].xPos + BALLSIDE,
                          balls[index].yPos,
                          balls[index].yPos + BALLSIDE,
                          balls[index].color);
        G8RTOS_SignalSemaphore(&LCDMutex);

        G8RTOS_Sleep(30);
    }
}

/*
 * Idle thread that runs when others do not
 */
void idle(void)
{
    while(1) {}
}

/*
 * ISR for a touch on the Touch Pad
 */
void LCD_Tap(void)
{
    //Clear IFG flag
    P4->IFG &= ~BIT0;

    touch = true;
}
