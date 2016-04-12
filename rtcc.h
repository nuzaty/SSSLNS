
#ifndef RTCC_H
#define RTCC_H

typedef struct Times {
    char sec;
    char min;
    char hr;
}Time;

typedef struct Dates {
    char date;
    char month;
    char year;    
}Date;

void I2C1_Init(void);
void RTCC_Update(void);

char* RTCC_GetLcdDate(void);
char* RTCC_GetLcdTime(void);
char* RTCC_GetStringData(void);

Time RTCC_GetTime(void);

void RTCC_WriteTime(char hr, char min, char sec);
void RTCC_WriteDate(char day, char date, char month, char year);

#endif
