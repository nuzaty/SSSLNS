
#ifndef SDCARD_H
#define SDCARD_H

extern unsigned char SDError; 

void SPI2_Init();
unsigned char SPI2_Write(unsigned char data);

unsigned char SD_Init();
unsigned char SD_ReadSector(unsigned long addr, unsigned char* buffer);
unsigned char SD_WriteSector(unsigned long addr, unsigned char* data);

extern char filename[11];
extern File f;
extern File *fp;
extern unsigned char sdCardModuleStart;

void SDC_Start(void);
void SDC_Stop(void);
void SDC_Update(void);

#endif