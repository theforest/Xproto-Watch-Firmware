
#include <stdint.h>

typedef struct {
    uint8_t sec;                 // Seconds.        [0-59]
    uint8_t min;                 // Minutes.        [0-59]
    uint8_t hour;                // Hours.          [0-11]
    uint8_t mday;                // Day.            [0-30]
    uint8_t mon;                 // Month.          [0-11]
    uint8_t year;                // Year since 2000
    uint8_t wday;                // Day of week.    [0-6]
} time_var;

#define LEAP_YEAR(year) ((year%4)==0)   // Valid for the range the watch works on

extern time_var now;

void gettime(time_var *timep);
void settime(time_var *timep);
void WATCH(void);
void CALENDAR(void);