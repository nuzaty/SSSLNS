
/** 
 * Author: Surasek Nusati (S-Kyousuke)
 * 
 */

#include "all.h"
#include "config.h"

#define MAX_PAGE 4

#define TRIS_OUTPUT 0
#define TRIS_INPUT 1

#define SW_TRIS _TRISA2

#define RELAY_SOURCE _LATA9
#define RELAY_SOURCE_TRIS _TRISA9
#define SOURCE_GRID 0
#define SOURCE_BATTERY 1

#define RELAY_LED _LATA10
#define RELAY_LED_TRIS _TRISA10
#define TURN_OFF_LED 0
#define TURN_ON_LED 1

#define LDR_ACTIVE_VOLTAGE 1.60  //Get from experiment only

unsigned char page = 0;

void SUP_Update() {
    if (ADC_GetPercentBattery() > 30.0) {
        RELAY_SOURCE = SOURCE_BATTERY;
    }
    else if (ADC_GetPercentBattery() < 20.0) {
        RELAY_SOURCE = SOURCE_GRID;
    }
}

void LED_Update() {  
    if (RTCC_GetTime().hr > 0x18 || RTCC_GetTime().hr < 0x06) {        
        RELAY_LED = TURN_ON_LED;
    }
    else {
         //RELAY_LED = (ADC_GetLdrVolt() <= LDR_ACTIVE_VOLTAGE)? TURN_OFF_LED : TURN_ON_LED; 
    }
}

// interrupt 4 service 
void __attribute__ ((interrupt, no_auto_psv)) _INT3Interrupt(void) { 
    LCD_Clear();
    ++page;
    if(page == MAX_PAGE)
        page = 0;
    _INT3IF = 0;
}

void init() {
    // Clock Initialize
    Clock_Init();    
    Timer_Init();
    _NSTDIS = 1;  //Interrupt Nesting Disable    
            
    // Digital Communication Initialize
    //UART1_Init(); 
    ADC_Init();
    I2C1_Init();    
            
    // Peripherals Initialize
    LCD_Init();
     
    // Interrupt 3 Initialize    
    SW_TRIS = TRIS_INPUT; // RA2/INT3 as input;
    
    _INT3IP = 1;
    _INT3IF = 0;
    _INT3IE = 1;
    _INT3EP = 1; //Interrupt on negative edge
    
    // Make relay drive port as output
    RELAY_SOURCE_TRIS = TRIS_OUTPUT; 
    RELAY_LED_TRIS = TRIS_OUTPUT;
    
    // Select battery as voltage source and turn on LED light
    RELAY_SOURCE = SOURCE_BATTERY; 
    RELAY_LED = TURN_OFF_LED;
    
    SDC_Start();
}

