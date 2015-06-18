#include <stdint.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "display.h"
#include "fonts.h"
#include "mygccdef.h"
#include "main.h"
#include "mso.h"

Disp_data Disp_send;
uint8_t   u8CursorX, u8CursorY;

// Small fonts, each character takes 5 nibbles (5 bytes contain 2 characters)
const uint8_t PROGMEM font[190] = {
    0x00, 0x00, 0x04, 0x44, 0x04,   // 0:   Space !
    0xAA, 0x00, 0x00, 0xAE, 0xEA,   // 1:   " #
    0x4E, 0xC6, 0xEA, 0x24, 0x8A,   // 2:   $ %
    0x4A, 0x4C, 0xE4, 0x40, 0x00,   // 3:   & '
    0x24, 0x44, 0x28, 0x44, 0x48,   // 4:   ( )
    0xA4, 0xA0, 0x00, 0x4E, 0x40,   // 5:   * +
    0x00, 0x04, 0x80, 0x0E, 0x00,   // 6:   , -
    0x00, 0x00, 0x42, 0x24, 0x88,   // 7:   . /
    0x4A, 0xAA, 0x44, 0xC4, 0x4E,   // 8:   0 1
    0xC2, 0x68, 0xEE, 0x26, 0x2E,   // 9:   2 3
    0xAA, 0xE2, 0x2E, 0x8C, 0x2C,   // 10:  4 5
    0xE8, 0xEA, 0xEE, 0x24, 0x44,   // 11:  6 7
    0xEA, 0xEA, 0xEE, 0xAE, 0x2E,   // 12:  8 9
    0x04, 0x04, 0x00, 0x40, 0x48,   // 13:  : ;
    0x24, 0x84, 0x20, 0xE0, 0xE0,   // 14:  < =
    0x84, 0x24, 0x8C, 0x24, 0x04,   // 15:  > ?
    0x0E, 0xEE, 0x2E, 0xAE, 0xAA,   // 16:  @ A
    0xEA, 0xCA, 0xEE, 0x88, 0x8E,   // 17:  B C
    0xCA, 0xAA, 0xCE, 0x8E, 0x8E,   // 18:  D E
    0xE8, 0xE8, 0x8E, 0x8A, 0xAE,   // 19:  F G
    0xAA, 0xEA, 0xAE, 0x44, 0x4E,   // 20:  H I
    0x22, 0x2A, 0xEA, 0xAC, 0xAA,   // 21:  J K
    0x88, 0x88, 0xEA, 0xEA, 0xAA,   // 22:  L M
    0xEA, 0xAA, 0xAE, 0xAA, 0xAE,   // 23:  N O
    0xEA, 0xE8, 0x84, 0xAA, 0xA6,   // 24:  P Q
    0xCA, 0xCA, 0xAE, 0x8E, 0x2E,   // 25:  R S
    0xE4, 0x44, 0x4A, 0xAA, 0xAE,   // 26:  T U
    0xAA, 0xAA, 0x4A, 0xAA, 0xEA,   // 27:  V W
    0xAA, 0x4A, 0xAA, 0xA4, 0x44,   // 28:  X Y
    0xE2, 0x48, 0xE6, 0x44, 0x46,   // 29:  Z [
    0x88, 0x42, 0x2C, 0x44, 0x4C,   // 30:  \ ]
    0x4A, 0x00, 0x00, 0x00, 0x0E,   // 31:  ^ _
    0x42, 0x00, 0x00, 0x0A, 0xAE,   // 32:  ` u     (a)
    0x00, 0xEA, 0xA2, 0x45, 0x99,   // 33:  n /     (b)  (c)
    0x44, 0xD5, 0xD0, 0x05, 0x52,   // 34:  div     (de)
    0x00, 0x32, 0x20, 0x0E, 0xAA,   // 35:  m       (fg)
    0x4E, 0x44, 0xCC, 0x44, 0xE4,   // 36:  up down (h)  (i)
    0x44, 0xAA, 0xE0, 0x00, 0x00    // 37:  delta   (j)
};

