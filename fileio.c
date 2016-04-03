
#include "all.h"

unsigned char FIOError = 0x00;

typedef struct {
    unsigned long fatAddress;
    unsigned long rootAddress;
    unsigned long dataAddress;
    unsigned short maxRoot;
    unsigned short maxCluster;
    unsigned short fatSize; 
    unsigned char fatCopy;
    unsigned char sectorPerCluster;
} Media;

Media media;

#define FIOE_INVALID_MBR 0x02
#define FIOE_PARTITION_TYPE 0x03
#define FIOE_INVALID_BR 0x04
#define FIOE_READ_DIR_FRIST 0x05
#define FIOE_READ_DIR 0x06
#define FIOE_MEDIA_NOT_INIT 0x07
#define FIOE_FIND_ERROR 0x08
#define FIOE_INVALID_MODE 0x09
#define FIOE_FILE_NOT_FOUND 0x0A
#define FIOE_READ_DATA_ERROR 0x0B
#define FIOE_INVALID_FILE 0x0C
#define FIOE_EOF 0x0D
#define FIOE_INVALID_CLUSTER 0x0E
#define FIOE_MEDIA_FULL 0x0F
#define FIOE_IDE_ERROR 0x10
#define FIOE_DIR_FULL 0x11
#define FIOE_FILE_OVERWRITE 0x12

// Custom return flag
#define FIO_NOT_FOUND 0x21
#define FIO_FOUND 0x20

// Master Boot Record key fields offsets
#define FO_MBR          0L   // master boot record sector LBA
#define FO_FIRST_P    0x1BE  // offset of first partition table
#define FO_FIRST_TYPE 0x1C2  // offset of first partition type
#define FO_FIRST_SECT 0x1C6  // first sector of first partition offset
#define FO_FIRST_SIZE 0x1CA  // number of sectors in partition
#define FO_SIGN       0x1FE  // MBR signature location (55,AA)

// Partition Boot Record key fields offsets
#define BR_SXC      0xd      // (byte) number of secotrs per cluster 
#define BR_RES      0xe      // (word) number of reserved sectors for the boot record
#define BR_FAT_SIZE 0x16     // (word) FAT size in number of sectors 
#define BR_FAT_CPY  0x10     // (byte) number of FAT copies
#define BR_MAX_ROOT 0x11     // (odd word) max number of entries in root dir

#define ENTRY_SIZE   32      // size of a directory entry (bytes)

#define ENTRY_NAME   0x00    // offset of file name (1*8 bytes)
#define ENTRY_EXT    0x08    // offset of file extension (1*3 bytes)
#define ENTRY_ATTRIB 0x0B    // offset of attribute (byte)
#define ENTRY_TIME   0x16    // offset of last use time  (word)
#define ENTRY_DATE   0x18    // offset of last use date  (word)
#define ENTRY_CLST   0x1A    // offset of first cluster in FAT (word)
#define ENTRY_FSIZE  0x1C    // offset of file size (dword)

#define ENTRY_DEL    0xE5    // marker of a deleted entry
#define ENTRY_EMPTY  0x00    // marker of last entry in directory

// file attributes mask    
#define ATT_RO      0x01    // attribute read only
#define ATT_HIDE    0x02    // attribute hidden
#define ATT_SYS     0x04    //  "  system file
#define ATT_VOL     0x08    //  "  volume label
#define ATT_DIR     0x10    //  "  sub-directory
#define ATT_ARC     0x20    //  " (to) archive 
#define ATT_LFN     0x0f    // mask for Long File Name records

#define FAT_EOF    0xFFFF // last cluster in a file
#define FAT_MCLST  0xFFF8 // max cluster value in a fat structure

#define getTwoBytes(buffer, offset) *(unsigned short*)(buffer + offset)
#define getTwoBytesOdd(buffer, offset) (*(buffer + offset)+ (*(buffer + offset + 1) << 8))
#define getFourBytes(buffer, offset) *(unsigned long*)(buffer + offset)

