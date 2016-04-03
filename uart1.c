
#include "all.h"

void UART1_Init(void) {
	// Configure U1MODE    
    /* 
     * Continue in Idle
     * No IR translation 
     * Flow Control mode
     * TX,RX enabled, CTS,RTS not
     * No Wake up (since we don't sleep here)
     * No Loop Back
     * No Autobaud (would require sending '55')
     * IdleState = 1  (for dsPIC)
     * 16 clocks per bit period
     * 8bit, No Parity, One Stop Bit
     */
	U1MODE = 0;		
	U1BRG = UxBRG;	

	// Configure U1 Status
    /* Clear all status
     * The transmit buffer becomes empty after Interrupt
     * Interrupt on character recieved
     * Address Detect Disabled
     */
    U1STA = 0;
	U1STAbits.UTXISEL1 = 1;

    // And turn the peripheral on
	U1MODEbits.UARTEN = 1;
	U1STAbits.UTXEN = 1;    
}

void UART1_Puts(char *string) {
    putsUART1((unsigned int *)string);
    while(BusyUART1()); 
}
