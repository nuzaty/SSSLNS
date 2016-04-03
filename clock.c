
#include "all.h"

void Clock_Init(void) {
    // 80 MHz Oscillator Configure
    PLLFBD = 0x0026;    // N1=2, N2=2
    CLKDIV  = 0x0000;   // M = 40        
}