void LCD_Update() {
    _INT3IP = 0;    // Disable INT3 for temporary    
    AdcData data = ADC_GetData();
    LCD_Home();
    switch(page) {
        case 1:
            LCD_Puts("Load Data           ");
            LCD_Row(2);
            if (data.vLoadRms != 0 && data.iLoadRms != 0) {
                sprintf(lcdChar, "V:%-7.1f  I:%-5.3f  ", data.vLoadRms, data.iLoadRms);  
                LCD_Puts(lcdChar);
                LCD_Row(3);
                if (data.pfLoad != 0) 
                    sprintf(lcdChar, "P:%-7.3f PF:%-6.2f ", data.pLoad, data.pfLoad);
                else 
                    sprintf(lcdChar, "P:%-7.3f PF:-      ", data.pLoad);               
                LCD_Puts(lcdChar);
            }
            else {                
                if (data.vLoadRms != 0 && data.iLoadRms == 0) {
                    sprintf(lcdChar, "V:%-7.1f  I:0     ", data.vLoadRms);  
                    LCD_Puts(lcdChar);                
                }
                else if (data.vLoadRms == 0 && data.iLoadRms != 0) {
                    sprintf(lcdChar, "V:0        I:%-5.3f  ", data.iLoadRms);  
                    LCD_Puts(lcdChar);
                }           
                else {
                    LCD_Puts("V:0        I:0     "); 
                }
                LCD_Row(3);
                LCD_Puts("P:0       PF:-     ");
            }
            LCD_Row(4);
            if (data.eLoad != 0) {
                sprintf(lcdChar, "E:%.4Lf kWh       ", data.eLoad);  
                LCD_Puts(lcdChar);
            }
            else LCD_Puts("E:0 kWh             ");            
            break;
            
        case 3:            
            LCD_Puts("PV Data             ");
            LCD_Row(2);
            
            if (data.vPV != 0 && data.iPV != 0) {
                sprintf(lcdChar, "V:%-6.2f I:%-6.2f  ", data.vPV, data.iPV);
                LCD_Puts(lcdChar);
            }
            else if (data.vPV != 0 && data.iPV == 0) {
                sprintf(lcdChar, "V:%-6.2f I:0       ", data.vPV);
                LCD_Puts(lcdChar);
            }
            else if (data.vPV == 0 && data.iPV != 0) {
                sprintf(lcdChar, "V:0      I:%-6.2f  ", data.iPV);
                LCD_Puts(lcdChar);
            }
            else LCD_Puts("V:0      I:0        ");            
            
            LCD_Row(3);
            if (data.pPv != 0) {
                sprintf(lcdChar, "P:%-7.2f           ", data.pPv);  
                LCD_Puts(lcdChar);
            }
            else LCD_Puts("P:0                 ");            
            
            LCD_Row(4);
            if (data.ePv != 0) {
                sprintf(lcdChar, "E:%.4Lf kWh       ", data.ePv);  
                LCD_Puts(lcdChar);
            }
            else LCD_Puts("E:0 kWh             ");
            break;
            
        case 2:                        
            LCD_Puts("Battery Data        ");
            LCD_Row(2);            
            
            if (data.vBatt != 0 && data.iBatt != 0) {
                sprintf(lcdChar, "V:%-6.2f I:%-6.2f  ", data.vBatt, data.iBatt);
                LCD_Puts(lcdChar);
            }
            else if (data.vBatt != 0 && data.iBatt == 0) {
                sprintf(lcdChar, "V:%-6.2f I:0       ", data.vBatt);
                LCD_Puts(lcdChar);
            }
            else if (data.vBatt == 0 && data.iBatt != 0) {
                sprintf(lcdChar, "V:0      I:%-6.2f  ", data.iBatt);
                LCD_Puts(lcdChar);
            }
            else LCD_Puts("V:0      I:0        ");
            
            LCD_Row(3);
            if (data.pBatt != 0) {
                if (data.percentBatt != 0) {
                    sprintf(lcdChar, "P:%-6.2f %%Batt:%-5.2f", data.pBatt, data.percentBatt);  
                    LCD_Puts(lcdChar);
                }
                else {
                    sprintf(lcdChar, "P:%-6.2f %%Batt:0    ", data.pBatt);  
                    LCD_Puts(lcdChar);
                }
                
            }
            else { 
                if (data.percentBatt != 0) {
                    sprintf(lcdChar, "P:0      %%Batt:%-5.2f", data.percentBatt); 
                    LCD_Puts(
                            lcdChar);
                }
                LCD_Puts("P:0      %Batt:0    ");                
            }
            
            LCD_Row(4);
            if (data.eBatt != 0) {
                sprintf(lcdChar, "E:%.4Lf kWh       ", data.eBatt);  
                LCD_Puts(lcdChar);
            }
            else LCD_Puts("E:0 kWh             ");
            break;
            
        case 0:
            LCD_Puts(RTCC_GetLcdTime());
            LCD_Row(2);
            LCD_Puts(RTCC_GetLcdDate());    
            LCD_Row(3);
            sprintf(lcdChar, "%%Eff: %-6.2f        ", data.efficiency);
            LCD_Puts(lcdChar);
            LCD_Row(4);
            LCD_Puts(sdCardText);
            break;

        default:
            break;
    }
    _INT3IP = 1; // Re-enable INT3
}

int main(void) {
    init();
    
    while(1) {        
        LCD_Update();
        SUP_Update();
        LED_Update();             
    }             
    return 0;
}
