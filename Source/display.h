/* ----------------------------------------------------------------------------
	128x64 Graphic LCD management for SSD1306 driver

	FILE NAME 	: display.h
	
	DESCRIPTION	: The purpose of this function is to manage a graphic LCD
			  by providing function for control and display text and graphic 
			  
	AUTHOR		: Gabriel Anzziani
    www.gabotronics.com

*/

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "fonts.h"

// LS013B7DH03 Commands
// M0 M1 M2 DMY DMY DMY DMY DMY

// M0: H' the module enters Dynamic Mode, where pixel data will be updated.
// M0: 'L' the module remains in Static Mode, where pixel data is retained

// M1: VCOM When M1 is 'H' then VCOM = 'H' is output. If M1 is 'L' then VCOM = 'L' is output.
// When EXTMODE = 'H', M1 value = XX (don’t care)

// M2 CLEAR ALL When M2 is 'L' then all flags are cleared. When a full display clearing is required,
// set M0 and M2 = HIGH and set all display data to white.

#define     STATIC_MODE     0b00000000      // Set Static Mode
#define     CLEAR_ALL       0b00100000      // Clear screen
#define     DYNAMIC_MODE    0b10000000      // Write Single or Multiple Line

#define DISPLAY_DATA_SIZE	(2048+128*2) // Data + Addresses + Trailers
#define FBAUD32M            0x0E   // BSEL=14 (SPI clock = 1.06MHz, LS013B7DH03 max is 1.1MHz)
#define FBAUD2M             0x00   // BSEL=0  (SPI clock = 1.00MHz, LS013B7DH03 max is 1.1MHz)
#define MAX_X				127
#define MAX_Y				127
#define LAST_LINE			15     // Last text line

typedef struct {
    uint8_t     *spidata;
    uint8_t     *buffer;
    uint8_t     display_setup1[2];
    uint8_t     buffer1[DISPLAY_DATA_SIZE];
    uint8_t     display_setup2[2];
    uint8_t     buffer2[DISPLAY_DATA_SIZE];
    uint8_t     buffer3[DISPLAY_DATA_SIZE];
} Disp_data;

extern const uint8_t BigFonts[];
extern Disp_data Disp_send;
extern uint8_t u8CursorX, u8CursorY;
extern const int8_t tdown[];
extern const int8_t tup[];
extern const int8_t tdual[];

#define lcd_goto(x,y) { u8CursorX=(x); u8CursorY=(y); }

/* EXTERN Function Prototype(s) */
void GLCD_LcdInit(void);
void GLCD_setting(void);
void GLCD_LcdOff(void);
void GLCD_Print(const char *);
void lcd_putsp(const char *);
void lcd_put5x8(const char *);
void GLCD_Bigchar (char u8Char);
void GLCD_Putchar (char);
void putchar5x8(char u8Char);
void printN(uint8_t Data);
void printN5x8(uint8_t Data);
void SwitchBuffer(void);
void clr_display(void);
void dma_display(void);
void set_pixel(uint8_t x, uint8_t y);
void set_pixelc(uint8_t x, uint8_t y, uint8_t c);
void sprite(uint8_t x, uint8_t y, const int8_t *ptr);
void lcd_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void lcd_hline(uint8_t x1, uint8_t x2, uint8_t y, uint8_t c);
void Rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c);
void fillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c);
void fillTriangle(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t x3,uint8_t y3, uint8_t c);
void lcd_circle(uint8_t x, uint8_t y, uint8_t radius, uint8_t c);
void circle_fill(uint8_t x,uint8_t y, uint8_t radius, uint8_t c);
void write_display(uint8_t data);
void zero_display();
void printV(int16_t Data, uint8_t gain);
void printF(uint8_t x, uint8_t y, int32_t Data);
void tiny_printp(uint8_t x, uint8_t y, const char *ptr);
void LcdInstructionWrite (unsigned char);
void bitmap(uint8_t x, uint8_t y, const uint8_t *BMP);
void bitmap_safe(int8_t x, int8_t y, const uint8_t *BMP, uint8_t c);
    
#endif