unsigned char FileIO_Init(void) {    
    unsigned char buffer[512];   
        
    SD_ReadSector(FO_MBR, buffer);
    
    if (buffer[FO_SIGN] != 0x55 || buffer[FO_SIGN+1] != 0xAA) {
        FIOError = FIOE_INVALID_MBR;
        goto ErrorExit;    
    }
    
    if (buffer[FO_FIRST_TYPE] != 0x04
            && buffer[FO_FIRST_TYPE] != 0x06
            && buffer[FO_FIRST_TYPE] != 0x0E) {
        sprintf(uartTextBuffer, "0x%02X\r", buffer[FO_FIRST_TYPE]);
        UART1_Puts(uartTextBuffer);
        FIOError = FIOE_PARTITION_TYPE;
        goto ErrorExit;     
    }    
    
    unsigned long partitionSize = getFourBytes(buffer, FO_FIRST_SIZE); // size (total sector) of partition
    unsigned long firstSectorAddress = getFourBytes(buffer, FO_FIRST_SECT);
    
    SD_ReadSector(firstSectorAddress, buffer);
    
    if (buffer[FO_SIGN] != 0x55 || buffer[FO_SIGN+1] != 0xAA) {
        FIOError = FIOE_INVALID_BR;
        goto ErrorExit;    
    }    
    
    media.fatAddress = firstSectorAddress + getTwoBytesOdd(buffer, BR_RES);
    media.fatCopy = buffer[BR_FAT_CPY];
    media.fatSize = getTwoBytesOdd(buffer, BR_FAT_SIZE);    
    media.maxRoot = getTwoBytesOdd(buffer, BR_MAX_ROOT);    
    media.rootAddress = media.fatAddress + (media.fatCopy * media.fatSize);
    media.dataAddress = media.rootAddress + (media.maxRoot >> 4); // * 32 bytes entry size / 512 bytes
    media.sectorPerCluster = buffer[BR_SXC];
    media.maxCluster = (partitionSize - (media.dataAddress - firstSectorAddress))/media.sectorPerCluster; 
     
    return 0;
    
ErrorExit:
    return 1;
}

unsigned char FileIO_ReadDir(File *fp, unsigned int entry) {   
    // 1 sector can keep only 16 entries (>> 4 = /16)
    unsigned long addr = media.rootAddress + (entry >> 4);    
    return SD_ReadSector(addr, fp->buffer);
}

unsigned char FileIO_WriteDir(File *fp, unsigned int entry) {   
    // 1 sector can keep only 16 entries (>> 4 = /16)
    unsigned long addr = media.rootAddress + (entry >> 4);    
    return SD_WriteSector(addr, fp->buffer);
}

unsigned char FileIO_FindDir(File *fp) {
    
    unsigned int entry = 0;        
    
    // read 1st sector of root directory    
    if(FileIO_ReadDir(fp, entry)) {
        FIOError = FIOE_READ_DIR_FRIST;
        return 1;
    }    
         
    unsigned int entryOffset = 0;   
    unsigned char data;
    
    while(1) {
        // get entry offset in buffer
        entryOffset = (entry & 0xf) * ENTRY_SIZE;
        
        // get 1st byte of name data and check if entry is avaliable
        // if end of list (empty) return file not found
        // if deleted entry skip to next entry
        data = fp->buffer[entryOffset + ENTRY_NAME];        
        
        if(data == ENTRY_EMPTY)
            return FIO_NOT_FOUND;  
        
        if(data != ENTRY_DEL) {             
            // if entry avaliable, check if not volumn or dir
            // if entry is normal file, compare file name
            // if entry found, fill file structure and return file found           
            data = fp->buffer[entryOffset + ENTRY_ATTRIB];
            int i;
            if(!(data & (ATT_DIR | ATT_VOL))) {
                for(i = ENTRY_NAME; i < ENTRY_ATTRIB; ++i)
                    if (fp->buffer[entryOffset+i] != fp->name[i]) {                        
                        break;                                          
                    }                
                if (i == ENTRY_ATTRIB) { 
                    fp->entry = entry;  
                    fp->time = getTwoBytes(fp->buffer, entryOffset + ENTRY_TIME);
                    fp->date = getTwoBytes(fp->buffer, entryOffset + ENTRY_DATE);
                    fp->size = getFourBytes(fp->buffer, entryOffset + ENTRY_FSIZE);
                    fp->startCluster = getFourBytes(fp->buffer, entryOffset + ENTRY_CLST);                    
                    return FIO_FOUND;                    
                } 
            }     
        }        
        ++entry;                
        // if already read all 16 entry, get next sector
        // if no next entry, break loop and return file not found
        if ((entry & 0xf) == 0 ) {
            if(FileIO_ReadDir(fp, entry)) {
                FIOError = FIOE_READ_DIR;
                return 1;
            }        
        } 
        if (entry >= media.maxRoot)
            break;
    }
    return FIO_NOT_FOUND;    
}

