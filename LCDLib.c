/*
 * LCDLib.c
 *
 *  Created on: Mar 2, 2017
 *      Author: Danny
 */

#include "msp.h"
#include <DriverLib.h>
#include <LCDLib.h>
#include "AsciiLib.h"
#include "msp432.h"
#include "G8RTOS.h"


static const eUSCI_SPI_MasterConfig spiConfigLCD =
{
    EUSCI_SPI_CLOCKSOURCE_SMCLK,
    12000000,//uint32_t clockSourceFrequency;
    12000000,//uint32_t desiredSpiClock;
    EUSCI_SPI_MSB_FIRST,
    EUSCI_SPI_PHASE_DATA_CHANGED_ONFIRST_CAPTURED_ON_NEXT,
    EUSCI_SPI_CLOCKPOLARITY_INACTIVITY_HIGH,
    EUSCI_SPI_3PIN
};

static const eUSCI_SPI_MasterConfig spiConfigTP =
{
    EUSCI_SPI_CLOCKSOURCE_SMCLK,
    12000000,//uint32_t clockSourceFrequency;
    1200000,//uint32_t desiredSpiClock;
    EUSCI_SPI_MSB_FIRST,
    EUSCI_SPI_PHASE_DATA_CHANGED_ONFIRST_CAPTURED_ON_NEXT,
    EUSCI_SPI_CLOCKPOLARITY_INACTIVITY_HIGH,
    EUSCI_SPI_3PIN
};

/************************************  Private Functions  *******************************************/

/*
 * Delay x ms
 */
static void Delay(unsigned long interval)
{
    while(interval > 0)
    {
        __delay_cycles(48000);
        interval--;
    }
}

/*******************************************************************************
 * Function Name  : LCD_initSPI
 * Description    : Configures LCD Control lines
 * Input          : None
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
static void LCD_initSPI()
{
    //Software reset turned on
    SPI_disableModule(EUSCI_B3_SPI_BASE);

    //Makes the MISO an input and its function connected to the SPI
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P10, GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

    //Makes CLK, MOSI and CS's output pins and their function connected to the SPI
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P10, GPIO_PIN1 | GPIO_PIN2,
                                                GPIO_PRIMARY_MODULE_FUNCTION);

    //Makes pin 4 and 5 of port 10 GPIO pins
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P10, GPIO_PIN4 | GPIO_PIN5,
                                                0x00);

    //Initializes SPI as master
    while(!SPI_initMaster(EUSCI_B3_SPI_BASE, &spiConfigLCD));

    //Enables SPI
    SPI_enableModule(EUSCI_B3_SPI_BASE);
}

/*******************************************************************************
 * Function Name  : LCD_reset
 * Description    : Resets LCD
 * Input          : None
 * Output         : None
 * Return         : None
 * Attention      : Uses P10.0 for reset
 *******************************************************************************/
static void LCD_reset()
{
    P10DIR |= BIT0;
    P10OUT |= BIT0;  // high
    Delay(100);
    P10OUT &= ~BIT0; // low
    Delay(100);
    P10OUT |= BIT0;  // high
}

/************************************  Private Functions  *******************************************/


/************************************  Public Functions  *******************************************/

/*******************************************************************************
 * Function Name  : LCD_DrawRectangle
 * Description    : Draw a rectangle as the specified color
 * Input          : xStart, xEnd, yStart, yEnd, Color
 * Output         : None
 * Return         : None
 * Attention      : Must draw from left to right, top to bottom!
 *******************************************************************************/