// System 3x6 (char #20 to #96) 
const uint8_t Fonts[] PROGMEM = {
    0x10,0x3E,0x10, // Line Feed            // 0x14
    0x10,0x10,0x1E, // Carriage Return      // 0x15
    0x30,0x0C,0x02, // long /               // 0x16
    0x38,0x20,0x38, // u                    // 0x17
    0x28,0x3C,0x08,//0x0A,0x0F,0x02, // up arrow             // 0x18
    0x05,0x0F,0x04, // down arrow           // 0x19
    0x00,0x00,0x38, // first part of m      // 0x1A
    0x18,0x08,0x38, // second part of m     // 0x1B
    0x30,0x0C,0x02, // first part of /div   // 0x1C
    0x28,0x3E,0x00, // second part of /div  // 0x1D
    0x18,0x20,0x18, // third part of /div   // 0x1E
    0x38,0x26,0x38, // delta                // 0x1F
    0x00,0x00,0x00, // Space                // 0x20 Valid ASCII characters from here
    0x00,0x2E,0x00, // !
    0x06,0x00,0x06, // "
    0x3E,0x14,0x3E, // #
    0x14,0x3F,0x0A, // $
    0x12,0x08,0x24, // %
    0x14,0x2A,0x34, // &
    0x00,0x06,0x00, // '
    0x1C,0x22,0x00, // (
    0x22,0x1C,0x00, // )
    0x1C,0x1C,0x1C, // *
    0x08,0x1C,0x08, // +
    0x40,0x20,0x00, // ,
    0x08,0x08,0x08, // -
    0x00,0x20,0x00, // .
    0x10,0x08,0x04, // /
    0x1C,0x22,0x1C, // 0    // 0x30
    0x24,0x3E,0x20, // 1
    0x32,0x2A,0x24, // 2
    0x22,0x2A,0x14, // 3
    0x0E,0x08,0x3E, // 4
    0x2E,0x2A,0x12, // 5
    0x3E,0x2A,0x3A, // 6
    0x02,0x3A,0x06, // 7
    0x3E,0x2A,0x3E, // 8
    0x2E,0x2A,0x3E, // 9
    0x00,0x14,0x00, // :
    0x20,0x14,0x00, // ;
    0x08,0x14,0x22, // <
    0x14,0x14,0x14, // = 
    0x22,0x14,0x08, // >
    0x02,0x2A,0x04, // ?
    0x1C,0x26,0x2E, // @    // 0x40
    0x3C,0x0A,0x3C, // A
    0x3E,0x2A,0x14, // B
    0x1C,0x22,0x22, // C
    0x3E,0x22,0x1C, // D
    0x3E,0x2A,0x22, // E
    0x3E,0x0A,0x02, // F
    0x1C,0x22,0x1A, // G
    0x3E,0x08,0x3E, // H
    0x22,0x3E,0x22, // I
    0x10,0x20,0x1E, // J
    0x3E,0x08,0x36, // K
    0x3E,0x20,0x20, // L
    0x3E,0x04,0x3E, // M
    0x3E,0x02,0x3E, // N
    0x3E,0x22,0x3E, // O
    0x3E,0x0A,0x04, // P    // 0x50
    0x1C,0x22,0x3C, // Q
    0x3E,0x0A,0x34, // R
    0x24,0x2A,0x12, // S
    0x02,0x3E,0x02, // T
    0x3E,0x20,0x3E, // U
    0x1E,0x20,0x1E, // V
    0x3E,0x10,0x3E, // W
    0x36,0x08,0x36, // X
    0x0E,0x30,0x0E, // Y
    0x32,0x2A,0x26, // Z
    0x00,0x3E,0x22, // [
    0x04,0x08,0x10, // /*\*/
    0x22,0x3E,0x00, // ]
    0x04,0x02,0x04, // ^
    0x40,0x40,0x40, // _
    0x02,0x04,0x00, // `   // 0x60
    0x34,0x2c,0x38, // a
    0x3e,0x24,0x18, // b
    0x18,0x24,0x24, // c
    0x18,0x24,0x3e, // d
    0x18,0x34,0x2c, // e
    0x08,0x3c,0x0a, // f
    0x58,0x54,0x3c, // g
    0x3e,0x04,0x38, // h
    0x00,0x3a,0x00, // i
    0x40,0x3a,0x00, // j
    0x3e,0x18,0x24, // k
    0x00,0x1e,0x20, // l
    0x3c,0x0c,0x3c, // m
    0x3c,0x04,0x38, // n
    0x18,0x24,0x18, // o
    0x7c,0x24,0x18, // p
    0x18,0x24,0x7c, // q
    0x38,0x04,0x04, // r
    0x28,0x3c,0x14, // s
    0x04,0x3e,0x24, // t
    0x3c,0x20,0x3c, // u
    0x1c,0x20,0x1c, // v
    0x3c,0x30,0x3c, // w
    0x24,0x18,0x24, // x
    0x0c,0x50,0x3c, // y
    0x34,0x3c,0x2c, // z
    0x08,0x36,0x22, // {
    0x00,0x36,0x00, // |
    0x22,0x36,0x08, // }
    0x04,0x06,0x02, // ~
    0x3e,0x3e,0x3e, // DEL - full block
};