void FileIO_FormatName(const char *filename, File *fp) {
    
    int i;
    char c;
    
    //Format File name to Uppercase and fill rest up to 8 with space    
    for( i=0; i<8; i++)
    {
        c = toupper( *filename++);
        if (( c == '.') || ( c == '\0'))
            break;
        else 
            fp->name[i] = c;
    }
    while ( i<8) fp->name[i++] = ' ';

    //Format Extension to Uppercase and fill rest up to 3 with space
    if ( c != '\0') 
    {    
        for( i=8; i<11; i++)
        {
            c = toupper( *filename++);
            if ( c == '.')
                c = toupper( *filename++);
            if ( c == '\0')
                break;
            else 
                fp->name[i] = c;
        }
        while (i<11) fp->name[i++] = ' ';
    }
}

unsigned char FileIO_ReadData(File *fp) {
    unsigned long addr = media.dataAddress 
        + (fp->currentCluster - 2) * media.sectorPerCluster 
        + fp->sectorInCluster;    
    return SD_ReadSector(addr, fp->buffer);    
}

unsigned char FileIO_WriteData(File *fp) {
    unsigned long addr = media.dataAddress 
        + (fp->currentCluster - 2) * media.sectorPerCluster 
        + fp->sectorInCluster;
    return SD_WriteSector(addr, fp->buffer);    
}


unsigned int FileIO_ReadFat(File *fp, unsigned int currentCluster)
{
    unsigned int cluster;
    unsigned long pageAddr;
        
    // get address of current cluster in fat    
    // load the fat sector containing the cluster
    pageAddr = media.fatAddress + (currentCluster >> 8);   // 256 clusters entry per fat sector  
    if (SD_ReadSector(pageAddr, fp->buffer))
        return 0xFFFF;  // failed

    // get the next cluster value
    cluster = getTwoBytesOdd(fp->buffer, ((currentCluster & 0xFF) << 1)); //  <<1 = *2  (cluster always start at even offset)  
    return cluster;     
}

unsigned int FileIO_WriteFat(File *fp, unsigned int cluster, unsigned int value)
{
    unsigned int offset;
    unsigned long addr;
    
    offset = cluster << 1; // *2
    addr = media.fatAddress + (offset >> 9);  // offset/512 (bytes to sector)
    if (SD_ReadSector(addr, fp->buffer))
        return 1;
    
    offset &= 0x1FE; // offset = 0-510 (cluster always start at even offset) 
    
    fp->buffer[offset] = value;
    fp->buffer[offset+1] = value >> 8;
    
    // Updaate to all fat
    int i;
    for (i=0; i < media.fatCopy; ++i) {
        if (SD_WriteSector(addr, fp->buffer))
           return 1;
        addr += media.fatSize;
    }
       
    return 0;

}

