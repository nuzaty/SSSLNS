
#ifndef LCD_H
#define	LCD_H

extern char lcdChar[21];

void LCD_Init(void);
void LCD_Command(unsigned char com);
void LCD_Puts(char *p);
void LCD_Home(void);
void LCD_Clear(void);
void LCD_Row(int row);


#endif

