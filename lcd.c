
#include "all.h"

#define LCDTRIS TRISD

#define RS LATDbits.LATD4
#define RW LATDbits.LATD5
#define EN LATDbits.LATD6

#define LCDDATA 1 	// RS = 1 ; access data register
#define LCDCMD 0 	// RS = 0 ; access command register

#define LCDWRITE 0 
#define LCDREAD 1

char lcdChar[21];

void LCD_Nibble(unsigned char nibble);

void LCD_Init(void) {
    LCDTRIS = 0;
    
    delay_ms(120);
    LCD_Nibble(0x3);
    delay_ms(5);
    LCD_Nibble(0x3);
    delay_ms(1);
    LCD_Nibble(0x3);
    delay_ms(1);
    LCD_Nibble(0x2);
    delay_ms(1);
    LCD_Nibble(0x2);
    LCD_Nibble(0x8);
    delay_ms(1);
    LCD_Nibble(0x0);
    LCD_Nibble(0x8);
    delay_ms(1);
    LCD_Nibble(0x0);
    LCD_Nibble(0x1);
    delay_ms(5);
    LCD_Nibble(0x0);
    LCD_Nibble(0x6);
    delay_ms(1);    
    LCD_Nibble(0x0);
    LCD_Nibble(0xC);
    delay_ms(1); 
}

void LCD_Nibble(unsigned char nibble) {
    RS = LCDCMD;
    RW = LCDWRITE;
    
    EN = 1;
	delay_ms(2);       
    LATD = (PORTD & 0xF0) | nibble; 
	delay_ms(1);
    EN = 0;
}

// Send command
void LCD_Command(unsigned char com)
{    
    RS = LCDCMD;
    RW = LCDWRITE;
    
    EN = 1;
	delay_ms(2);       
    LATD = (PORTD & 0xF0) | com >> 4; 
	delay_ms(1);
    EN = 0;
    
    EN = 1;
	delay_ms(2);       
    LATD = (PORTD & 0xF0) | (com & 0x0F); 
	delay_ms(1);
    EN = 0;
}

// Send data
void LCD_Data(unsigned char data)
{
	RS = LCDDATA;
	RW  = LCDWRITE;
   
    EN = 1;
	delay_ms(2);       
    LATD = (PORTD & 0xF0) | data >> 4; 
	delay_ms(1);
    EN = 0;
    
    EN = 1;
	delay_ms(2);       
    LATD = (PORTD & 0xF0) | (data & 0x0F); 
	delay_ms(1);
    EN = 0;
}

// put String to LCD Display
void LCD_Puts(char *p)
{	
	while(*p)
	{
		LCD_Data(*p);
		p++;
	}
}

// set cursor to home
void LCD_Home(void) 
{
	LCD_Command(0x02); // return cursor to home (0,0)
	delay_ms(1);
}

// clear display
void LCD_Clear(void)
{
	LCD_Command(0x01); // clear display, return cursor to home
	delay_ms(1);
}

// set cursor position
void LCD_Row(int row) 
{
    char addr;
    switch(row)
    {
     case 1: addr = 0x00; break; //Starting address of 1st line
     case 2: addr = 0x40; break; //Starting address of 2nd line
     case 3: addr = 0x14; break; //Starting address of 3rd line
     case 4: addr = 0x54; break; //Starting address of 4th line
     default: ; 
    }
    LCD_Command(addr | 0x80);
    delay_ms(1);
}