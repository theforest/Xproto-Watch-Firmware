// Hardware specific definitions

#ifndef HARDWARE_H
#define HARDWARE_H

#define MENUPULL    0x18    // Menu button Pull down, invert pin

#define     LCDVDD          5           // LCD VDD
#define     LCD_DISP        2           // DISPLAY ON / OFF
#define		LCD_CS		    0           // Chip select
#define		LCD_CTRL        VPORT3.OUT

// PORT DEFINITIONS
#define LEDGRN      1           // PORTD
#define LEDRED      2           // PORTD
#define ANPOW       6           // PORTB
#define LOGIC_DIR   7           // PORTB
#define EXTCOMM     4           // PORTD

#define ONGRN()         setbit(VPORT1.OUT, LEDGRN)
#define OFFGRN()        clrbit(VPORT1.OUT, LEDGRN)
#define ONRED()         setbit(VPORT1.OUT, LEDRED)
#define OFFRED()        clrbit(VPORT1.OUT, LEDRED)
#define ANALOG_ON()     setbit(VPORT0.OUT, ANPOW)
#define ANALOG_OFF()    clrbit(VPORT0.OUT, ANPOW)
#define LOGIC_DIROUT()  clrbit(VPORT0.OUT, LOGIC_DIR)
#define LOGIC_DIRIN()   setbit(VPORT0.OUT, LOGIC_DIR)
#define EXTCOMMH()      setbit(VPORT3.OUT, EXTCOMM)
#define EXTCOMML()      clrbit(VPORT3.OUT, EXTCOMM)
#define SECPULSE()      testbit(VPORT3.OUT, EXTCOMM)    // Half second high, Half second low

// Port definitions for Assembly code

#define EXTPIN 0x0012,2 // External trigger pin is VPORT0.2
#define CH1ADC 0x0224   // ADCA CH0.RESL
#define CH2ADC 0x0264   // ADCB CH0.RESL

#endif
