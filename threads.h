/*
 * threads.h
 *
 *  Created on: Mar 23, 2021
 *      Author: enemi
 */

#ifndef THREADS_H_
#define THREADS_H_

#define BALLFIFO 0



/*
 * Reads accelerometer values and saves them to globals
 */
void readAccelerometer(void);

/*
 * Waits for screen to be tapped then attempts to add a ball thread
 */
void waitForTap(void);

/*
 * Funtion that represents a ball
 */
void ball(void);

/*
 * Idle thread that runs when others do not
 */
void idle(void);

/*
 * ISR for a touch on the Touch Pad
 */
void LCD_Tap(void);

#endif /* THREADS_H_ */