void LCD_DrawRectangle(int16_t xStart, int16_t xEnd, int16_t yStart, int16_t yEnd, uint16_t Color)
{
    // Optimization complexity: O(64 + 2N) Bytes Written 
    /* Check special cases for out of bounds */
    if((xEnd <= MAX_SCREEN_X) && (yEnd <= MAX_SCREEN_Y)
            && (xStart >= MIN_SCREEN_X) && (yStart >= MIN_SCREEN_Y))
    {
        /* Set window area for high-speed RAM write */
        LCD_WriteReg(HOR_ADDR_START_POS, yStart);     /* Horizontal GRAM Start Address */
        LCD_WriteReg(HOR_ADDR_END_POS, (yEnd - 1));  /* Horizontal GRAM End Address */
        LCD_WriteReg(VERT_ADDR_START_POS, xStart);    /* Vertical GRAM Start Address */
        LCD_WriteReg(VERT_ADDR_END_POS, (xEnd - 1)); /* Vertical GRAM Start Address */

        /* Set cursor */
        LCD_SetCursor(xStart, yStart);

        /* Set index to GRAM */
        LCD_WriteIndex(GRAM);

        /* Send out data only to the entire area */
        uint32_t area = (xEnd - xStart) * (yEnd - yStart);
        SPI_CS_LOW;

        LCD_Write_Data_Start();
        for(uint32_t y = 0; y < area; ++y)
        {
            LCD_Write_Data_Only(Color);
        }

        SPI_CS_HIGH;
    }
}

/******************************************************************************
 * Function Name  : PutChar
 * Description    : Lcd screen displays a character
 * Input          : - Xpos: Horizontal coordinate
 *                  - Ypos: Vertical coordinate
 *                  - ASCI: Displayed character
 *                  - charColor: Character color
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void PutChar( uint16_t Xpos, uint16_t Ypos, uint8_t ASCI, uint16_t charColor)
{
    uint16_t i, j;
    uint8_t buffer[16], tmp_char;
    GetASCIICode(buffer,ASCI);  /* get font data */
    for( i=0; i<16; i++ )
    {
        tmp_char = buffer[i];
        for( j=0; j<8; j++ )
        {
            if( (tmp_char >> 7 - j) & 0x01 == 0x01 )
            {
                LCD_SetPoint( Xpos + j, Ypos + i, charColor );  /* Character color */
            }
        }
    }
}

/******************************************************************************
 * Function Name  : GUI_Text
 * Description    : Displays the string
 * Input          : - Xpos: Horizontal coordinate
 *                  - Ypos: Vertical coordinate
 *                  - str: Displayed string
 *                  - charColor: Character color
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
void LCD_Text(uint16_t Xpos, uint16_t Ypos, uint8_t *str, uint16_t Color)
{
    uint8_t TempChar;

    /* Set area back to span the entire LCD */
    LCD_WriteReg(HOR_ADDR_START_POS, 0x0000);     /* Horizontal GRAM Start Address */
    LCD_WriteReg(HOR_ADDR_END_POS, (MAX_SCREEN_Y - 1));  /* Horizontal GRAM End Address */
    LCD_WriteReg(VERT_ADDR_START_POS, 0x0000);    /* Vertical GRAM Start Address */
    LCD_WriteReg(VERT_ADDR_END_POS, (MAX_SCREEN_X - 1)); /* Vertical GRAM Start Address */
    do
    {
        TempChar = *str++;
        PutChar( Xpos, Ypos, TempChar, Color);
        if( Xpos < MAX_SCREEN_X - 8)
        {
            Xpos += 8;
        }
        else if ( Ypos < MAX_SCREEN_X - 16)
        {
            Xpos = 0;
            Ypos += 16;
        }
        else
        {
            Xpos = 0;
            Ypos = 0;
        }
    }
    while ( *str != 0 );
}


/*******************************************************************************
 * Function Name  : LCD_Clear
 * Description    : Fill the screen as the specified color
 * Input          : - Color: Screen Color
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
void LCD_Clear(uint16_t Color)
{
    /* Sets area back to span the entire LCD */
    LCD_WriteReg(HOR_ADDR_START_POS, 0x0000);     /* Horizontal GRAM Start Address */
    LCD_WriteReg(HOR_ADDR_END_POS, (MAX_SCREEN_Y - 1));  /* Horizontal GRAM End Address */
    LCD_WriteReg(VERT_ADDR_START_POS, 0x0000);    /* Vertical GRAM Start Address */
    LCD_WriteReg(VERT_ADDR_END_POS, (MAX_SCREEN_X - 1)); /* Vertical GRAM Start Address */

    /* Sets cursor to (0,0) */
    LCD_SetCursor(0, 0);

    /* Sets write index to GRAM */
    LCD_WriteIndex(GRAM);

    /* Start data transmittion */
    SPI_CS_LOW;
    LCD_Write_Data_Start();

    for(uint32_t y = 0; y <= SCREEN_SIZE; ++y)
    {
        LCD_Write_Data_Only(Color);
    }

    SPI_CS_HIGH;
}

