
#include "all.h"

#define FSCL 400000L  		//400 kHz
#define I2CxBRG (FCY/FSCL - FCY/1111111L)    //Baud Rate Generator

static char lcdString[21];
static char stringData[32];
static Date date;
static Time time;

void I2C1_Init(void) {    
    
    unsigned int config;		// Keep configuration i2c module

	config = (I2C1_ON & 		// Enable i2c module            
			I2C1_IDLE_CON & 	// i2c module working on Idle mode
			I2C1_CLK_REL & 		// Release clock
			I2C1_IPMI_DIS & 	// Disable IPMI mode
			I2C1_7BIT_ADD& 		// Address mode 7 bit
			I2C1_SLW_DIS & 		// Disable slew rate
			I2C1_SM_DIS &		// Disable SMBus 		
			I2C1_GCALL_DIS & 	// Disable General call
			I2C1_STR_DIS &		// Disable clock stretch 
			I2C1_ACK & 			// Acknowledge data 
			I2C1_ACK_DIS & 		// Initial disable acknowledge
			I2C1_RCV_DIS &		// Initial disable receive
			I2C1_STOP_DIS &     // Initial disable stop condition
			I2C1_RESTART_DIS& 	// Initial disable restart condition
			I2C1_START_DIS &     // Initial disable start condition
            MI2C1_INT_ON );		// Enable interrupt module i2c in master mode
    
    OpenI2C1(config, I2CxBRG);    
    
}

void RTCC_Write(char addr, char value) {
    StartI2C1();
    while(I2C1CONbits.SEN);			// Wait until start OK
    MasterWriteI2C1(0xD0);		// Send ID into DS1621
    IdleI2C1();						// Wait for acknowledge		
    MasterWriteI2C1(addr);		
    IdleI2C1();						// Wait for acknowledge
    MasterWriteI2C1(value);
    IdleI2C1();						// Wait for acknowledge
    StopI2C1();						// Stop i2c condition
    while(I2C1CONbits.PEN);			// Wait until stop OK
}
 
unsigned char RTCC_Read(char addr) {
    StartI2C1();
    while(I2C1CONbits.SEN);			// Wait until start OK
    MasterWriteI2C1(0xD0);		// Send ID into DS1621
    IdleI2C1();						// Wait for acknowledge		
    MasterWriteI2C1(addr);		
    IdleI2C1();						// Wait for acknowledge
    RestartI2C1();
    while(I2C1CONbits.RSEN);
    MasterWriteI2C1(0xD1);
    IdleI2C1();
    unsigned char data = MasterReadI2C1();
    IdleI2C1();						// Wait for acknowledge
    StopI2C1();						// Stop i2c condition
    while(I2C1CONbits.PEN);			// Wait until stop OK
    return data;
}

void RTCC_Update(void) {
    time.sec = RTCC_Read(0x00);
    time.min = RTCC_Read(0x01);
    time.hr = RTCC_Read(0x02);
    
    date.date = RTCC_Read(0x04);
    date.month = RTCC_Read(0x05);
    date.year = RTCC_Read(0x06);
    
    char secOnes = time.sec & 0x0F;
    char secTens = (time.sec & 0xF0) >> 4;
    char minOnes = time.min & 0x0F;
    char minTens = (time.min & 0xF0) >> 4;
    char hrOnes = time.hr & 0x0F;
    char hrTens = (time.hr & 0xF0) >> 4;
    
    char dateOnes = date.date & 0x0F;
    char dateTens = (date.date & 0xF0) >> 4;
    char monthOnes = date.month & 0x0F;
    char monthTens = (date.month & 0xF0) >> 4;
    char yearOnes = date.year & 0x0F;
    char yearTens = (date.year & 0xF0) >> 4;
    
    sprintf(lcdString, "%d%d/%d%d/%d%d %d%d:%d%d:%d%d   ", 
            dateTens, dateOnes, 
            monthTens, monthOnes, 
            yearTens, yearOnes,
            hrTens, hrOnes, 
            minTens, minOnes,            
            secTens, secOnes);
        
    sprintf(stringData, "%d%d/%d%d/%d%d %d%d:%d%d:%d%d",
            dateTens, dateOnes, 
            monthTens, monthOnes, 
            yearTens, yearOnes,
            hrTens, hrOnes, 
            minTens, minOnes,
            secTens, secOnes);
}

char* RTCC_GetLcdString(void) {        
    return lcdString;
}

char* RTCC_GetStringData(void) {
    return stringData;
}

void RTCC_WriteTime(char hr, char min, char sec) {
    RTCC_Write(0x02, hr); 
    RTCC_Write(0x01, min);  
    RTCC_Write(0x00, sec);
}

void RTCC_WriteDate(char day, char date, char month, char year) {
    RTCC_Write(0x03, day);
    RTCC_Write(0x04, date);
    RTCC_Write(0x05, month);
    RTCC_Write(0x06, year);
}