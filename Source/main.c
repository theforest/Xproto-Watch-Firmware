/*****************************************************************************

Oscilloscope Watch

Gabotronics
February 2014

Copyright 2014 Gabriel Anzziani

This program is distributed under the terms of the GNU General Public License

ATXMEGA256A3U

Compiled with GCC, -Os optimizations

www.gabotronics.com
email me at: gabriel@gabotronics.com

*****************************************************************************/

// TODO & Optimizations:
/*      Sniffer IRDA, 1 Wire, MIDI        
        Gain Calibration
		PC control digital lines -> bus driver! SPI / I2C / UART ...
        Custom AWG backup in User Signature Row
        UART Auto baud rate
        Programmer mode
        If no sleeptime, check if menu timeout works
        Check if pretrigger samples completed with DMA (last part of mso.c)
        Make Srate signed, so tests like Srate>=11 become Srate>=0
        Share buffer between CH1 and CH2
        MSO Logic Analyzer more SPS, with DMA
        Force trigger, Trigger timeout in menu
		USB Frame counter
        Disable gain on CH1 if Gain=1, to allow simultaneous sampling
        Channel math in meter mode
		USE NVM functions from BOOT */
// TODO When 64k parts come out:
/*      Vertical zoom
        Use as a programmer
        Terminal mode
        FFT waterfall
        Bigger fonts
        Setting profiles
        Continuous data to USB from ADC
        Independent CH1 and CH2 Frequency measurements, up to 1Mhz
        1v/octave (CV/Gate) AWG control
        RMS
        Arbitrary math expression on AWG
        DAC Calibration
	    Use DMA for USART PC transfers
        Dedicated Bode plots
        Dedicated VI Curve
	    12bit with slow sampling
        Horizontal cursor on FFT
        16MSPS for logic data, 1/sinc(x) for analog
	    Show menu title */
// TODO Expansion boards: SD, Keyboard, Display, RS232, MIDI, Video, RAM

/* Hardware resources:
    Timers:
        RTC   Half second timer
        TCC0  Frequency counter time keeper
            Also used as split timer, source is Event CH7 (40.96mS)
            TCC0L Controls the auto trigger
            TCC0H Auto key repeat
        TCC1  Counts post trigger samples
              UART sniffer time base
              Frequency counter low 16bits
        TCD0  Split timer, source is Event CH6 (1.024ms)
            TCD0L 40.96mS period - 24.4140625 Hz - Source for Event CH7
	        TCD0H Controls LCD refresh rate
	    TCD1  Overflow used for AWG
        TCE0  Controls Interrupt ADC (srate >= 11), srate: 6, 7, 8, 9, 10
              Fixed value for slow sampling
              Frequency counter high 16bits
        TCE1L 
        TCE1H 
        TCF0  Half Days counter
    Events:
	    CH0 TCE0 overflow used for ADC
	    CH1 ADCA CH0 conversion complete
        CH2 Input pin for frequency measuring
        CH3 TCD1 overflow used for DAC
        CH4 TCC0 overflow used for freq. measuring
        CH5 TCC1 overflow used for freq. measuring
        CH6 CLKPER / 32768 -> every 1.024ms
        CH7 TCD0L underflow: 40.96mS period - 24.4140625 Hz
	DMAs:
	    CH0 ADC CH0  / SPI Sniffer MOSI / UART Sniffer
	    CH1 ADC CH1  / SPI Sniffer MISO
	    CH2 Port CHD / Display
	    CH3 AWG DAC
    USART:
	    USARTD0 for Memory LCD
	    USARTC0 for Sniffer
	    USARTE0 for External Interface Port
	RAM:
	    1024:   Display buffer
         128:   Endpoint0 out + in
         128:   Endpoint1 out + in
         768:   CH1+CH2+CHD Data
         256:   AWG Buffer
          31:   M
		1536:   Temp (FFT, logic sniffer)
        -----
        3871    Total + plus some global variables
    Interrupt Levels:
        TCE0:           High        Slow Sampling
        PORTC_INT1      High        SPI sniffer
        PORTC INT0      High        I2C sniffer
        TCC1            High        UART sniffer
        USARTE0:        Medium      Serial port
        USB BUSEVENT    Medium      USB Bus Event
        USB_TRNCOMPL    Medium      USB Transaction Complete
        PORTA INT0:     Medium      keys
        TCC0L:          Low         auto trigger
        TCC0H:          Low         auto keys
        RTC:            Low         sleep timeout, menu timeout
        TCD0H:          Low         Wake from sleep (LCD refresh rate)
*/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/fuse.h>
#include "main.h"
#include "mso.h"
#include "logic.h"
#include "awg.h"
#include "USB\usb_xmega.h"
#include "USB\Descriptors.h"
#include "time.h"