/******************************************************************************
 * Function Name  : LCD_SetPoint
 * Description    : Drawn at a specified point coordinates
 * Input          : - Xpos: Row Coordinate
 *                  - Ypos: Line Coordinate
 * Output         : None
 * Return         : None
 * Attention      : 18N Bytes Written
 *******************************************************************************/
void LCD_SetPoint(uint16_t Xpos, uint16_t Ypos, uint16_t color)
{
    //Checks if point is out of bounds
    if(Xpos < MAX_SCREEN_X && Ypos < MAX_SCREEN_Y)
    {
        //Sets cursor to Xpos and Ypos
        LCD_SetCursor(Xpos, Ypos);
        //Writes color to GRAM
        LCD_WriteReg(GRAM, color);
    }
}

/*******************************************************************************
 * Function Name  : LCD_Write_Data_Only
 * Description    : Data writing to the LCD controller
 * Input          : - data: data to be written
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_Write_Data_Only(uint16_t data)
{
    /* Sends out MSB */
    SPI_transmitData(EUSCI_B3_SPI_BASE, data >> 8);

    /* Sends out LSB */
    SPI_transmitData(EUSCI_B3_SPI_BASE, data);
}

/*******************************************************************************
 * Function Name  : LCD_WriteData
 * Description    : LCD write register data
 * Input          : - data: register data
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_WriteData(uint16_t data)//d
{
    SPI_CS_LOW;

    SPISendRecvByte(SPI_START | SPI_WR | SPI_DATA);    /* Write : RS = 1, RW = 0       */
    SPISendRecvByte((data >>   8));                    /* Write D8..D15                */
    SPISendRecvByte((data & 0xFF));                    /* Write D0..D7                 */

    SPI_CS_HIGH;
}

/*******************************************************************************
 * Function Name  : TP_ReadReg
 * Description    : Reads the selected LCD Register.
 * Input          : None
 * Output         : None
 * Return         : LCD Register Value.
 * Attention      : None
 *******************************************************************************/
inline uint16_t TP_ReadReg(uint8_t LCD_Reg)
{
    uint16_t data;
    //Software reset turned on
    SPI_disableModule(EUSCI_B3_SPI_BASE);

    //Initializes SPI as master
    while(!SPI_initMaster(EUSCI_B3_SPI_BASE, &spiConfigTP));

    //Enables SPI
    SPI_enableModule(EUSCI_B3_SPI_BASE);

    //Turns off interrupt for P4.0
    __NVIC_DisableIRQ(PORT4_IRQn);
    P4->IE &= ~BIT0;

    //Draws rectangle
    G8RTOS_WaitSemaphore(&LCDMutex);

    SPI_CS_TP_LOW;

    /* Sends byte of data */
    SPISendRecvTPByte(LCD_Reg);

    /* Returns 16-bit Reg */
    uint8_t dataHigh = SPISendRecvTPByte(0);

    /* Returns 16-bit Reg */
    uint8_t dataLow = SPISendRecvTPByte(0);

    SPI_CS_TP_HIGH;

    //Draws rectangle
    G8RTOS_SignalSemaphore(&LCDMutex);

    //Turns on interrupt for P4.0
    P4->IFG &= ~BIT0;
    P4->IE |= BIT0;
    __NVIC_EnableIRQ(PORT4_IRQn);

    data = ((dataHigh << 8) | (dataLow)) >> 3;

    //Software reset turned on
    SPI_disableModule(EUSCI_B3_SPI_BASE);

    //Initializes SPI as master
    while(!SPI_initMaster(EUSCI_B3_SPI_BASE, &spiConfigLCD));

    //Enables SPI
    SPI_enableModule(EUSCI_B3_SPI_BASE);

    return data;
}

