
#include "all.h"

void Clock_Init(void) {
    // 80 MHz Oscillator Configure
    PLLFBD = 0x0026;    // M = 40
    CLKDIV  = 0x0000;   // N1=2, N2=2
    
    __builtin_write_OSCCONH(0x03);		// Initiate Clock Switch to Primary Oscillator with PLL (NOSC=0x03)										
    __builtin_write_OSCCONL(OSCCON || 0x01);		// Start clock switching
    while(OSCCONbits.COSC != 0x03);	// Wait for Clock switch to occur
    while(OSCCONbits.LOCK!=1);		// Wait for PLL to lock    
}