unsigned int FileIO_NewFat(File* fp) {
    unsigned int i;
    unsigned cluster = fp->currentCluster;   
    
    do {
        // check next cluster in FAT
        ++cluster;
        
        // check if reached last cluster in FAT, re-start from top
        if(cluster >= media.maxCluster)
            cluster = 0;
        
        // check if full circle done, media full
        if(cluster == fp->currentCluster) {
            FIOError = FIOE_MEDIA_FULL;
            return 1;
        }
        i = FileIO_ReadFat(fp, cluster);
    
    } while (i!=0); // Loop until find empty cluster
    
    // chain cluster
    // new crate cluster = 0xFFFF
    // if this file is not new file, old cluster value = new cluster        
    FileIO_WriteFat(fp, cluster, FAT_EOF);
    if (fp->currentCluster > 0) FileIO_WriteFat(fp, fp->currentCluster, cluster);
    
    fp->currentCluster = cluster;    
    return 0;
}

unsigned char FileIO_NextFat(File* fp, unsigned int numLink) {
    unsigned int cluster;
    
    do {
        cluster = FileIO_ReadFat(fp, fp->currentCluster);
        
        // check if valid cluster        
        if(cluster >= FAT_MCLST) {
            FIOError = FIOE_EOF;
            return 1;
        }        
        if(cluster >= media.maxCluster) {
            FIOError = FIOE_INVALID_CLUSTER;
            return 1;
        }
        FileIO_ReadFat(fp, cluster);
    }
    while (--numLink > 0);
    
    fp->currentCluster = cluster;    
    return 0;
}

unsigned char FileIO_NewDir(File* fp) {
    unsigned int entry;
    unsigned int entryOffset;
    unsigned char data;
    
    entry = 0;
    
    if(FileIO_ReadDir(fp, entry)) {
        return 1;            
    }
    
    while(1) {
        entryOffset = (entry &0x0f) * ENTRY_SIZE;
        data = fp->buffer[entryOffset + ENTRY_NAME];
        if ((data == ENTRY_EMPTY ||data == ENTRY_DEL)) {
            fp->entry = entry;
            return FIO_FOUND;        
        }
        
        entry++;        
        // get next sector if need        
        if((entry & 0x0f) == 0) {
            if(FileIO_ReadDir(fp, entry)) {
                return 1;            
            }        
        }        
        // if read all entry return not found
        if(entry > media.maxRoot) {
            return FIO_NOT_FOUND;        
        }        
    }
    return 1;
}