const uint8_t BigFonts[] PROGMEM = {
    0xF8,0xFE,0xFF,0x07,0x03,0x03,0x07,0xFF,0xFE,0xF8,0x0F,0x3F,0x7F,0x70,0x60,0x60,0x70,0x7F,0x3F,0x0F,
    0x00,0x10,0x18,0x1C,0xFE,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x7F,0x7F,0x00,0x00,0x00,
    0x0C,0x0E,0x0F,0x07,0x03,0x83,0xC7,0xFF,0xFE,0x7C,0x70,0x78,0x7C,0x7E,0x6F,0x67,0x63,0x61,0x60,0x60,
    0x0C,0x0E,0x0F,0xC7,0xC3,0xE7,0xFF,0xFE,0xBC,0x00,0x18,0x38,0x78,0x70,0x60,0x60,0x71,0x7F,0x3F,0x1F,
    0x00,0x80,0xC0,0xF0,0x78,0x1E,0xFF,0xFF,0xFF,0x00,0x0E,0x0F,0x0F,0x0D,0x0C,0x0C,0x7F,0x7F,0x7F,0x0C,
    0xF0,0xFF,0xFF,0x7F,0x63,0x63,0xE3,0xE3,0xC3,0x83,0x0C,0x3C,0x7C,0x70,0x60,0x60,0x70,0x7F,0x3F,0x1F,
    0xF8,0xFE,0xFF,0xC7,0xC3,0xC3,0xC7,0xCF,0x8E,0x0C,0x0F,0x3F,0x7F,0x71,0x60,0x60,0x71,0x7F,0x3F,0x1F,
    0x03,0x03,0x03,0x83,0xE3,0xFB,0x7F,0x1F,0x07,0x03,0x00,0x00,0x7C,0x7F,0x7F,0x03,0x00,0x00,0x00,0x00,
    0x1C,0x3E,0xFF,0xE7,0xC3,0xC3,0xE7,0xFF,0x3E,0x1C,0x1E,0x3F,0x7F,0x73,0x61,0x61,0x73,0x7F,0x3F,0x1E,
    0x7C,0xFE,0xFF,0xC7,0x83,0x83,0xC7,0xFF,0xFE,0xF8,0x18,0x38,0x79,0x71,0x61,0x61,0x71,0x7F,0x3F,0x0F
};

// Sprites
const int8_t tdown[] PROGMEM = { 0,0, 0,2,  1,0, 1,1,  1,2,  1,3,  2,2, 255  }; // Falling trigger
const int8_t tup[]   PROGMEM = { 0,0, 0,-2, 1,0, 1,-1, 1,-2, 1,-3, 2,-2, 255 }; // Rising trigger
const int8_t tdual[] PROGMEM = { 0,0, 0,-2, 1,0, 1,-1, 1,-2, 1,-3, 2,-2, 0,2,  1,0, 1,1,  1,2,  1,3,  2,2, 255 }; // Dual edge

void SwitchBuffer(void) {
    togglebit(WOptions, tick);
    if(testbit(WOptions, tick)) {   // Switch display buffer
        Disp_send.buffer=Disp_send.buffer1;
        Disp_send.spidata=Disp_send.display_setup1;
    }
    else {
        Disp_send.buffer=Disp_send.buffer2;
        Disp_send.spidata=Disp_send.display_setup2;
    }
}

// Clear display buffer
void clr_display(void) {
    uint8_t *p;
    p=Disp_send.buffer;
    for(uint8_t i=0; i<128; i++) {
        for(uint8_t j=0; j<16; j++) {
            *p++=0;
        }
        p+=2;
    }
    lcd_goto(0,0);
}

void GLCD_setting(void) {
    cli();
    if(testbit(Display, flip)) {
        //LcdInstructionWrite(LCD_SET_SCAN_NOR);   // direction
        //LcdInstructionWrite(LCD_SET_SEG_REMAP1);
    }
    else {
        //LcdInstructionWrite(LCD_SET_SCAN_FLIP);   // direction
        //LcdInstructionWrite(LCD_SET_SEG_REMAP0);
    }
    //if(testbit(Display, disp_inv)) LcdInstructionWrite(LCD_DISP_REV);   // invert
    //else LcdInstructionWrite(LCD_DISP_NOR);   // no invert
    sei();
}

