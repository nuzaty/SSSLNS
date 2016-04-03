
#ifndef CLOCK_H
#define	CLOCK_H

#define FOSC 80000000L      // 80MHz
#define FCY (FOSC/2)
#define UxBRG (FCY/(57600L*16L))-1

void Clock_Init(void);

#endif	/* CLOCK_H */