void FileIO_Open(File *fp, const char* filename, const char mode) {     

    unsigned char r;     
    
    FileIO_FormatName(filename, fp);    
    
    if((mode == 'r' )||(mode == 'w')){
        fp->mode = mode;    
    }
    else {
        FIOError = FIOE_INVALID_MODE;
        goto ErrorExit;   
    }
    
    r = FileIO_FindDir(fp);
    if (r == 1) {
        FIOError = FIOE_FIND_ERROR;    
        goto ErrorExit;
    }
    
    fp->seek = 0;
    fp->sectorInCluster = 0;
    fp->byteInSector = 0;
        
   // Read Mode
    if(fp->mode == 'r') {        
        
        if(r == FIO_NOT_FOUND) {
            FIOError = FIOE_FILE_NOT_FOUND;
            goto ErrorExit;            
        }                
        
        // Read data and calculate data bytes        
        fp->currentCluster = fp->startCluster;
        
        if(FileIO_ReadData(fp)) {
            FIOError = FIOE_READ_DATA_ERROR;
            goto ErrorExit;                
        }
                
        if(fp->size - fp->seek < 512)
            fp->dataBytes = fp->size - fp->seek;
        else
            fp->dataBytes = 512;
    }      
    
    //Write Mode    
    else {
        if(r == FIO_NOT_FOUND) {             
            // mark as new file
            fp->currentCluster = 0; 
            
            // allocate first custer
            if(FileIO_NewFat(fp))
                goto ErrorExit;            
            fp->startCluster = fp->currentCluster;
            
            r = FileIO_NewDir(fp);
            if(r == 1) {
                FIOError = FIOE_IDE_ERROR;
                goto ErrorExit;
            }            
            if(r == FIO_NOT_FOUND) {
                FIOError = FIOE_DIR_FULL;
                goto ErrorExit;            
            }
            else {                
                // filled new entry
                fp->size = 0;                
                unsigned int entryOffset = (fp->entry & 0x0f) * ENTRY_SIZE;
                
                // set file size = 0
                fp->buffer[entryOffset + ENTRY_FSIZE] = 0;
                fp->buffer[entryOffset + ENTRY_FSIZE + 1] = 0;
                fp->buffer[entryOffset + ENTRY_FSIZE + 2] = 0;
                fp->buffer[entryOffset + ENTRY_FSIZE + 3] = 0;       
                
                //set DUMMY date and time
                fp->date = 0x34FE; // July 30th, 2006
                fp->buffer[entryOffset + ENTRY_DATE] = fp->date;
                fp->buffer[entryOffset + ENTRY_DATE+1] = fp->date>>8;
                fp->buffer[entryOffset + ENTRY_TIME] = fp->time;
                fp->buffer[entryOffset + ENTRY_TIME+1] = fp->time>>8;

                // set first cluster
                fp->buffer[entryOffset+ ENTRY_CLST]  = fp->startCluster;
                fp->buffer[entryOffset + ENTRY_CLST+1]= (fp->startCluster>>8);

                //set name
                int i;
                for ( i = 0; i < ENTRY_ATTRIB; ++i)
                    fp->buffer[entryOffset + i] = fp->name[i];

                // set attribute
                fp->buffer[entryOffset + ENTRY_ATTRIB] = ATT_ARC;

                // update the directory sector;
                if (FileIO_WriteDir( fp, fp->entry))
                {
                    FIOError = FIOE_IDE_ERROR;
                    goto ErrorExit;
                }
                
                
            }
        }
        else 
        {
            FIOError = FIOE_FILE_OVERWRITE;
            goto ErrorExit;                            
        }
    }  
    
    fp->chk = ~( fp->entry + fp->name[0]);
    return;
    
ErrorExit:
    return;
}


unsigned int FileIO_Read(File* fp, void* buffer, unsigned int size) {
    
    unsigned int count = size;
    unsigned int len;
    
    // checksum fails or not open in read mode
    if (( fp->entry + fp->name[0] != ~fp->chk ) || ( fp->mode != 'r'))     {   
        FIOError = FIOE_INVALID_FILE; 
        return size-count;
    }    
    
    while(count > 0) {
        if(fp->seek == fp->size) {
            FIOError = FIOE_EOF;   
            break;
        }               
        // load a new sector if necessary
        if (fp->byteInSector == fp->dataBytes) 
        {   
            fp->byteInSector = 0;
            fp->sectorInCluster++;
            
            // get a new cluster if necessary
            if ( fp->sectorInCluster == media.sectorPerCluster)
            {
                fp->sectorInCluster = 0;
                if (FileIO_NextFat(fp, 1))
                {  
                    break;
                }
            }            
            // read data
            if (FileIO_ReadData(fp))
            {
                break; 
            }        
            // determine how much data is really inside the buffer
            if ( fp->size-fp->seek < 512)
                fp->dataBytes = fp->size - fp->seek;
            else
                fp->dataBytes  = 512;
        } 
        
        // copy as many bytes as possible in a single chunk
        // take as much as fits in the current sector
        if (fp->byteInSector + count < fp->dataBytes)
            len = count;                // fits all in current sector
        else 
            len = fp->dataBytes - fp->byteInSector;    // take a first chunk, there is more   
        
        memcpy(buffer, fp->buffer + fp->byteInSector, len);  
        
        count -= len;
        buffer += len;
        fp->byteInSector += len;
        fp->seek += len;            
    }    
    return size-count;
}
unsigned int FileIO_Write(File* fp, void* source, unsigned int count) {
    unsigned int len;
    unsigned int size = count;
    
    // checksum fails or not open in write mode
    if (( fp->entry + fp->name[0] != ~fp->chk ) || ( fp->mode != 'w'))     {   
        FIOError = FIOE_INVALID_FILE; 
        return size-count;
    } 
    
    while(count > 0) {
    
        //copy as many bytes at a time as possible
        if ( fp->byteInSector + count < 512)
            len = count;
        else
            len = 512 - fp->byteInSector ;

        memcpy( fp->buffer+ fp->byteInSector, source, len);
        
        // 2.2 update all pointers and counters
        fp->byteInSector +=len;       // advance buffer position
        fp->seek +=len;      // count the added bytes
        count -=len;         // update the counter
        source +=len;           // advance the source pointer        

        // update the file size too
        if (fp->seek > fp->size)
            fp->size = fp->seek;

        // if buffer full, write current buffer to current sector
        if (fp->byteInSector == 512)
        {
            // write buffer full of data
            if (FileIO_WriteData(fp))
                return 1;

            // advance to next sector in cluster
            fp->byteInSector = 0;
            fp->sectorInCluster++;

            // 2.4.3 get a new cluster if necessary
            if (fp->sectorInCluster == media.sectorPerCluster)
            {
                fp->sectorInCluster = 0;
                if (FileIO_NewFat(fp))
                    return 1;
            }
        } 
    }
    return size-count;            
}