// Set pixel on display buffer
void set_pixel(uint8_t x, uint8_t y) {
    // normal orientation
    //Disp_send.display_data[(uint16_t)(y<<4) + (x>>3)] |= (uint8_t)(0x80 >> (x & 0x07));

    // Rotated -90
    x=127-x;
    Disp_send.buffer[(uint16_t)(x*18) + (y>>3)] |= (uint8_t)(0x80 >> (y & 0x07));

    // Rotated +90
    //y=127-y;
    //Disp_send.display_data[(uint16_t)(x<<4) + (y>>3)] |= (uint8_t)(0x80 >> (y & 0x07));

}

// Safe pixel on display buffer with color
void set_pixelc(uint8_t x, uint8_t y, uint8_t c) {
    uint16_t address;
    uint8_t offset;
    if(x>=128 || y>=128) return;
    x=127-x;
    address = (uint16_t)(x*18) + (y>>3);
    offset = (uint8_t)(0x80 >> (y & 0x07));
    // Rotated -90
    if(c==0)        Disp_send.buffer[address] &= ~offset;
    else if(c==1)   Disp_send.buffer[address] |= offset;
    else            Disp_send.buffer[address] ^= offset;
}

void sprite(uint8_t x, uint8_t y, const int8_t *ptr) {
    int8_t a, b;
    do {
        a=pgm_read_byte(ptr++);
        b=pgm_read_byte(ptr++);
        if((uint8_t)a==255) return;
        set_pixel(x+a,y+b);
    } while(1);
}
    
// OR byte on display buffer
void write_display(uint8_t data) {
    uint8_t i=7;
    //Disp_send.display_data[((uint16_t)(u8CursorY<<7)) + (u8CursorX++)] |= data;
    do {
        if(data&0x80) set_pixel(u8CursorX,(u8CursorY<<3)+i);
        data=data<<1;
        i--;
    } while(i!=255);
    u8CursorX++;
}

// OR byte on display buffer
void write_displayg(uint8_t data) {
    uint8_t x;
    x=u8CursorX++;
    x=127-x;
    REVERSE(data);
    Disp_send.buffer[((uint16_t)(x*18)) + (u8CursorY)] |= data;
}

//-----------------------------------------------------------------------
void lcd_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    uint8_t i,dxabs,dyabs;
    int8_t dx,dy,stepx,stepy;
    dx=(int8_t)x2-x1;      // the horizontal distance of the line
    dy=(int8_t)y2-y1;      // the vertical distance of the line
    if(dy<0) { dyabs=-dy; stepy=-1; }
    else { dyabs=dy; stepy=1; }
    if(dx<0) { dxabs=-dx; stepx=-1; }
    else {dxabs=dx; stepx=1; }
    set_pixel(x1,y1);
    if (dxabs>=dyabs) { // the line is more horizontal than vertical
        uint8_t e=(uint8_t)(dxabs>>1);
        for(i=0;i<dxabs;i++) {
            e+=dyabs;
            if (e>=dxabs) {
                e-=dxabs;
                y1+=stepy;
            }
            x1+=stepx;
            set_pixel(x1,y1);
        }
    }
    else {  // the line is more vertical than horizontal
        uint8_t e=(uint8_t)(dyabs>>1);
        for(i=0;i<dyabs;i++) {
            e+=dxabs;
            if (e>=dyabs) {
                e-=dyabs;
                x1+=stepx;
            }
            y1+=stepy;
            set_pixel(x1,y1);
        }
    }
}

// Horizontal line
void lcd_hline(uint8_t x1, uint8_t x2, uint8_t y, uint8_t c) {
    if(x1>=192) x1=0; else if(x1>=128) x1=127;
    if(x2>=192) x2=0; else if(x2>=128) x2=127;
	if(x1>=x2) SWAP(x1,x2);
	for(;x1<=x2;x1++) set_pixelc(x1,y,c);
}

void Rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c) {
    if(y1>=y2) SWAP(x1,x2);
    lcd_hline(x1,x2,y1,c);
    lcd_hline(x1,x2,y2,c);
    lcd_line(x1,y1,x1,y2);
    lcd_line(x2,y1,x2,y2);
}

void fillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c) {
    if(y1>=y2) SWAP(x1,x2);    
    while(y1<=y2) {
        lcd_hline(x1,x2,y1++,c);
    }
}

