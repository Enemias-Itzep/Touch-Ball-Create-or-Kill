#include "RGBLeds.h"
#include <DriverLib.h>
#include "msp.h"

/*
 * LP3943_ColorSet
 * This function will set the frequencies and PWM duty cycle
 * for each register of the specified unit.
 */
static void LP3943_ColorSet(uint32_t unit, uint32_t PWM_DATA){
    return;
}

/*
 * LP3943_LedModeSet
 * This function will set each of the LEDs to the desired operating
 * mode. The operation modes are on, off, PWM1 and PWM2.
 */
void LP3943_LedModeSet(uint32_t unit, uint16_t LED_DATA){
    /*
     * Unit to be written
     * 0x00 | Red
     * 0x01 | Blue
     * 0x02 | Green
     *
     * Registers to be written
     * LS0 | LED0-3 Selector
     * LS1 | LED4-7 Selector
     * LS2 | LED8-11 Selector
     * LS3 | LED12-16 Selector
     */

    uint8_t LS0 = ((LED_DATA & 0x1000) >> 6) | //LED3
            ((LED_DATA & 0x2000) >> 9) |
            ((LED_DATA & 0x4000) >> 12) |
            ((LED_DATA & 0x8000) >> 15); //LED0

    uint8_t LS1 = ((LED_DATA & 0x0100) >> 2) |
            ((LED_DATA & 0x0200) >> 5) |
            ((LED_DATA & 0x0400) >> 8) |
            ((LED_DATA & 0x0800) >> 11); //LED4

    uint8_t LS2 = ((LED_DATA & 0x0010) << 2) |
            ((LED_DATA & 0x0020) >> 1) |
            ((LED_DATA & 0x0040) >> 4) |
            ((LED_DATA & 0x0080) >> 7); //LED8

    uint8_t LS3 = ((LED_DATA & 0x0001) << 6) |
            ((LED_DATA & 0x0002) << 3) |
            ((LED_DATA & 0x0004) << 0) |
            ((LED_DATA & 0x0008) >> 3); //LED12

    // Writes slave address to register + offset for LP
    UCB2I2CSA = 0x60 | unit; //eUSCI_Bx I2C Slave Address Register

    // Enables start bit and loops until its on
    UCB2CTLW0 |= UCTXSTT;
    while(UCB2CTLW0 & UCTXSTT);
    while (!(UCB2IFG & UCTXIFG0));

    //Selects register to be written and enables auto increment
    UCB2TXBUF = (0b00010000 | 0x06);
    while (!(UCB2IFG & UCTXIFG0));

    /* Sends data to registers, and polls to see when finished */
    UCB2TXBUF = LS0;
    while((UCB2IFG & UCTXIFG0) == 0);

    UCB2TXBUF = LS1;
    while((UCB2IFG & UCTXIFG0) == 0);

    UCB2TXBUF = LS2;
    while((UCB2IFG & UCTXIFG0) == 0);

    UCB2TXBUF = LS3;
    while((UCB2IFG & UCTXIFG0) == 0);

    //Enables stop bit
    UCB2CTLW0 |= UCTXSTP;
}

/*
 * Performs necessary initializations for RGB LEDs
 */
void init_RGBLEDS(){
    uint16_t UNIT_OFF = 0x0000;

    // Software reset enable
    UCB2CTLW0 = UCSWRST; //0x0001

    // Initialize I2C master
    // Set as master, I2C mode, Clock sync, SMCLK source, Transmitter
    UCB2CTLW0 |= (UCMST | UCMODE_3 | UCSYNC | UCSSEL_3 | UCTR);

    // Set the Fclk as 400kHz
    // Presumes that the SMCLK is selected as source and Fsmclk is 12MHz..
    UCB2BRW = 30;

    // In conjunction with the next line, this sets the pins as I2C mode.
    // (Table found on p160 of SLAS826E)
    // Set P3.6 as UCB2_SDA and 3.7 as UCB2_SLC
    P3SEL0 |= 0xC0;
    P3SEL1 &= ~0xC0;

    // Bitwise anding of all bits except UCSWRST
    UCB2CTLW0 &= (~UCSWRST);

    //Turns of all LEDs
    LP3943_LedModeSet(RED, UNIT_OFF);
    LP3943_LedModeSet(GREEN, UNIT_OFF);
    LP3943_LedModeSet(BLUE, UNIT_OFF);
}