const char mainmenutxt[][18] PROGMEM = {           // Menus:
    "  Digital Watch  ",    // Watch
    "  Oscilloscope   ",    // Oscilloscope
    "Protocol Sniffer ",    // Sniffer
    "Frequency Counter",    // Counter
    "      Games      ",    // Games
    "     Settings    ",
};

const char optionmenutxt[][22] PROGMEM = {           // Menus:
    "Time  Calendar  Tasks",    // Watch
    "Scope   Curve   Meter",    // Oscilloscope
    "I2C      SPI     UART",    // Sniffer
    "Freq   Counter Period",    // Counter
    "Blocks  Space   Chess",    // Games
    "Config Diagnose About",
};
  
/*

turn off JTAG !!!


FUSES = {
    .FUSEBYTE0 = 0xFF,  // JTAG - Not used
    .FUSEBYTE1 = 0x00,  // Watchdog Configuration
    .FUSEBYTE2 = 0xBD,  // Reset Configuration
    .FUSEBYTE4 = 0xF7,  // Start-up Configuration
    .FUSEBYTE5 = 0xE9,  // EESAVE and BOD Level
};/
/*  Fuses:
    WDWP = 8CLK
    WDP = 8CLK
    BOOTRST = BOOTLDR
    TOSCSEL = XTAL
    BODPD = OFF
    RSTDISBL = [ ]
    SUT = 4MS
    WDLOCK = [ ]
    BODACT = SAMPLED
    EESAVE = [ ]
    BODLVL = 2V6    */

uint8_t SP_ReadCalibrationByte(uint8_t location);
static inline void Restore(void);
//uint16_t readVCC(void);

// Big buffer to store large but temporary data
TempData Temp;

// Variables that need to be saved in NVM
NVMVAR M;

// EEProm variables
uint8_t EEMEM EESleepTime = 32;     // Sleep timeout in minutes
uint8_t EEMEM EEDACgain   = 0;      // DAC gain calibration
uint8_t EEMEM EEDACoffset = 0;      // DAC offset calibration
uint8_t EEMEM EECalibrated = 0xFF;  // Offset calibration done

//static void CalibrateDAC(void);
void SimpleADC(void);
static inline void LoadEE(void);                  // Load settings from EEPROM
void CalibrateOffset(void);
void CalibrateGain(void);
    
