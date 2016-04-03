
#include "all.h"

void Timer_Init(void) {
    
    //T2 Timer & Interrupt settings
    TMR2 = 0;
    PR2 = 31250-1; // Interrupt every 250 ms
    T2CON = 0x8020;    
    _T2IP = 7; // Highest Interrupt priority
    _T2IF = 0;
    _T2IE = 1;
    
}

void __attribute__ ((interrupt, no_auto_psv)) _T2Interrupt(void) {      
    RTCC_Update();
    _T2IF = 0;
}

void delay_ms(unsigned int ms) {    
    T1CON = 0x8010;    //turn on timer  200 ns/1 TMR
    int i;
    for(i = 0; i < ms; ++i) {
        TMR1 = 0;
        while(TMR1 < 5000); // 5000 x 200 ns =  1 ms;    
    }    
    T1CON = 0x0;    // timer off timer
}