unsigned char FileIO_Close(File* fp) {
    
    if ( fp->mode == 'w') {
        // 1.1 if the current buffer contains data, flush it
        if ( fp->byteInSector>0)
        {
            if (FileIO_WriteData( fp))
                return 1;
        }
        // 1.2      finally update the dir entry,
        // 1.2.1    retrive the dir sector
        if (FileIO_ReadDir(fp, fp->entry))
            return 1;

        // 1.2.2    determine position in DIR sector
        unsigned int entryOffset = (fp->entry & 0xf) * ENTRY_SIZE;  // 16 entry per sector
        
        // 1.2.3 update file size
        fp->buffer[entryOffset + ENTRY_FSIZE] = fp->size;
        fp->buffer[entryOffset + ENTRY_FSIZE +1]= fp->size>>8;
        fp->buffer[entryOffset + ENTRY_FSIZE +2]= fp->size>>16;
        fp->buffer[entryOffset + ENTRY_FSIZE +3]= fp->size>>24;

        // 1.2.4    update the directory sector;
        if (FileIO_WriteDir(fp, fp->entry))
            return 1;
    
    }    
    return 0;

}
unsigned char FileIO_UpdateSize(File* fp) {    
    FileIO_Close(fp);       
    fp->chk = ~( fp->entry + fp->name[0]);
    return 0;
}