int main(void) {
    PR.PRGEN = 0b01011000;          // Stop: USB, AES, EBI, EVSYS, only RTC and DMA on
    PR.PRPA  = 0b00000111;          // Stop: DAC, ADC, AC
    PR.PRPB  = 0b00000111;          // Stop: DAC, ADC, AC
    PR.PRPC  = 0b11111111;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPD  = 0b11101111;          // Stop: TWI,       , USART1, SPI, HIRES, TC1, TC0
    PR.PRPE  = 0b11111111;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPF  = 0b11111110;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    NVM.CTRLB = 0b00001100;         // EEPROM Mapping, Turn off bootloader flash
    // PORTS CONFIGURATION
    PORTA.DIR       = 0b00101110;   // 1.024V, BATTSENSE, x, CH1, x, x, x, 2.048V
    PORTA.PIN4CTRL  = 0x07;         // Input Disable on pin PA4
    PORTA.PIN6CTRL  = 0x07;         // Input Disable on pin PA6
    PORTA.PIN7CTRL  = 0x07;         // Input Disable on pin PA7
    PORTB.DIR       = 0b11000011;   // LOGICDIR, ANPOW, CH2, 1.024V, AWG, TRIG, x, x
    PORTB.PIN2CTRL  = 0x01;         // Sense rising edge (Freq. counter)    
	PORTB.PIN3CTRL	= 0x07;         // Input Disable on pin PB3
    PORTB.PIN4CTRL  = 0x07;         // Input Disable on pin PB4
    PORTB.PIN5CTRL  = 0x07;         // Input Disable on pin PB5
    PORTB.OUT       = 0b10000000;   // Logic port as input, Analog power off
    //PORTC.DIR = 0b00000000;       // LOGIC, register initial value is 0
    PORTC.INT0MASK  = 0x01;         // PC0 (SDA) will be the interrupt 0 source
    PORTC.INT1MASK  = 0x80;         // PC7 (SCK) will be the interrupt 1 source
    PORTD.DIR       = 0b00111111;   // D+, D-, LCDVDD, EXTCOMM, LCDIN, LCDDISP, LCDCLK, LCDCS
    PORTD.OUT       = 0b00100000;   // Power to LCD
    PORTE.DIR       = 0b00111111;   // Crystal, crystal, buzzer, buzzer, BATTSENSEPOW, RED, GRN, WHT
    // PORTE.PIN4CTRL = 0x40; // Invert output
    // PORTE.OUT       = 0x20;
    // PORTF.DIR       = 0x00;      // Switches
    PORTCFG.MPCMASK = 0xFF;
    PORTF.PIN0CTRL  = 0x18;         // Pull up on pin Port F
    PORTF.INTCTRL   = 0x02;         // PORTA will generate medium level interrupts
    PORTF.INT0MASK  = 0xFF;         // All inputs are the source for the interrupt
    PORTCFG.VPCTRLA = 0x41;         // VP1 Map to PORTE, VP0 Map to PORTB
    PORTCFG.VPCTRLB = 0x32;         // VP3 Map to PORTD, VP2 Map to PORTC    

    // Initialize LCD
    GLCD_LcdInit();
	SwitchBuffer();
    //memcpy_P(Disp_send.display_data+286,  &LOGO, 69);   // Gabotronics
	//GLCD_setting();
    //tiny_printp(50,7,VERSION);
    LoadEE();
    CPU_Slow();
    
    DMA.CTRL          = 0x80;       // Enable DMA, single buffer, round robin
    
    // TIME CONTROL!
    CLK.RTCCTRL = CLK_RTCSRC_TOSC_gc | CLK_RTCEN_bm;    // Set 1.024kHz from external 32.768kHz crystal oscillator
    RTC.PER = 511;                  // 1024/2-1 = 1 seconds
    RTC.COMP = 255;
    RTC.CTRL = 0x02;                // Prescale by 2 -> 512Hz
    RTC.INTCTRL = 0x05;             // Generate low level interrupts (Overflow and Compare)
    EVSYS.CH4MUX    = 0x08;         // Event CH4 = RTC overflow (every second)
    TCF0.PER = 43199;               // 43200 seconds = 12 hours
    TCF0.CTRLA = 0x0C;              // Source is Event CH4
    TCF0.INTCTRLA = 0x01;           // 12 hour interrupt, low level interrupt
    
    PMIC.CTRL = 0x07;               // Enable High, Medium and Low level interrupts
    sei();                          // Enable global interrupts    

    if(eeprom_read_byte(&EECalibrated)) CalibrateOffset();

/*
    uint8_t i;
    i=eeprom_read_byte(&EESleepTime);
    // Check if KD pressed
    //if(!testbit(PORTA.IN,1)) setbit(Misc,bigfont);   // Use bigfont bit to enter settings
    Key=0;  // Without this, you need to press KD before power up
    while(testbit(Misc, bigfont)) {
        uint8_t Volt;
        clr_display();
        ADCA.CTRLA          = 0x01;     // Enable ADC
        ADCA.CH3.MUXCTRL    = 0x28;     // PA5 (2.048 / 2)
        ADCA.PRESCALER    = 0x07;   // Prescaler 512 (62.5kHZ ADC clock)
        ADCA.CTRLB        = 0x14;   // signed mode, no free run, 8 bit
        // 2.048V measure
        ADCA.REFCTRL      = 0x10;   // REF= VCC/1.6 (~2.06V)
        ADCA.CH3.CTRL     = 0x81;   // Start conversion, single ended input
        _delay_ms(1);
        Volt = ADCA.CH3.RESL;
        printF(64,1,(uint32_t)Volt*3248);     // Convert to volts (V * 2.0625*2/127)
//        lcd_goto(0,0); printhex(VPORT2.IN);   // Shows the logic input data
        //tiny_printp(0,3,PSTR("rev")); GLCD_Putchar('A'+MCU.REVID);
        tiny_printp(0,6,PSTR("OFFSET       SLEEP:      RESTORE"));
        u8CursorX=58;   // Already have a new line from previous print
        if(i) printN(i);    // Sleep timeout
        else GLCD_Putchar('-');
        if(testbit(Key, userinput)) {
            clrbit(Key, userinput);
            if(testbit(Key,K1)) Calibrate();
            if(testbit(Key,K2)) i+=16;
            if(testbit(Key,K3)) Restore();
            if(testbit(Key,KM)) {
                eeprom_write_byte(&EESleepTime, i);
                clrbit(Misc,bigfont);   // set bit to normal value
            }
        }
        show_display(); WaitDisplay();
    }*/
    Key=K1;
    uint8_t item=2, animating=0;   // Menu item
    uint8_t old_item=0;
    int8_t step=15,from=-101;
	
    for(;;) {
        // Check User Input
        if(testbit(Key, userinput)) {
            clrbit(Key, userinput);
            old_item=item;;
            if(testbit(Key,KD)) {
                item++;
                if(item>6) item=1;
                step=-15;
                from=133;                
            }
            if(testbit(Key,KM) || testbit(Key,KI)) {
                if(item<=1) item=6; else item--;
                step=15;
                from=-101;
            }
            switch(item) {
                case 1:     // Watch Menu
                    if(testbit(Key,K1)) {
                        WATCH();           // go to WATCH
                        old_item=0; step=15; from=-101;
                    }
                    if(testbit(Key,K2)) {
                        CALENDAR();           // go to WATCH
                        old_item=0; step=15; from=-101;
                    }
                break;
                case 2:     // Oscilloscope Menu
                    if(testbit(Key,K1)) {
                        AnalogOn();
                        RTC.INTCTRL = 0x00;     // Disable RTC interrupts
                        // 1Hz Memory LCD EXTCOMM
                        TCD0.CTRLB = 0b00010000;            // Enable HCMPENA, pin4
                        TCD0.CCAH = 128;                      // Automatic EXTCOMM with Timer D0                        
                        MSO();              // go to MSO
                        TCD0.CTRLB = 0;
                        TCD0.CCAH = 0;
                        RTC.INTCTRL = 0x05;
                        AnalogOff();
                        old_item=0; step=15; from=-101;
                    }                        
                break;
                case 3:     // Sniffer Menu
                break;
                case 4:     // Frequency Counter Menu
                break;
                case 5:     // Games Menu
                    if(testbit(Key,K3)) {
                        CPU_Fast();
                        CHESS();              // go to Chess
                        CPU_Slow();
                        old_item=0; step=15; from=-101;
                    }                
                break;
                case 6:     // Settings Menu
                break;
            }
        }
        if(item!=old_item) {
			PR.PRPE  = 0x6C;        // Stop: TWI,       , USART1, SPI, HIRES
            TCE1.CTRLB = 0b00110001;        // Enable output compares, waveform mode
            TCE1.CCA = 250;                 // 4KHz (CPU @ 2MHz)
            TCE1.CTRLA = 0x02;              // Enable timer
            for(int8_t n=step; n<118 && n>-118; n+=step) {
                if(step>1) step--;
                if(step<-1) step++;
                dma_display();
                SwitchBuffer();
                clr_display();
                bitmap_safe(16+n,4,(int16_t)pgm_read_word(Icons+old_item),1);
                bitmap_safe(from+n,4,(int16_t)pgm_read_word(Icons+item),5);
                WaitDisplay();
                TCE1.CTRLA = 0;             // Disable timer
                TCE1.CTRLB = 0;
				PR.PRPE  = 0xFF;
                PORTE.OUT = 0x00;
            }
            lcd_goto(16,2);
            lcd_put5x8(mainmenutxt[item-1]);
            lcd_goto(0,15);
            lcd_put5x8(optionmenutxt[item-1]);
            WaitDisplay();
            dma_display();
            old_item=item;            
        }
        WaitDisplay();
        SLEEP.CTRL = SLEEP_SMODE_PSAVE_gc | SLEEP_SEN_bm;
        asm("sleep");
        asm("nop");
    }        
    return 0;
}

