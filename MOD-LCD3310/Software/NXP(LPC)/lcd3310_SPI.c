/*************************************************************************
 *
 *    Driver for the Nokia 3310 LCD
 *
 **************************************************************************/
#include "includes.h"

/* TO IMPLEMENT YOUR VERSION OF THE DRIVER YOU'LL HAVE TO EDIT THIS SECTION ONLY */

#define LCD_CS_MAKE_OUT()		LCD3310_CS_DIR |= LCD3310_CS_MASK
#define LCD_CS_HIGH()  			PIN_HIGH(LCD3310_CS_OUT)
#define LCD_CS_LOW()  			PIN_LOW(LCD3310_CS_OUT)

#define LCD_RES_MAKE_OUT()		LCD3310_RST_DIR |= LCD3310_RST_MASK
#define LCD_RES_HIGH()			PIN_HIGH(LCD3310_RST_OUT)
#define LCD_RES_LOW()			PIN_LOW(LCD3310_RST_OUT)

#define LCD_DC_MAKE_OUT()		LCD3310_CD_DIR |= LCD3310_CD_MASK
#define LCD_DC_HIGH()			PIN_HIGH(LCD3310_CD_OUT)
#define LCD_DC_LOW()			PIN_LOW(LCD3310_CD_OUT)

#define SEND_BYTE_SPI() 		\
	{							\
		while(SSP0SR_bit.BSY);  \
		SSP0DR = data;          \
		while(SSP0SR_bit.BSY);  \
	}

static void Initialize_SPI(void)
{
    // disable the module if enabled previously
    SSP0CR1 = 0x0;

    // enable clock
    SYSAHBCLKCTRL_bit.SSP0 = 1;
    SSP0CLKDIV_bit.DIV = 1;

    // setup pin directions
    IOCON_PIO0_14_bit.FUNC = 0x2; // SCK
    IOCON_PIO0_14_bit.MODE = 0x0;
    IOCON_PIO0_17_bit.FUNC = 0x2; // MOSI
    IOCON_PIO0_17_bit.MODE = 0x0;

    // clear pull-ups on the RST, CS and CD
    IOCON_PIO2_13_bit.MODE = 0x0;
    IOCON_PIO2_14_bit.MODE = 0x0;
    IOCON_PIO2_15_bit.MODE = 0x0;


    SSP0CR0_bit.DSS = 7; // 8-bit data frame
    SSP0CR0_bit.FRF = 0; //	SPI mode
    SSP0CR0_bit.CPOL = 1;
    SSP0CR0_bit.CPHA = 0;
    SSP0CR0_bit.SCR = 0; // 1 clock per bit

    // set up speed
    SSP0CPSR_bit.CPSDVSR = 0xFE; // ((SYS_GetMainClk()/(SYSAHBCLKDIV)) / 200000) + 1;  	

    // enable the module
    SSP0CR1_bit.SSE = 1;
}

/* END OF SECTION */


#define LCD_START_LINE_ADDR	(66-2)

#if LCD_START_LINE_ADDR	> 66
#error "Invalid LCD starting line address"
#endif

// LCD memory index
unsigned int LcdMemIdx;

// represent LCD matrix
unsigned char LcdMemory[LCD_CACHE_SIZE];

