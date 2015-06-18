/****************************************************************************

XMEGA Oscilloscope and Development Kit

Gabotronics
February 2012

Copyright 2012 Gabriel Anzziani

This program is distributed under the terms of the GNU General Public License

www.gabotronics.com
email me at: gabriel@gabotronics.com

*****************************************************************************/

#ifndef _DATA_H
#define _DATA_H

#include <stdint.h>
#include "awg.h"
#include "main.h"

extern const char VERSION[];
extern const uint32_t Powersof10[];
extern const char Minustext[];
extern const char multinv[];
extern const char one_over_delta_T[];
extern const char delta_V[];
extern const char unitkHz[];
extern const char count[];
extern const char Stop[];
extern const char unitmV[];
extern const char unitV[];
extern const char Vdiv[];
extern const char Sdiv[];
extern const char Fone[];
extern const char Ftwo[];
extern int16_t EEMEM offset16CH1;
extern int16_t EEMEM offset16CH2;
extern int8_t EEMEM gain8CH1;
extern int8_t EEMEM gain8CH2;
extern int8_t  EEMEM offset8CH1[8][7];
extern int8_t  EEMEM offset8CH2[8][7];
extern const uint8_t FLGPIO[12];
extern uint8_t EEMEM EEGPIO[12];
extern const int8_t Hamming[128];
extern const int8_t Hann[128];
extern const int8_t Blackman[128];
extern const int8_t Exp[128];
extern int8_t EEMEM EEwave[256];
extern const unsigned char BATTICON[];
extern const unsigned char PULSE[];
extern const unsigned char PULSEINV[];
extern const unsigned char LOGO[];

#endif