/*
// Waits for the DMA to complete (the DMA's ISR will SET LCD_CS)
void WaitDisplay(void) {
    uint16_t n=0;
    while(testbit(LCD_CTRL,LCD_CS)) {   // Wait for transfer complete
        _delay_us(1);
        n++;
        if(n==20000) break;     // timeout ~ 20mS
    }
    OFFGRN();
    OFFRED();    
}
*/
// Tactile Switches - This is configured as medium level interrupt
ISR(PORTF_INT0_vect) {
    uint8_t i,in,j=0;
    // Debounce: need to read 10 consecutive equal numbers
    OFFGRN();   // Avoid having the LED on during the interrupt
    OFFRED();
    for(i=25; i>0; i--) {
        delay_ms(1);
		in = PORTF.IN;              // Read port
		if(j!=in) { j=in; i=10; }   // Value changed
	}
    Key=0;
    if(!testbit(in,4)) setbit(Key,KM);  // Menu key
    if(!testbit(in,0)) setbit(Key,KD);  // Decrease
    if(!testbit(in,6)) setbit(Key,KI);  // Increase
    if(!testbit(in,5)) setbit(Key,KB);  // Back
    if(!testbit(in,7)) {  // Light
        setbit(VPORT1.OUT,0);
        delay_ms(1000);
        clrbit(VPORT1.OUT,0);
		if(!testbit(PORTF.IN, 6)) Jump_boot();
    }        
    //if(!testbit(in,7))  // Light
    if(testbit(Display, flip)) {
        if(!testbit(in,3)) setbit(Key,K3);
        if(!testbit(in,2)) setbit(Key,K2);
        if(!testbit(in,1)) setbit(Key,K1);
    }
    else {
        if(!testbit(in,3)) setbit(Key,K1);
        if(!testbit(in,2)) setbit(Key,K2);
        if(!testbit(in,1)) setbit(Key,K3);
    }
    if(Key) {
        setbit(MStatus, update);         // Valid key
        setbit(Key, userinput);
        // TD0H used for auto repeat key
        if(TCC0.CTRLE!=0) {             // Not doing a frequency count
            TCC0.CNTH = 24;                             // Restart timer
            setbit(TCC0.INTFLAGS, TC2_HUNFIF_bp);       // Clear trigger timeout interrupt
            TCC0.INTCTRLA |= TC2_HUNFINTLVL_LO_gc;      // Enable Auto Key interrupt
        }
    }
    else clrbit(Misc,keyrep);
    //RTC.CNT=0;  // Clear screen saver timer
}