// Fill a triangle - Bresenham method
// Original from http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
void fillTriangle(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t x3,uint8_t y3, uint8_t c) {
	uint8_t t1x,t2x,y,minx,maxx,t1xp,t2xp;
	uint8_t changed1 = 0;
	uint8_t changed2 = 0;
	int8_t signx1,signx2,dx1,dy1,dx2,dy2;
	uint8_t e1,e2;
    // Sort vertices
	if (y1>y2) { SWAP(y1,y2); SWAP(x1,x2); }
	if (y1>y3) { SWAP(y1,y3); SWAP(x1,x3); }
	if (y2>y3) { SWAP(y2,y3); SWAP(x2,x3); }

	t1x=t2x=x1; y=y1;   // Starting points

	dx1 = (int8_t)(x2 - x1); if(dx1<0) { dx1=-dx1; signx1=-1; } else signx1=1;
	dy1 = (int8_t)(y2 - y1);
 
	dx2 = (int8_t)(x3 - x1); if(dx2<0) { dx2=-dx2; signx2=-1; } else signx2=1;
	dy2 = (int8_t)(y3 - y1);
	
	if (dy1 > dx1) {   // swap values
        SWAP(dx1,dy1);
		changed1 = 1;
	}
	if (dy2 > dx2) {   // swap values
        SWAP(dy2,dx2);
		changed2 = 1;
	}
	
	e2 = (uint8_t)(dx2>>1);
    // Flat top, just process the second half
    if(y1==y2) goto next;
    e1 = (uint8_t)(dx1>>1);
	
	for (uint8_t i = 0; i < dx1;) {
		t1xp=0; t2xp=0;
		if(t1x<t2x) { minx=t1x; maxx=t2x; }
		else		{ minx=t2x; maxx=t1x; }
        // process first line until y value is about to change
		while(i<dx1) {
			i++;			
			e1 += dy1;
	   	   	while (e1 >= dx1) {
				e1 -= dx1;
   	   	   	   if (changed1) t1xp=signx1;//t1x += signx1;
				else          goto next1;
			}
			if (changed1) break;
			else t1x += signx1;
		}
	// Move line
	next1:
        // process second line until y value is about to change
		while (1) {
			e2 += dy2;		
			while (e2 >= dx2) {
				e2 -= dx2;
				if (changed2) t2xp=signx2;//t2x += signx2;
				else          goto next2;
			}
			if (changed2)     break;
			else              t2x += signx2;
		}
	next2:
		if(minx>t1x) minx=t1x; if(minx>t2x) minx=t2x;
		if(maxx<t1x) maxx=t1x; if(maxx<t2x) maxx=t2x;
	   	lcd_hline(minx, maxx, y,c);    // Draw line from min to max points found on the y
		// Now increase y
		if(!changed1) t1x += signx1;
		t1x+=t1xp;
		if(!changed2) t2x += signx2;
		t2x+=t2xp;
    	y += 1;
		if(y==y2) break;
		
   }
	next:
	// Second half
	dx1 = (int8_t)(x3 - x2); if(dx1<0) { dx1=-dx1; signx1=-1; } else signx1=1;
	dy1 = (int8_t)(y3 - y2);
	t1x=x2;
 
	if (dy1 > dx1) {   // swap values
        SWAP(dy1,dx1);
		changed1 = 1;
	} else changed1=0;
	
	e1 = (uint8_t)(dx1>>1);
	
	for (uint8_t i = 0; i<=dx1; i++) {
		t1xp=0; t2xp=0;
		if(t1x<t2x) { minx=t1x; maxx=t2x; }
		else		{ minx=t2x; maxx=t1x; }
	    // process first line until y value is about to change
		while(i<dx1) {
    		e1 += dy1;
	   	   	while (e1 >= dx1) {
				e1 -= dx1;
   	   	   	   	if (changed1) { t1xp=signx1; break; }//t1x += signx1;
				else          goto next3;
			}
			if (changed1) break;
			else   	   	  t1x += signx1;
			if(i<dx1) i++;
		}
	next3:
        // process second line until y value is about to change
		while (t2x!=x3) {
			e2 += dy2;
	   	   	while (e2 >= dx2) {
				e2 -= dx2;
				if(changed2) t2xp=signx2;
				else          goto next4;
			}
			if (changed2)     break;
			else              t2x += signx2;
		}	   	   
	next4:

		if(minx>t1x) minx=t1x; if(minx>t2x) minx=t2x;
		if(maxx<t1x) maxx=t1x; if(maxx<t2x) maxx=t2x;
	   	lcd_hline(minx, maxx, y,c);    // Draw line from min to max points found on the y
		// Now increase y
		if(!changed1) t1x += signx1;
		t1x+=t1xp;
		if(!changed2) t2x += signx2;
		t2x+=t2xp;
    	y += 1;
		if(y>y3) return;
	}
}