void FileIO_Open2(File *fp, const char* filename) {  
    
    FileIO_FormatName(filename, fp);
    fp->mode = 'w';    
        
    unsigned char r;
    
    r = FileIO_FindDir(fp);
    if (r == 1) {
        FIOError = FIOE_FIND_ERROR;    
        goto ErrorExit;
    }    
        
    fp->seek = 0;
    fp->sectorInCluster = 0;
    fp->byteInSector = 0;
    
    if(r == FIO_NOT_FOUND) {             
        // mark as new file
        fp->currentCluster = 0; 
            
        // allocate first custer
        if(FileIO_NewFat(fp))
            goto ErrorExit;            
        fp->startCluster = fp->currentCluster;
            
        r = FileIO_NewDir(fp);
        if(r == 1) {
            FIOError = FIOE_IDE_ERROR;
            goto ErrorExit;
        }            
        if(r == FIO_NOT_FOUND) {
            FIOError = FIOE_DIR_FULL;
            goto ErrorExit;          
        }
        else {                
            // filled new entry
            fp->size = 0;                
            unsigned int entryOffset = (fp->entry & 0x0f) * ENTRY_SIZE;
                
            // set file size = 0
            fp->buffer[entryOffset + ENTRY_FSIZE] = 0;
            fp->buffer[entryOffset + ENTRY_FSIZE + 1] = 0;
            fp->buffer[entryOffset + ENTRY_FSIZE + 2] = 0;
            fp->buffer[entryOffset + ENTRY_FSIZE + 3] = 0;       
                
            //set DUMMY date and time
            fp->date = 0x34FE; // July 30th, 2006
            fp->buffer[entryOffset + ENTRY_DATE] = fp->date;
            fp->buffer[entryOffset + ENTRY_DATE+1] = fp->date>>8;
            fp->buffer[entryOffset + ENTRY_TIME] = fp->time;
            fp->buffer[entryOffset + ENTRY_TIME+1] = fp->time>>8;

            // set first cluster
            fp->buffer[entryOffset+ ENTRY_CLST]  = fp->startCluster;
            fp->buffer[entryOffset + ENTRY_CLST+1]= (fp->startCluster>>8);

            //set name
            int i;
            for ( i = 0; i < ENTRY_ATTRIB; ++i)
                fp->buffer[entryOffset + i] = fp->name[i];

            // set attribute
            fp->buffer[entryOffset + ENTRY_ATTRIB] = ATT_ARC;

            // update the directory sector;
             if (FileIO_WriteDir( fp, fp->entry))
             {
                FIOError = FIOE_IDE_ERROR;
                goto ErrorExit;
             }     
        }
    }
    else {
        fp->currentCluster = fp->startCluster;
            
        unsigned int len;            
        while(1) {               

            if(fp->seek == fp->size) 
                break;            
            
            if(fp->size - fp->seek < 512)
                fp->dataBytes = fp->size - fp->seek;
            else
                fp->dataBytes  = 512;
        
            if (fp->byteInSector + 512 < fp->dataBytes) 
                len = 512;
            else
                len = fp->dataBytes - fp->byteInSector;
        
            fp->byteInSector += len;
            fp->seek += len;
            
            // load a new sector if necessary
            if (fp->byteInSector == fp->dataBytes) 
            {   
                fp->byteInSector = 0;
                fp->sectorInCluster++;                    
                // get a new cluster if necessary
                if ( fp->sectorInCluster == media.sectorPerCluster)
                {
                    fp->sectorInCluster = 0;
                    if (FileIO_NextFat(fp, 1))
                        break;
                }
            }
        }     
    }
        
    fp->chk = ~( fp->entry + fp->name[0]);
    return;
    
ErrorExit:
    fp = NULL;
    return;
}

void FileIO_WriteFileData(File* fp) {
    
    // checksum fails or not open in write mode
    if (( fp->entry + fp->name[0] != ~fp->chk ) ||  ( fp->mode != 'w'))     {   
        FIOError = FIOE_INVALID_FILE;
    }
    
    unsigned int len;  
    
    if ( fp->byteInSector + 512 < 512)
        len = 512;
    else
        len = 512 - fp->byteInSector ;
    
    memcpy(fp->buffer + fp->byteInSector, fileData, len);    
        
    fp->byteInSector += len;
    fp->seek += len;
    
    if (fp->seek > fp->size)
        fp->size = fp->seek;
    
    if (fp->byteInSector == 512)
    {        
        if (FileIO_WriteData(fp))
                return;           
        
        fp->byteInSector = 0;
        fp->sectorInCluster++;
        
        if (fp->sectorInCluster == media.sectorPerCluster)
        {
            fp->sectorInCluster = 0;
            if (FileIO_NewFat(fp))
                return;
        }
    }    
}

void FileIO_AppendFileData(File *fp, char* filename) {
    FileIO_Open2(fp, filename);    
    FileIO_WriteFileData(fp);
    FileIO_Close(fp);
}

void FileIO_Append(File *fp) {
    FileIO_WriteFileData(fp);
    FileIO_UpdateSize(fp);
}