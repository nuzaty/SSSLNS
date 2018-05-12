
#include "all.h"

unsigned char SDError = 0x00;

// Error code
#define SD_ER_NO_CARD 0x01
#define SD_ER_INIT 0x02
#define SD_ER_READ 0x03
#define SD_ER_WRITE 0x04
#define SD_ER_RESET 0x99

// SD card commands
#define RESET 0 // a.k.a. GO_IDLE (CMD0)
#define CMD16 16
#define INIT 1 // a.k.a SEND_OP_COND (CMD1)
#define READ_SINGLE 17 // read a block of data
#define WRITE_SINGLE 24 // write a block of data

#define DATA_START 0xFE 
#define DATA_ACCEPT 0x05

#define SCK _RG6
#define SDO _RG7
#define SDI _RG8
#define CS _LATD12
#define TCS _TRISD12

#define SPI2_Read() SPI2_Write(0xff);
#define SPI2_Clock() SPI2_Write(0xff);
#define SD_Assert() CS = 0;
#define SD_Deassert() CS = 1; SPI2_Clock()

void SPI2_Init() {       
    TCS = 0; //Chip select port as output
    CS = 1;  //Not Select
    
    SPI2CON1 = 0;    
    SPI2STAT = 0;  
    
    SPI2CON1bits.CKE = 1;
    SPI2CON1bits.CKP = 0;
    SPI2CON1bits.MSTEN = 1; //Master mode  
    SPI2CON1bits.PPRE = 0; // 1:64 prescaler
    SPI2CON1bits.SPRE = 7; // 1:1 sec prescaler
      
    SPI2STATbits.SPIEN = 1;   
}

void SPI2_SpeedUp() {    
    SPI2CON1 = 0;    
    SPI2STAT = 0;  
    
    SPI2CON1bits.CKE = 1;
    SPI2CON1bits.CKP = 0;
    SPI2CON1bits.MSTEN = 1; //Master mode  
    SPI2CON1bits.PPRE = 3; // 1:1 pri prescaler
    SPI2CON1bits.SPRE = 4; // 1:4 sec prescaler
      
    SPI2STATbits.SPIEN = 1;
}

unsigned char SPI2_Write(unsigned char data) {
    SPI2BUF = data;		// write the data out to the SPI peripheral
    while (!SPI2STATbits.SPIRBF);	// wait for the data to be sent out	
    return SPI2BUF;
}

unsigned char SD_Command(unsigned char cmd, unsigned long add) {
    SPI2_Write(cmd | 0x40);
    SPI2_Write(add >> 24);
    SPI2_Write(add >> 16);
    SPI2_Write(add >> 8);
    SPI2_Write(add);
    SPI2_Write(0x95);
        
    unsigned char  i = 9;
    unsigned char  r;
    do {
        r = SPI2_Read(); // check if ready
        if (r != 0xff) {
            break;           
        }
    } while (--i > 0);    
    return r;    
    
    /* return response
    FF - timeout, no answer
    00 - command accepted
    01 - command received, card in idle state (after RESET)
        other errors
    */
}

unsigned char SD_Init() {
    SPI2_Init();
    unsigned int i, r;
    SD_Deassert();
    // 1. while the card is not selected    
    // 2. send 80 clock cycles (10 idle bytes) to start up
    for ( i=0; i<10; ++i)
        SPI2_Clock();
    
    SD_Assert();
    r = SD_Command(RESET, 0);
    SD_Deassert();
    
    if (r != 1) {
        SDError = SD_ER_NO_CARD;
        return 1;    
    }
    
    for ( i=0; i<10000; ++i) {
        SD_Assert();
        r = SD_Command(INIT, 0);    
        SD_Deassert();
        if (!r) break;
    }    
    
    if (r != 0) {
        SDError = SD_ER_INIT;
        return 1;    
    }    
    
    SPI2_SpeedUp();
    return 0;
}

unsigned char SD_ReadSector(unsigned long addr, unsigned char* buffer) {
    unsigned char r;
    SD_Assert();
    r = SD_Command(READ_SINGLE, addr << 9); // sector to bytes(*512)
    
    if (r == 0) {        
        
        unsigned int timeOut = 25000;
        unsigned int i;
    
        do {
            r = SPI2_Read();
            if (r == DATA_START)
                break;        
        } while(--timeOut > 0);
        
        if (timeOut > 0) {
            for(i=0; i<512; i++)
                buffer[i] = SPI2_Read();  
            
            //CRC 16 bit (2 byte)
            SPI2_Clock();
            SPI2_Clock();  
            
            SD_Deassert();
            return 0;   
        }         
    }
    SD_Deassert();
    SDError = SD_ER_READ;
    return 1;
}

unsigned char SD_WriteSector(unsigned long addr, unsigned char* data) {
    unsigned char r = 0xff;
    unsigned long i;
    
    SD_Assert();
    r = SD_Command(WRITE_SINGLE, addr << 9);   // sector to bytes(*512)
    
    if (r == 0) {
        SPI2_Clock();
        
        SPI2_Write(DATA_START);
        
        for( i=0; i<512; i++)
           SPI2_Write(data[i]);
        
        //CRC 16 bit (2 byte)
        SPI2_Clock();
        SPI2_Clock();            
        
        r = SPI2_Read();
        if ( (r & 0xf) == DATA_ACCEPT)
        {
            for( i=0; i<250000L; i++)
            {
                r = SPI2_Read();
                if ( r != 0 )
                    break;
            }
            SD_Deassert();
            return 0;
        }
    }
    
    SD_Deassert();
    SDError = SD_ER_WRITE;
    return 1; 
}

char filename[11];
File f;
File *fp;
unsigned char sdCardModuleStart = 0;

void SDC_Start(void) {
    
    SDError = 0;
    FIOError = 0;    
    
    if(!SD_Init()) {
        strcpy(sdCardText, "Setting up SD Card..");
        if (!FileIO_Init()) {
            fp = &f;
            unsigned int i = 0;    
            do {
                FIOError = 0;
                sprintf(filename, "DATA%04dCSV", i);
                FileIO_Open(fp, filename, 'w');            
                ++i;
            } while (FIOError != 0);    
                        
            if (fp != NULL) {
                for(i = 0;i<512;++i) {
                    fileData[i] = 0;    
                }
                FIOError = 0;
                strcpy(fileData,
                        "DateTime, "
                        "VLOAD, ILOAD, PLOAD, ELOAD, PFLOAD, "
                        "VBATT, IBATT, PBATT, %BATT, EBATT, "
                        "VPV, IPV, PPV, EPV, "
                        "%EFF\r");
                FileIO_Append(fp);  
                sdCardModuleStart = 1;
                strcpy(sdCardText, "Recording Data...   ");
                return;
            }               
        }   
    }
    strcpy(sdCardText, "Insert SD Card      ");    
}

void SDC_Stop(void) {
    sdCardModuleStart = 0;
    if (fp != NULL) FileIO_Close(fp);
    strcpy(sdCardText, "Safe to Remove Card "); 
}

void SDC_Update(void) {
    // recording data
    if (sdCardModuleStart) {
        if (SDError == 0 && FIOError == 0) {
            sprintf(fileData,"%s,%s\r", RTCC_GetStringData(), ADC_GetStringData());
            FileIO_Append(fp);         
        }
        else {
            sdCardModuleStart = 0;  
        }                
    }
    else SDC_Start();
}