/*
// Fill a triangle - slope method
void fillTriangleslope(uint8_t x0, uint8_t y0,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
 	uint8_t a, b, y, last;
  	// Sort coordinates by Y order (y2 >= y1 >= y0)
  	if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
  	if (y1 > y2) { SWAP(y2, y1); SWAP(x2, x1); }
  	if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
  
  	if(y0 == y2) { // All on same line case
    	a = b = x0;
    	if(x1 < a)      a = x1;
    	else if(x1 > b) b = x1;
    	if(x2 < a)      a = x2;
    	else if(x2 > b) b = x2;
        lcd_hline(a, b, y0);
        return;
    }

    int8_t
        dx01 = x1 - x0,
        dy01 = y1 - y0,
        dx02 = x2 - x0,
        dy02 = y2 - y0,
        dx12 = x2 - x1,
        dy12 = y2 - y1;
    int16_t sa = 0, sb = 0;

    // For upper part of triangle, find scanline crossings for segment
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y
    // is included here (and second loop will be skipped, avoiding a /
    // error there), otherwise scanline y1 is skipped here and handle
    // in the second loop...which also avoids a /0 error here if y0=y
    // (flat-topped triangle)
    if(y1 == y2) last = y1;   // Include y1 scanline
    else         last = y1-1; // Skip it

    for(y=y0; y<=last; y++) {
        a   = x0 + sa / dy01;
        b   = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        // longhand a = x0 + (x1 - x0) * (y - y0) / (y1 - y0)
        //          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
        lcd_hline(a, b, y);
    }

    // For lower part of triangle, find scanline crossings for segment
    // 0-2 and 1-2.  This loop is skipped if y1=y2
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for(; y<=y2; y++) {
        a   = x1 + sa / dy12;
        b   = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        // longhand a = x1 + (x2 - x1) * (y - y1) / (y2 - y1)
        //          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
        lcd_hline(a, b, y);
    }
}
*/

/***********************************************************************
Draws a circle with center at x,y with given radius.
Set show to 1 to draw pixel, set to 0 to hide pixel.
************************************************************************/
void lcd_circle(uint8_t x, uint8_t y, uint8_t radius, uint8_t c) {
    uint8_t xc = 0;
    uint8_t yc = radius;
    int p = 3 - (radius<<1);
    while (xc <= yc) {
        set_pixelc(x + xc, y + yc, c);
        set_pixelc(x + xc, y - yc, c);
        set_pixelc(x - xc, y + yc, c);
        set_pixelc(x - xc, y - yc, c);
        set_pixelc(x + yc, y + xc, c);
        set_pixelc(x + yc, y - xc, c);
        set_pixelc(x - yc, y + xc, c);
        set_pixelc(x - yc, y - xc, c);
        if (p < 0) p += (xc++ << 2) + 6;
        else p += ((xc++ - yc--)<<2) + 10;
    }
}


/**************************************************************************
 *  circle_fill                                                           *
 *    Draws and fills a circle.                                           *
 **************************************************************************/
void circle_fill(uint8_t x,uint8_t y, uint8_t radius, uint8_t c) {
    uint8_t xc = 0;
    uint8_t yc = radius;
    int p = 3 - (radius<<1);
    while (xc <= yc) {
		lcd_hline(x - xc, x + xc, y - yc, c);
		lcd_hline(x - xc, x + xc, y + yc, c);
		lcd_hline(x - yc, x + yc, y - xc, c);
		lcd_hline(x - yc, x + yc, y + xc, c);
        if (p < 0) p += (xc++ << 2) + 6;
        else p += ((xc++ - yc--)<<2) + 10;
    }
}


/*-------------------------------------------------------------------------------
Print a char on the LCD
	GLCD_Putchar (uint8_t u8Char)
		u8Char = char to display
-------------------------------------------------------------------------------*/
void GLCD_Putchar(char u8Char) {
    uint16_t pointer;
	uint8_t data,u8CharColumn=0;
	pointer = (unsigned int)(Fonts)+(u8Char-20)*(3);
    if(u8Char!='\n') {
       	/* Draw a char */
    	while (u8CharColumn < 3)	{
            data = pgm_read_byte_near(pointer++);
		    if(testbit(Misc,negative)) data = ~(data|128);
		    write_display(data);
		    u8CharColumn++;
	    }
    }
    // Special characters
    if(u8Char==0x1C) {       // Begin long 'd' character
        write_display(0x30);
    }
    else if(u8Char==0x1D) {  // Complete long 'd' character
        write_display(0x38);
        u8CursorX++;
    }
    else if(u8Char==0x1A) {  // Complete long 'm' character
        write_display(0x08);
    }
    else if(u8CursorX < 128) {  // if not then insert a space before next letter
		data = 0;
		if(testbit(Misc,negative)) data = 127;
		write_display(data);
	}
    if(u8CursorX>=128 || u8Char=='\n') {    // Next line
        u8CursorX = 0; u8CursorY++;
    }
}