// Auto repeat key
ISR(TCC2_HUNF_vect) {
    TCC0.INTCTRLA &= ~TC2_HUNFINTLVL_LO_gc;         // Disable Auto Key interrupt
    if(Key) setbit(Misc,keyrep);
}

void CPU_Fast(void) {
    // Clock Settings
    OSC.XOSCCTRL = 0xCB;    // Crystal type 0.4-16 MHz XTAL - 16K CLK Start Up time
    OSC.PLLCTRL = 0xC2;     // XOSC is PLL Source - 2x Factor (32MHz)
    OSC.CTRL |= OSC_RC2MEN_bm | OSC_XOSCEN_bm;
    delay_ms(2);
    // Switch to internal 2MHz if crystal fails
    if(!testbit(OSC.STATUS,OSC_XOSCRDY_bp)) {   // Check XT ready flag
        OSC.XOSCCTRL = 0x00;    // Disable external oscillators
        // Not entering, comment to save
        //OSC.PLLCTRL = 0x10;     // 2MHz is PLL Source - 16x Factor (32MHz)
    }
    OSC.CTRL = OSC_RC2MEN_bm | OSC_RC32MEN_bm | OSC_PLLEN_bm | OSC_XOSCEN_bm;
    delay_ms(2);
    CCPWrite(&CLK.CTRL, CLK_SCLKSEL_PLL_gc);    // Switch to PLL clock
    // Clock OK!
    OSC.CTRL = OSC_RC32MEN_bm | OSC_PLLEN_bm | OSC_XOSCEN_bm;    // Disable internal 2MHz

    if(CLK.CTRL & CLK_SCLKSEL_RC32M_gc) {   // Clock error?
        tiny_printp(0,7,PSTR("NO XT"));
    }
    USARTD0.BAUDCTRLA = FBAUD32M;	    // SPI clock rate for display, CPU is at 32MHz    
    NVM.CTRLB = 0b00001100;             // EEPROM Mapping, Turn off bootloader flash, Turn on EEPROM
}

void CPU_Slow(void) {
    OSC.CTRL |= OSC_RC2MEN_bm;  // Enable internal 2MHz
    delay_ms(2);
    CCPWrite(&CLK.CTRL, CLK_SCLKSEL_RC2M_gc);    // Switch to 2MHz clock
    OSC.CTRL = OSC_RC2MEN_bm;
    OSC.PLLCTRL = 0;
    OSC.XOSCCTRL = 0;
    USARTD0.BAUDCTRLA = FBAUD2M;	    // SPI clock rate for display, CPU is at 2MHz
    NVM.CTRLB = 0b00001110;             // EEPROM Mapping, Turn off bootloader flash, Turn off EEPROM
}

void AnalogOn(void) {
    ANALOG_ON();
    // Power reduction: Stop unused peripherals
    PR.PRGEN = 0x18;        // Stop: AES, EBI
    PR.PRPA  = 0x04;        // Stop: DAC
    PR.PRPB  = 0x01;        // Stop: AC
    PR.PRPC  = 0x7C;        // Stop: TWI, USART0, USART1, SPI, HIRES
    PR.PRPD  = 0x6C;        // Stop: TWI,       , USART1, SPI, HIRES
    PR.PRPE  = 0x6C;        // Stop: TWI,       , USART1, SPI, HIRES
    CPU_Fast();
}

void AnalogOff(void) {
    ANALOG_OFF();
    DMA.CTRL          = 0x00;       // Disable DMA
    DMA.CTRL          = 0x40;       // Reset DMA
    ADCA.CTRLA        = 0x00;       // Disable ADC
    ADCB.CTRLA        = 0x00;       // Disable ADC
    // POWER REDUCTION: Stop unused peripherals - Stop everything but RTC and DMA
    PR.PRGEN = 0b01011000;          // Stop: USB, AES, EBI, EVSYS, only RTC and DMA on
    PR.PRPA  = 0b00000111;          // Stop: DAC, ADC, AC
    PR.PRPB  = 0b00000111;          // Stop: DAC, ADC, AC
    PR.PRPC  = 0b11111111;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPD  = 0b11101111;          // Stop: TWI,       , USART1, SPI, HIRES, TC1, TC0
    PR.PRPE  = 0b11111111;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPF  = 0b11111110;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    CPU_Slow();
    DMA.CTRL          = 0x80;       // Enable DMA
}

