
#ifndef FILEIO_H
#define FILEIO_H

typedef struct {
    unsigned char buffer[512];
    unsigned int time;
    unsigned int date; 
    unsigned long size;
    char name[11];
    unsigned int entry;
    unsigned int startCluster;
    unsigned int currentCluster;
    unsigned int sectorInCluster;
    unsigned int byteInSector;
    unsigned int dataBytes;
    unsigned long seek;
    char mode;
    char chk;
} File;


extern unsigned char FIOError;

unsigned char FileIO_Init(void);
unsigned char FileIO_FindDir(File *fp);
void FileIO_Open(File* fp, const char* filename, const char mode);
void FileIO_Open2(File *fp, const char* filename);
unsigned int FileIO_Read(File* fp, void* buffer, unsigned int size);
unsigned int FileIO_Write(File* fp, void* source, unsigned int count);
void FileIO_WriteSectorData(File* fp);
unsigned char FileIO_Close(File* fp);
unsigned int FileIO_ReadFat(File *fp, unsigned int currentCluster);
unsigned char FileIO_NextFat(File* fp, unsigned int numLink);
void FileIO_AppendFileData(File *fp, char* filename);
void FileIO_Append(File *fp);

#endif 