/*******************************************************************************
 * Function Name  : LCD_WriteIndex
 * Description    : LCD write register address
 * Input          : - index: register address
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_WriteIndex(uint16_t index)
{
    SPI_CS_LOW;

    /* SPI write data */
    SPISendRecvByte(SPI_START | SPI_WR | SPI_INDEX);   /* Write : RS = 0, RW = 0  */
    SPISendRecvByte(0);
    SPISendRecvByte(index);

    SPI_CS_HIGH;
}

/*******************************************************************************
 * Function Name  : SPISendRecvByte
 * Description    : Send one byte then receive one byte of response
 * Input          : uint8_t: byte
 * Output         : None
 * Return         : Recieved value 
 * Attention      : None
 *******************************************************************************/
inline uint8_t SPISendRecvByte(uint8_t byte)
{
    /* Sends byte of data */
    SPI_transmitData(EUSCI_B3_SPI_BASE, byte);

    /* Waits as long as busy */
    while(SPI_isBusy(EUSCI_B3_SPI_BASE));

    /* Returns received value*/
    return SPI_receiveData(EUSCI_B3_SPI_BASE);
}


/*******************************************************************************
 * Function Name  : SPISendRecvTPByte
 * Description    : Send one byte then receive one byte of response from Touchpanel
 * Input          : uint8_t: byte
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline uint8_t SPISendRecvTPByte (uint8_t byte)
{
    /* Sends byte of data */
    SPI_transmitData(EUSCI_B3_SPI_BASE, byte);

    //While receive flag is not set
    while(!(UCB3IFG_SPI & UCRXIFG));

    /* Returns received value*/
    return SPI_receiveData(EUSCI_B3_SPI_BASE);
}

/*******************************************************************************
 * Function Name  : LCD_Write_Data_Start
 * Description    : Start of data writing to the LCD controller
 * Input          : None
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_Write_Data_Start(void)
{
    SPISendRecvByte(SPI_START | SPI_WR | SPI_DATA);    /* Write : RS = 1, RW = 0 */
}

/*******************************************************************************
 * Function Name  : LCD_ReadData
 * Description    : LCD read data
 * Input          : None
 * Output         : None
 * Return         : return data
 * Attention      : Diagram (d) in datasheet
 *******************************************************************************/
inline uint16_t LCD_ReadData()
{
    uint16_t value;
    SPI_CS_LOW;

    SPISendRecvByte(SPI_START | SPI_RD | SPI_DATA);   /* Read: RS = 1, RW = 1   */
    SPISendRecvByte(0);                               /* Dummy read 1           */
    value = (SPISendRecvByte(0) << 8);                /* Read D8..D15           */
    value |= SPISendRecvByte(0);                      /* Read D0..D7            */

    SPI_CS_HIGH;
    return value;
}

/*******************************************************************************
 * Function Name  : LCD_WriteReg
 * Description    : Writes to the selected LCD register.
 * Input          : - LCD_Reg: address of the selected register.
 *                  - LCD_RegValue: value to write to the selected register.
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue)
{
    /* Write 16-bit Index */
    LCD_WriteIndex(LCD_Reg);

    /* Write 16-bit Reg Data */
    LCD_WriteData(LCD_RegValue);
}

/*******************************************************************************
 * Function Name  : LCD_SetCursor
 * Description    : Sets the cursor position.
 * Input          : - Xpos: specifies the X position.
 *                  - Ypos: specifies the Y position.
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos )
{
    /* Set horizonal GRAM coordinate (Ypos) */
    LCD_WriteReg(HORIZONTAL_GRAM_SET, Ypos);

    /* Set vertical GRAM coordinate (Xpos) */
    LCD_WriteReg(VERTICAL_GRAM_SET, Xpos);
}