// From Application Note AVR1003
void CCPWrite( volatile uint8_t * address, uint8_t value ) {
    uint8_t volatile saved_sreg = SREG;
    cli();

#ifdef __ICCAVR__
	asm("movw r30, r16");
#ifdef RAMPZ
	RAMPZ = 0;
#endif
	asm("ldi  r16,  0xD8 \n"
	    "out  0x34, r16  \n"
#if (__MEMORY_MODEL__ == 1)
	    "st     Z,  r17  \n");
#elif (__MEMORY_MODEL__ == 2)
	    "st     Z,  r18  \n");
#else /* (__MEMORY_MODEL__ == 3) || (__MEMORY_MODEL__ == 5) */
	    "st     Z,  r19  \n");
#endif /* __MEMORY_MODEL__ */

#elif defined __GNUC__
	volatile uint8_t * tmpAddr = address;
#ifdef RAMPZ
	RAMPZ = 0;
#endif
	asm volatile(
		"movw r30,  %0"	      "\n\t"
		"ldi  r16,  %2"	      "\n\t"
		"out   %3, r16"	      "\n\t"
		"st     Z,  %1"       "\n\t"
		:
		: "r" (tmpAddr), "r" (value), "M" (CCP_IOREG_gc), "i" (&CCP)
		: "r16", "r30", "r31"
		);

#endif
	SREG = saved_sreg;
}

// Read out calibration byte.
uint8_t SP_ReadCalibrationByte(uint8_t location) {
	uint8_t result;
	/* Load the NVM Command register to read the calibration row. */
	NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
 	result = pgm_read_byte(location);
	/* Clean up NVM Command register. */
 	NVM_CMD = NVM_CMD_NO_OPERATION_gc;
	return result;
}

// Calibrate offset, inputs must be connected to ground
void CalibrateOffset(void) {
    int8_t  *q1, *q2, avrg8;  // temp pointers to signed 8 bits
    uint8_t i,j,s=0;
    int16_t avrg1, avrg2;
    Key=0;
    clr_display();
    lcd_putsp(PSTR("DISCONNECT CH1,CH2"));
    tiny_printp(116,15,PSTR("GO"));
    dma_display(); WaitDisplay();
    while(!Key);
    AnalogOn();
    if(testbit(Key,K3)) {
        clr_display();
	    for(Srate=0; Srate<8; Srate++) {	// Cycle thru first 8 srates
            i=6; do {
                s++;
                M.CH1gain=i;
                M.CH2gain=i;
                SimpleADC();
                q1=Temp.IN.CH1;
                q2=Temp.IN.CH2;
                // Calculate offset for CH1
                avrg1=0;
                avrg2=0;
                j=0; do {
            	    avrg1+= (*q1++);
            	    avrg2+= (*q2++);
                } while(++j);
                avrg8=avrg1>>8;
                ONGRN();
                eeprom_write_byte((uint8_t *)&offset8CH1[Srate][i], avrg8);
                j = 32+avrg8; // add 32 to center on screen
                if(j<64) lcd_line(s,32,s,j);
                else ONRED();
                avrg8=avrg2>>8;
                eeprom_write_byte((uint8_t *)&offset8CH2[Srate][i], avrg8);
                j = 32+avrg8; // add 32 to center on screen
                if(j<64) lcd_line(s+64,32,s+64,j);
                else ONRED();
                dma_display(); WaitDisplay();
            } while(i--);
        }
        // Calculate offset for Meter in VDC
        avrg1=0;
        avrg2=0;
        ADCA.CTRLB = 0x10;          // signed mode, no free run, 12 bit right adjusted
        ADCA.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        ADCB.CTRLB = 0x10;          // signed mode, no free run, 12 bit right adjusted
        ADCB.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        i=0;
        do {
            ADCA.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            ADCB.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            delay_ms(1);
            avrg1+= (int16_t)ADCA.CH0.RES;  // Measuring 0V, should not overflow 16 bits
            avrg2+= (int16_t)ADCB.CH0.RES;  // Measuring 0V, should not overflow 16 bits
        } while(++i);
        eeprom_write_word((uint16_t *)&offset16CH1, avrg1/*+0x08*/);
        eeprom_write_word((uint16_t *)&offset16CH2, avrg2/*+0x08*/);
        eeprom_write_byte(&EECalibrated, 0);    // Calibration complete!
    }
    Key=0;
    AnalogOff();
}