/*-------------------------------------------------------------------------------
Print a char on the LCD
	GLCD_Putchar (uint8_t u8Char)
		u8Char = char to display
-------------------------------------------------------------------------------*/
void GLCD_Bigchar (char u8Char) {
	if(u8Char=='.') {           // Small point to Save space
		write_display (0x60);
		write_display (0x60);
		u8CursorX+=2;
	}
	else if(u8Char=='-') {      // Negative sign
		write_display (0x03);
		write_display (0x03);
		write_display (0x03);
		write_display (0x03);
		u8CursorX+=2;
    }
	else if(u8Char==' ') {      // Space
		u8CursorX+=6;
    }
	else {                      // Number
        uint8_t i=0;
        uint16_t pointer = (unsigned int)(BigFonts)+(u8Char)*20;
		// Upper side
		u8CursorY--;
		while (i < 10) { write_display (pgm_read_byte_near(pointer++)); i++; }
		i=0;
		// Lower Side
		u8CursorY++;
		u8CursorX-=10;
		while (i < 10) { write_display(pgm_read_byte_near(pointer++)); i++; }
        u8CursorX+=2;
	}
}

/*-------------------------------------------------------------------------------
Print a string on the LCD from a string in program memory
	GLCD_Printf (uint8_t *au8Text) 
		*au8Text = string to display
-------------------------------------------------------------------------------*/
void lcd_putsp (const char *ptr) {
    char c;
    while ((c=pgm_read_byte(ptr++)) != 0x00) {
        GLCD_Putchar(c);
    }
}

/*-------------------------------------------------------------------------------
Print a char on the LCD
	GLCD_Putchar (uint8_t u8Char)
		u8Char = char to display
-------------------------------------------------------------------------------*/
void putchar5x8(char u8Char) {
    uint16_t pointer;
	uint8_t data,u8CharColumn=0;
	pointer = (unsigned int)(Font5x8)+(u8Char-0x20)*(5);
    if(u8Char!='\n') {
       	/* Draw a char */
    	while (u8CharColumn < 5)	{
            data = pgm_read_byte_near(pointer++);
		    if(testbit(Misc,negative)) data = ~(data|128);
		    write_displayg(data);
		    u8CharColumn++;
	    }
    }
    if(u8CursorX < 128) {  // Insert a space before next letter
		data = 0;
		if(testbit(Misc,negative)) data = 127;
		write_displayg(data);
	}
    if(u8CursorX>=128 || u8Char=='\n') {    // Next line
        u8CursorX = 0; u8CursorY++;
    }
}

/*-------------------------------------------------------------------------------
Print a string on the LCD from a string in program memory
	GLCD_Printf (uint8_t *au8Text) 
		*au8Text = string to display
-------------------------------------------------------------------------------*/
void lcd_put5x8 (const char *ptr) {
    char c;
    while ((c=pgm_read_byte(ptr++)) != 0x00) {
        putchar5x8(c);
    }
}

// Print Number
void printN(uint8_t Data) {
    uint8_t d=0x30;
	while (Data>=100)	{ d++; Data-=100; }
    if(d>0x30) GLCD_Putchar(d);
	d=0x30;
	while (Data>=10)	{ d++; Data-=10; }
    GLCD_Putchar(d);
    GLCD_Putchar(Data+0x30);
}

// Print Number
void printN5x8(uint8_t Data) {
    uint8_t d=0x30;
	while (Data>=100)	{ d++; Data-=100; }
    if(d!=0x30) putchar5x8(d);
    d=0x30;
	while (Data>=10)	{ d++; Data-=10; }
    putchar5x8(d);
    putchar5x8(Data+0x30);
}

extern const uint16_t milivolts[];

// Print Voltage
void printV(int16_t Data, uint8_t gain) {
    printF(u8CursorX,u8CursorY,(int32_t)Data*milivolts[gain]/8);
}    