const unsigned char FontLookup [][5] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00}, // sp
    { 0x00, 0x00, 0x2f, 0x00, 0x00}, // !
    { 0x00, 0x07, 0x00, 0x07, 0x00}, // "
    { 0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
    { 0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
    { 0xc4, 0xc8, 0x10, 0x26, 0x46}, // %
    { 0x36, 0x49, 0x55, 0x22, 0x50}, // &
    { 0x00, 0x05, 0x03, 0x00, 0x00}, // '
    { 0x00, 0x1c, 0x22, 0x41, 0x00}, // (
    { 0x00, 0x41, 0x22, 0x1c, 0x00}, // )
    { 0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    { 0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    { 0x00, 0x00, 0x50, 0x30, 0x00}, // ,
    { 0x10, 0x10, 0x10, 0x10, 0x10}, // -
    { 0x00, 0x60, 0x60, 0x00, 0x00}, // .
    { 0x20, 0x10, 0x08, 0x04, 0x02}, // /
    { 0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    { 0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    { 0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    { 0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    { 0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    { 0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    { 0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    { 0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    { 0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    { 0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    { 0x00, 0x36, 0x36, 0x00, 0x00}, // :
    { 0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    { 0x08, 0x14, 0x22, 0x41, 0x00}, // <
    { 0x14, 0x14, 0x14, 0x14, 0x14}, // =
    { 0x00, 0x41, 0x22, 0x14, 0x08}, // >
    { 0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    { 0x32, 0x49, 0x59, 0x51, 0x3E}, // @
    { 0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    { 0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    { 0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    { 0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    { 0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    { 0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    { 0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    { 0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    { 0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    { 0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    { 0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    { 0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    { 0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    { 0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    { 0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    { 0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    { 0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    { 0x46, 0x49, 0x49, 0x49, 0x31}, // S
    { 0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    { 0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    { 0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    { 0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    { 0x63, 0x14, 0x08, 0x14, 0x63}, // X
    { 0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    { 0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    { 0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    { 0x55, 0x2A, 0x55, 0x2A, 0x55}, // 55
    { 0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    { 0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    { 0x40, 0x40, 0x40, 0x40, 0x40}, // _
    { 0x00, 0x01, 0x02, 0x04, 0x00}, // '
    { 0x20, 0x54, 0x54, 0x54, 0x78}, // a
    { 0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    { 0x38, 0x44, 0x44, 0x44, 0x20}, // c
    { 0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    { 0x38, 0x54, 0x54, 0x54, 0x18}, // e
    { 0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    { 0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    { 0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    { 0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    { 0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    { 0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    { 0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    { 0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    { 0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    { 0x38, 0x44, 0x44, 0x44, 0x38}, // o
    { 0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    { 0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    { 0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    { 0x48, 0x54, 0x54, 0x54, 0x20}, // s
    { 0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    { 0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    { 0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    { 0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    { 0x44, 0x28, 0x10, 0x28, 0x44}, // x
    { 0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    { 0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    { 0x08, 0x6C, 0x6A, 0x19, 0x08}, // { (lighting)
    { 0x0C, 0x12, 0x24, 0x12, 0x0C}, // | (heart)
    { 0x7E, 0x7E, 0x7E, 0x7E, 0x7E}, // square
};


// simple delays

void Delay(volatile unsigned long a)
{
    while (a != 0) a--;
}

void Delayc(volatile unsigned char a)
{
    while (a != 0) a--;
}

/****************************************************************************/
/*  Initialize LCD Controller                                               */
/*  Function : LCDInit                                                      */
/*      Parameters                                                          */
/*          Input   :  Nothing                                              */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDInit(void)
{
    // Initialie SPI Interface
    Initialize_SPI();

    // set pin directions
    LCD_CS_MAKE_OUT();
    LCD_CS_HIGH();
    LCD_DC_MAKE_OUT();
    LCD_RES_MAKE_OUT();

    // Toggle reset pin
    LCD_RES_LOW();
    Delay(1000);
    LCD_RES_HIGH();
    Delay(1000);

    // Send sequence of command
    LCDSend(0x21, SEND_CMD); // LCD Extended Commands.
    LCDSend(0xC8, SEND_CMD); // Set LCD Vop (Contrast). 0xC8
    LCDSend(0x04 | !!(LCD_START_LINE_ADDR & (1u << 6)), SEND_CMD); // Set Temp S6 for start line
    LCDSend(0x40 | (LCD_START_LINE_ADDR & ((1u << 6) - 1)), SEND_CMD); // Set Temp S[5:0] for start line
    //LCDSend( 0x13, SEND_CMD );  // LCD bias mode 1:48.
    LCDSend(0x12, SEND_CMD); // LCD bias mode 1:68.
    LCDSend(0x20, SEND_CMD); // LCD Standard Commands, Horizontal addressing mode.
    //LCDSend( 0x22, SEND_CMD );  // LCD Standard Commands, Vertical addressing mode.
    LCDSend(0x08, SEND_CMD); // LCD blank
    LCDSend(0x0C, SEND_CMD); // LCD in normal mode.

    // Clear and Update
    LCDClear();
    LCDUpdate();
}

/****************************************************************************/
/*  Reset LCD 	                                                            */
/*  Function : LCDReset                                                     */
/*      Parameters                                                          */
/*          Input   :  Resets the LCD module		                        */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDReset(void)
{

    // Close SPI module - optional
    // NOT DONE

    LCD_RES_LOW();
}

/****************************************************************************/
/*  Update LCD                                                              */
/*  Function : LCDUpdate                                                    */
/*      Parameters                                                          */
/*          Input   :  sends buffer data to the LCD                         */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDUpdate(void)
{
    int x, y;

    for (y = 0; y < 48 / 8; y++)
    {
        LCDSend(0x80, SEND_CMD);
        LCDSend(0x40 | y, SEND_CMD);
        for (x = 0; x < 84; x++)
        {
            LCDSend(LcdMemory[y * 84 + x], SEND_CHR);
        }
    }
}
/****************************************************************************/
/*  Send to LCD                                                             */
/*  Function : LCDSend                                                      */
/*      Parameters                                                          */
/*          Input   :  data and  SEND_CHR or SEND_CMD                       */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDSend(unsigned char data, unsigned char cd)
{
    LCD_CS_LOW();

    if (cd == SEND_CHR)
    {
        LCD_DC_HIGH();
    }
    else
    {
        LCD_DC_LOW();
    }

    // send data over SPI
    SEND_BYTE_SPI();

    LCD_CS_HIGH();
}


/****************************************************************************/
/*  Clear LCD                                                               */
/*  Function : LCDClear                                                     */
/*      Parameters                                                          */
/*          Input   :  Nothing                                              */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDClear(void)
{
    int i;

    // loop all cache array
    for (i = 0; i < LCD_CACHE_SIZE; i++)
    {
        LcdMemory[i] = 0x00;
    }
}

/****************************************************************************/
/*  Write char at x position on y row                                       */
/*  Function : LCDChrXY                                                     */
/*      Parameters                                                          */
/*          Input   :  position, row, char                                  */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDChrXY(unsigned char x, unsigned char y, unsigned char ch)
{
    unsigned int index = 0;
    unsigned int offset = 0;
    unsigned int i = 0;

    // check for out off range
    if (x > LCD_X_RES) return;
    if (y > LCD_Y_RES) return;

    index = (unsigned int) x * 5 + (unsigned int) y * 84;

    for (i = 0; i < 5; i++)
    {
        offset = FontLookup[ch - 32][i];
        LcdMemory[index] = offset;
        index++;
    }

}

/****************************************************************************/
/*  Write negative char at x position on y row                              */
/*  Function : LCDChrXYInverse                                              */
/*      Parameters                                                          */
/*          Input   :  position, row, char                                  */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDChrXYInverse(unsigned char x, unsigned char y, unsigned char ch)
{
    unsigned int index = 0;
    unsigned int i = 0;

    // check for out off range
    if (x > LCD_X_RES) return;
    if (y > LCD_Y_RES) return;

    index = (unsigned int) x * 5 + (unsigned int) y * 84;

    for (i = 0; i < 5; i++)
    {
        LcdMemory[index] = ~(FontLookup[ch - 32][i]);
        index++;
    }

}

/****************************************************************************/
/*  Set LCD Contrast                                                        */
/*  Function : LcdContrast                                                  */
/*      Parameters                                                          */
/*          Input   :  contrast                                             */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDContrast(unsigned char contrast)
{

    //  LCD Extended Commands.
    LCDSend(0x21, SEND_CMD);

    // Set LCD Vop (Contrast).
    LCDSend(0x80 | contrast, SEND_CMD);

    //  LCD Standard Commands, horizontal addressing mode.
    LCDSend(0x20, SEND_CMD);
}


/****************************************************************************/
/*  Send string to LCD                                                      */
/*  Function : LCDStr                                                       */
/*      Parameters                                                          */
/*          Input   :  row, text, inversion                                 */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDStr(unsigned char row, const unsigned char *dataPtr, unsigned char inv)
{

    // variable for X coordinate
    unsigned char x = 0;

    // loop to the and of string
    while (*dataPtr)
    {
        if (inv)
        {
            LCDChrXYInverse(x, row, *dataPtr);
        }
        else
        {
            LCDChrXY(x, row, *dataPtr);
        }
        x++;
        dataPtr++;
    }

    LCDUpdate();
}