// Calibrate gain, inputs must be connected to 4.000V
void CalibrateGain(void) {
    uint8_t i,j,s=0;
    static int32_t avrg1, avrg2;
    int16_t offset;
    #ifndef NODISPLAY
        Key=0;
        clr_display();
        lcd_putsp(PSTR("NOW CONNECT 4.000V"));
        tiny_printp(116,7,PSTR("GO"));
        dma_display();
        while(!Key);
    #else
        setbit(Key,K3);
    #endif
    if(testbit(Key,K3)) {
        clr_display();
        // Calculate offset for Meter in VDC
        avrg1=0;
        avrg2=0;
        ADCA.CTRLB = 0x90;          // signed mode, no free run, 12 bit right adjusted
        ADCA.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        ADCB.CTRLB = 0x90;          // signed mode, no free run, 12 bit right adjusted
        ADCB.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        i=0;
        do {
            ADCA.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            ADCB.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            delay_ms(1);
            avrg1-= (int16_t)ADCA.CH0.RES;
            avrg2-= (int16_t)ADCB.CH0.RES;
        } while(++i);
        // Vcal = 4V
        // Amp gain = 0.18
        // ADC Reference = 1V
        // 12 bit signed ADC -> Max = 2047
        // ADC cal = 4*.18*2047*256 = 377303
        // ADCcal = ADCmeas * (2048+cal)/2048
		offset=(int16_t)eeprom_read_word((uint16_t *)&offset16CH1);      // CH1 Offset Calibration
		avrg1+=offset;
        avrg1 = (377303*2048l-avrg1*2048)/avrg1;
        eeprom_write_byte((uint8_t *)&gain8CH1, avrg1);
		offset=(int16_t)eeprom_read_word((uint16_t *)&offset16CH2);      // CH2 Offset Calibration
		avrg2+=offset;
        eeprom_write_byte((uint8_t *)&gain8CH2, avrg2);
    }
    Key=0;
}

// Fill up channel data buffers
void SimpleADC(void) {
	Apply();
	_delay_ms(64);
    StartDMAs();
	_delay_ms(16);
    ADCA.CTRLB = 0x14;          // Stop free run of ADC (signed mode, no free run, 8 bit)
    ADCB.CTRLB = 0x14;          // Stop free run of ADC (signed mode, no free run, 8 bit)
    // Disable DMAs
    clrbit(DMA.CH0.CTRLA, 7);
    clrbit(DMA.CH2.CTRLA, 7);
    clrbit(DMA.CH1.CTRLA, 7);
}

// Receives a nibble, returns the corresponding ascii that represents the HEX value
char NibbleToChar(uint8_t nibble) {
    nibble=nibble+'0';          // '0' thru '9'
    if(nibble>'9') nibble+=7;   // 'A' thru 'F'
    return nibble;
}

// Prints a HEX number
void printhex(uint8_t n) {
    uint8_t temp;
    temp = n>>4;   GLCD_Putchar(NibbleToChar(temp));
    temp = n&0x0F; GLCD_Putchar(NibbleToChar(temp));
}

// Converts an uint to int, then returns the half
uint8_t half(uint8_t number) {
    int8_t temp;
    temp=(int8_t)(number-128);
    temp=temp/2;
    return (uint8_t)(temp+128);
}

// Converts an uint to int, then return the double, or saturates
uint8_t twice(uint8_t number) {
    int8_t temp;
    if(number<=64) return 0;
    if(number>=192) return 255;
    temp = (int8_t)(number-128);
    temp = temp*2;
    return (uint8_t)(temp+128);
}