// Print Fixed point Number with 5 digits
// or Print Long integer with 7 digits
void printF(uint8_t x, uint8_t y, int32_t Data) {
	uint8_t D[8]={0,0,0,0,0,0,0,0},point=0;
    lcd_goto(x,y);
    if(Data<0) {
        Data=-Data;
        if(testbit(Misc,bigfont)) GLCD_Bigchar('-');
        else GLCD_Putchar('-');
    }
    else {
        if(testbit(Misc,bigfont)) GLCD_Bigchar(' ');
        else GLCD_Putchar(' ');
    }
    if(testbit(Misc,negative)) {   // 7 digit display
        point=3;
    }
    else {  // 4 digit display
	    if(Data>=999900000L) Data = 9999;
	    else if(Data>=100000000L)  Data = (Data/100000);
	    else if(Data>=10000000L) {
    	    Data = (Data/10000);
    	    point = 1;
	    }
	    else if(Data>=1000000L) {
    	    Data = (Data/1000);
    	    point = 2;
	    }
	    else {
    	    Data = (Data/100);
    	    point = 3;
	    }
    }
    
    uint8_t i=7;
    do {    // Decompose number
        uint32_t power;
        power=pgm_read_dword_near(Powersof10+i);
        while(Data>=power) { D[i]++; Data-=power; }
    } while(i--);

    if(testbit(Misc, negative)) i=7;    // To print all digits
    else i=3;
	for(; i!=255; i--) {
		if(testbit(Misc,bigfont)) {
			GLCD_Bigchar(D[i]);
			if(point==i) GLCD_Bigchar('.');
		}
		else {
			GLCD_Putchar(0x30+D[i]);
			if(point==i) GLCD_Putchar('.');
		}
	}
}

// Print small font text at x,y from program memory
void tiny_printp(uint8_t x, uint8_t y, const char *ptr) {
    lcd_goto(x,y);
    lcd_putsp(ptr);
}

/*----------------------------------------------------------------------------
Send a run length encoded image from program memory to the LCD
	Decode algorithm:
	Get one byte, put it to the output file, and now it's the 'last' byte. 
	Loop 
	Get one byte 
	Is the current byte equal to last? 
	Yes 
		Now get another byte, this is 'counter' 
		Put current byte in the output file 
		Copy 'counter' times the 'last' byte to the output file 
		Put last copied byte in 'last' (or leave it alone) 
	No 
		Put current byte to the output file 
		Now 'last' is the current byte 
	Repeat. 

    BMP[0] contains width/8
    BMP[1] contains height
----------------------------------------------------------------------------*/
void bitmap(uint8_t x, uint8_t y, const uint8_t *BMP) {
    uint8_t *p;
  	uint8_t row=0,col=0;
	uint8_t data=0,count=0;
	uint8_t width,height;
    uint8_t const *b;
    width=pgm_read_byte(&BMP[0]);
    height=pgm_read_byte(&BMP[1])/8;
    b=&BMP[2];
    p= &Disp_send.buffer[(uint16_t)((127-x)*18)+y];
  	for ( ; col < width; col++) {
		for (row=0; row<height; row++) {
			if(count==0) {
				data = pgm_read_byte(b++);
				if(data==pgm_read_byte(b++)) {
					count = pgm_read_byte(b++);
				}
				else {
					count = 1;
					b--;
				}
			}
			count--;
            *p++=data;
		}
        p-=18+height;   // Next line
	}
}

/*----------------------------------------------------------------------------
Send a run length encoded image from program memory to the LCD
	Decode algorithm:
	Get one byte, put it to the output file, and now it's the 'last' byte. 
	Loop 
	Get one byte 
	Is the current byte equal to last? 
	Yes 
		Now get another byte, this is 'counter' 
		Put current byte in the output file 
		Copy 'counter' times the 'last' byte to the output file 
		Put last copied byte in 'last' (or leave it alone) 
	No 
		Put current byte to the output file 
		Now 'last' is the current byte 
	Repeat. 

    BMP[0] contains width/8
    BMP[1] contains height
----------------------------------------------------------------------------*/
void bitmap_safe(int8_t x, int8_t y, const uint8_t *BMP, uint8_t c) {
    uint8_t *p;
  	uint8_t row=0,col=0;
	uint8_t data=0,count=0;
    int16_t width,height;
	uint8_t const *b;
    width=pgm_read_byte(&BMP[0]);
    height=pgm_read_byte(&BMP[1])/8;
    b=&BMP[2];    
    if(BMP==0 || width<0 || y+height<=0) return;
    p= &Disp_send.buffer[(uint16_t)((127-x)*18)+y];
  	for ( ; col < width; col++) {
		for (row=0; row<height; row++) {
			if(count==0) {
				data = pgm_read_byte(b++);
				if(data==pgm_read_byte(b++)) {
					count = pgm_read_byte(b++);
				}
				else {
					count = 1;
					b--;
				}
			}
			count--;
//            if(x>=0 && x<128 && (y+row)>=0 && (y+row)<16) {
            if(p>=Disp_send.buffer && p<(Disp_send.buffer+18*128)) {
                if(c==0)        *p &= data;
                else if(c==1)   *p |= data;
                else if(c==2)   *p &= ~data;
                else if(c==3)   *p |= ~data;
                else if(c==4)   *p ^= data;
                else            *p = data;
            }
            p++;
		}
        p-=18+height;   // Next line
	}
}