/*******************************************************************************
 * Function Name  : LCD_Init
 * Description    : Configures LCD Control lines, sets whole screen black
 * Input          : bool usingTP: determines whether or not to enable TP interrupt 
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
void LCD_Init(bool usingTP)
{
    LCD_initSPI();

    if (usingTP)
    {
        /* Configures low true interrupt on P4.0 for TP */
        P4->DIR &= ~BIT0; //Makes button 0 an input
        P4->IES |= BIT0;  //Flag set on falling edge
        P4->REN |= BIT0;  //Pullup or Pulldown enabled
        P4->OUT |= BIT0;  //Input configured as pullup
        P4->IFG &= ~BIT0; //Clears interrupt flag for button 0
        P4->IE |= BIT0;   //Enables interrupt for 4.0

    }

    LCD_reset();

    LCD_WriteReg(0xE5, 0x78F0); /* set SRAM internal timing */
    LCD_WriteReg(DRIVER_OUTPUT_CONTROL, 0x0100); /* set Driver Output Control */
    LCD_WriteReg(DRIVING_WAVE_CONTROL, 0x0700); /* set 1 line inversion */
    LCD_WriteReg(ENTRY_MODE, 0x1038); /* set GRAM write direction and BGR=1 */
    LCD_WriteReg(RESIZING_CONTROL, 0x0000); /* Resize register */
    LCD_WriteReg(DISPLAY_CONTROL_2, 0x0207); /* set the back porch and front porch */
    LCD_WriteReg(DISPLAY_CONTROL_3, 0x0000); /* set non-display area refresh cycle ISC[3:0] */
    LCD_WriteReg(DISPLAY_CONTROL_4, 0x0000); /* FMARK function */
    LCD_WriteReg(RGB_DISPLAY_INTERFACE_CONTROL_1, 0x0000); /* RGB interface setting */
    LCD_WriteReg(FRAME_MARKER_POSITION, 0x0000); /* Frame marker Position */
    LCD_WriteReg(RGB_DISPLAY_INTERFACE_CONTROL_2, 0x0000); /* RGB interface polarity */

    /* Power On sequence */
    LCD_WriteReg(POWER_CONTROL_1, 0x0000); /* SAP, BT[3:0], AP, DSTB, SLP, STB */
    LCD_WriteReg(POWER_CONTROL_2, 0x0007); /* DC1[2:0], DC0[2:0], VC[2:0] */
    LCD_WriteReg(POWER_CONTROL_3, 0x0000); /* VREG1OUT voltage */
    LCD_WriteReg(POWER_CONTROL_4, 0x0000); /* VDV[4:0] for VCOM amplitude */
    LCD_WriteReg(DISPLAY_CONTROL_1, 0x0001);
    Delay(200);

    /* Dis-charge capacitor power voltage */
    LCD_WriteReg(POWER_CONTROL_1, 0x1090); /* SAP, BT[3:0], AP, DSTB, SLP, STB */
    LCD_WriteReg(POWER_CONTROL_2, 0x0227); /* Set DC1[2:0], DC0[2:0], VC[2:0] */
    Delay(50); /* Delay 50ms */
    LCD_WriteReg(POWER_CONTROL_3, 0x001F);
    Delay(50); /* Delay 50ms */
    LCD_WriteReg(POWER_CONTROL_4, 0x1500); /* VDV[4:0] for VCOM amplitude */
    LCD_WriteReg(POWER_CONTROL_7, 0x0027); /* 04 VCM[5:0] for VCOMH */
    LCD_WriteReg(FRAME_RATE_AND_COLOR_CONTROL, 0x000D); /* Set Frame Rate */
    Delay(50); /* Delay 50ms */
    LCD_WriteReg(GRAM_HORIZONTAL_ADDRESS_SET, 0x0000); /* GRAM horizontal Address */
    LCD_WriteReg(GRAM_VERTICAL_ADDRESS_SET, 0x0000); /* GRAM Vertical Address */

    /* Adjust the Gamma Curve */
    LCD_WriteReg(GAMMA_CONTROL_1,    0x0000);
    LCD_WriteReg(GAMMA_CONTROL_2,    0x0707);
    LCD_WriteReg(GAMMA_CONTROL_3,    0x0307);
    LCD_WriteReg(GAMMA_CONTROL_4,    0x0200);
    LCD_WriteReg(GAMMA_CONTROL_5,    0x0008);
    LCD_WriteReg(GAMMA_CONTROL_6,    0x0004);
    LCD_WriteReg(GAMMA_CONTROL_7,    0x0000);
    LCD_WriteReg(GAMMA_CONTROL_8,    0x0707);
    LCD_WriteReg(GAMMA_CONTROL_9,    0x0002);
    LCD_WriteReg(GAMMA_CONTROL_10,   0x1D04);

    /* Set GRAM area */
    LCD_WriteReg(HOR_ADDR_START_POS, 0x0000);     /* Horizontal GRAM Start Address */
    LCD_WriteReg(HOR_ADDR_END_POS, (MAX_SCREEN_Y - 1));  /* Horizontal GRAM End Address */
    LCD_WriteReg(VERT_ADDR_START_POS, 0x0000);    /* Vertical GRAM Start Address */
    LCD_WriteReg(VERT_ADDR_END_POS, (MAX_SCREEN_X - 1)); /* Vertical GRAM Start Address */
    LCD_WriteReg(GATE_SCAN_CONTROL_0X60, 0x2700); /* Gate Scan Line */
    LCD_WriteReg(GATE_SCAN_CONTROL_0X61, 0x0001); /* NDL,VLE, REV */
    LCD_WriteReg(GATE_SCAN_CONTROL_0X6A, 0x0000); /* set scrolling line */

    /* Partial Display Control */
    LCD_WriteReg(PART_IMAGE_1_DISPLAY_POS, 0x0000);
    LCD_WriteReg(PART_IMG_1_START_END_ADDR_0x81, 0x0000);
    LCD_WriteReg(PART_IMG_1_START_END_ADDR_0x82, 0x0000);
    LCD_WriteReg(PART_IMAGE_2_DISPLAY_POS, 0x0000);
    LCD_WriteReg(PART_IMG_2_START_END_ADDR_0x84, 0x0000);
    LCD_WriteReg(PART_IMG_2_START_END_ADDR_0x85, 0x0000);

    /* Panel Control */
    LCD_WriteReg(PANEL_ITERFACE_CONTROL_1, 0x0010);
    LCD_WriteReg(PANEL_ITERFACE_CONTROL_2, 0x0600);
    LCD_WriteReg(DISPLAY_CONTROL_1, 0x0133); /* 262K color and display ON */
    Delay(50); /* delay 50 ms */

    LCD_Clear(LCD_BLACK);
}

/*******************************************************************************
 * Function Name  : TP_ReadXY
 * Description    : Obtain X and Y touch coordinates
 * Input          : None
 * Output         : None
 * Return         : Pointer to "Point" structure
 * Attention      : None
 *******************************************************************************/
Point TP_ReadXY()
{
    Point p;

    /* Reads X coord. */
    p.x = TP_ReadX();

    /* Reads Y coord. */
    p.y = TP_ReadY();

    /* Returns point  */
    return p;
}

/*******************************************************************************
 * Function Name  : TP_ReadX
 * Description    : Obtain X touch coordinate
 * Input          : None
 * Output         : None
 * Return         : X Coordinate
 * Attention      : None
 *******************************************************************************/
uint16_t TP_ReadX()
{
    uint16_t p = TP_ReadReg(CHX);
    return (p - 340) / 11;
}

/*******************************************************************************
 * Function Name  : TP_ReadY
 * Description    : Obtain Y touch coordinate
 * Input          : None
 * Output         : None
 * Return         : Y Coordinate
 * Attention      : None
 *******************************************************************************/
uint16_t TP_ReadY()
{
    uint16_t p = TP_ReadReg(CHY);
    return (p - 250) / 15;
}

/************************************  Public Functions  *******************************************/

