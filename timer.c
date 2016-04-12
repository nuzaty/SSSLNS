
#include "all.h"

void Timer_Init(void) {
    
    //T2 Timer & Interrupt settings
    TMR2 = 0;
    PR2 = 40000-1; // Interrupt every 256 ms
    T2CON = 0x8030;
    _T2IP = 7; // Highest Interrupt priority
    _T2IF = 0;
    _T2IE = 1;
    
    //T8/T9 32 Timer settings    
    T8CON = 0;
    T9CON = 0;
    
    T8CONbits.T32 = 1;
    T8CONbits.TCS = 0;
    T8CONbits.TGATE = 0;
    T8CONbits.TCKPS = 0b11; // 1:256 prescaler
    T8CONbits.TON = 1;    
}

void __attribute__ ((interrupt, no_auto_psv)) _T2Interrupt(void) {      
    RTCC_Update();
    _T2IF = 0;
}

void __attribute__ ((interrupt, no_auto_psv)) _T3Interrupt(void) {      
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

void T89_Reset() {
    TMR8 = 0;
    TMR9 = 0;
}

double T89_GetSec() {
    unsigned long value = ((unsigned long)TMR9 << 16) + TMR8;
    double sec = value * 0.0000064;
    return sec;
}