/*
// Calibrate DAC gain and offset, connect AWG to CH1
// Adjust with rotary encoders
static void CalibrateDAC(void) {
    uint8_t i, step=0, data, average;
    uint8_t test, bestoffset, bestgain, bestmeasure1;
    uint16_t sum, bestmeasure2;
    clr_display();

    ADCA.CH0.CTRL = 0x03 | (6<<2);       // Set gain 6
    CH1.offset=(signed char)eeprom_read_byte(&offsetsCH1[6]);

    AWGAmp=127;         // Amplitude range: [0,127]
    AWGtype=1;          // Waveform type
    AWGduty=256;        // Duty cycle range: [0,512]
    AWGOffset=0;        // 0V offset
    desiredF = 100000;  // 1kHz
    BuildWave();
    while(step<7) {
        while(!testbit(TCD0.INTFLAGS, TC1_OVFIF_bp));   // wait for refresh timeout
        setbit(TCD0.INTFLAGS, TC1_OVFIF_bp);
        // Acquire data

        // Display waveform
        i=0; sum=0;
        do {
            data=addwsat(CH1.data[i],CH1.offset);
            sum+=data;
            set_pixel(i>>1, data>>2);    // CH1
        } while(++i);
        average=(uint8_t)(sum>>8);

        switch(step) {
            case 0: // Connect AWG to CH1
                tiny_printp(0,0,PSTR("AWG Calibration Connect AWG CH1 Press 5 to start"));
                step++;
            break;
            case 1:
                if(key) {
                    if(key==KC) step++;
                    else step=7;         // Did not press 5 -> exit
                }
            break;
            case 2: // Output 0V from AWG
                AWGAmp=1;         // Amplitude range: [0,127]
                AWGtype=1;        // Waveform type
                BuildWave();
                tiny_printp(0,3,PSTR("Adjusting offset"));
                // ADS931 power, output enable, CH gains
//                PORTE.OUT = 0;
                CH1.offset=(signed char)eeprom_read_byte(&offsetsCH1[0]);
                step++;
                bestoffset = 0;
                test = 0;
                bestmeasure1=0;
                DACB.OFFSETCAL = 0;
            break;
            case 3: // Adjust Offset
                if(abs((int16_t)average-128)<abs((int16_t)bestmeasure1-128)) {    // Current value is better
                    bestoffset = test;
                    bestmeasure1=average;
                    lcd_goto(0,4);
                    if(bestoffset>=0x40) printN(0x40-bestoffset);
                    else printN(bestoffset);
                }
                lcd_line(0,bestmeasure1>>1,127,bestmeasure1>>1);
                test++;
                DACB.OFFSETCAL = test;
                if(test>=128) {
                    step++;
                    DACB.OFFSETCAL = bestoffset;   // Load DACA offset calibration
                }
            break;
            case 4: // Output -1.75V from AWG
                AWGAmp=0;           // Full Amplitude
                AWGtype=1;          // Waveform type
                AWGOffset=112;      // Offset = -1.75
                BuildWave();
                tiny_printp(0,5,PSTR("Adjusting gain"));
//                PORTE.OUT = 4;  // 0.5V / div
                CH1.offset=(signed char)eeprom_read_byte(&offsetsCH1[4]);
                step++;
                bestgain = 0;
                test=0;
                bestmeasure2=0;
                DACB.GAINCAL = 0;
            break;
            case 5: // Adjust gain
                // (1.75/0.5)*32+128)*256 = 61440
                if(abs((int32_t)sum-61696)<abs((int32_t)bestmeasure2-61696)) {    // Current value is better
                    bestgain = test;
                    bestmeasure2=sum;
                    lcd_goto(0,6);
                    if(bestgain>=0x40) printN(0x40-bestgain);
                    else printN(bestgain);
                }
                test++;
                DACB.GAINCAL = test;
                if(test>=128) {
                    step++;
                    DACB.GAINCAL = bestgain;
                }
            break;
            case 6: // Calibration complete
                // Save calibration results
                AWGAmp=0;
                eeprom_write_byte(&EEDACoffset, bestoffset);    // Save offset calibration
                eeprom_write_byte(&EEDACgain, bestgain);        // Save gain calibration
                tiny_printp(0,15,PSTR("Cal complete"));
                step++;
            break;
        }
    }
    // Restore Waveform
    LoadAWGvars();              // Load AWG settings
    BuildWave();                // Construct AWG waveform
}*/

extern const NVMVAR FLM;
static inline void Restore(void) {
    memcpy_P(0,  &FLGPIO, 12);
    memcpy_P(&M, &FLM, sizeof(NVMVAR));
    ONGRN();
    SaveEE();
    OFFGRN();
    Key=0;
}

extern NVMVAR EEMEM EEM;

// Save settings to EEPROM
void SaveEE(void) {
    if(!testbit(Misc,lowbatt)) {
	    if(MFFT<0x20) { // Don't save Meter mode settings
            Srate=old_s;
		    M.CH1gain=old_g1;
		    M.CH2gain=old_g2;
	    }
        eeprom_write_block(0, &EEGPIO, 12);
        eeprom_write_block(&M, &EEM, sizeof(NVMVAR));
        old_s=Srate; old_g1=M.CH1gain; old_g2=M.CH2gain;
    }
}

// Load settings from EEPROM
static inline void LoadEE(void) {
    eeprom_read_block(0, &EEGPIO, 12);
    eeprom_read_block(&M, &EEM, sizeof(NVMVAR));
}

void PowerDown(void) {
	USB.CTRLB = 0;          // USB Disattach
	USB.ADDR = 0;
	USB.CTRLA = 0;
    OFFGRN();               // In case the LED was on
    SaveEE();               // Save MSO settings
    GLCD_LcdOff();
    while(Key);
    PORTE.OUTCLR = 0x01;    // Power up clear
    SLEEP.CTRL = SLEEP_SMODE_PDOWN_gc | SLEEP_SEN_bm;
    asm("sleep");
    SLEEP.CTRL = 0x00;
    GLCD_LcdInit();
    PORTE.OUTSET = 0x01;    // Power up
	Key=0;
	USB_ResetInterface();
}

// Wake up from sleep
ISR(TCD2_HUNF_vect) {
    SLEEP.CTRL = 0x00;
}

// Delay in mili seconds, take into account current CPU speed
void delay_ms(uint16_t n) {
    while(n--) {
        if(CLK.CTRL==0) {   // Running at 2MHz
            _delay_us(62);
        }
        else {              // Running at 32MHz
            _delay_us(999);
        }
    }
}
