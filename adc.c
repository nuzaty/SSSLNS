
#include "all.h"

//ANx Pin Configure
#define VPV 0
#define IPV 1
#define VBATT 2
#define IBATT 3
#define VLOAD 4
#define ILOAD 5
#define VLDR 6

#define VREF 3.253

// Parameter get from experiment

#define IPV_OFFSET 1.61
#define IPV_REVERSE_GAIN -10

#define IBATT_OFFSET 1.5966829475
#define IBATT_REVERSE_GAIN -9.97649657126

#define VPV_OFFSET 0
#define VPV_REVERSE_GAIN 17.1

#define VBATT_OFFSET 0
#define VBATT_REVERSE_GAIN 17

#define ILOAD_REVERSE_GAIN -3.35
#define VLOAD_REVERSE_GAIN -239

//ADC Configure

#define SAMPLE_COUNT 5714
#define LDR_SAMPLE_COUNT 96

#define MAX_CHNUM	 	7	// Highest Analog input number in Channel Scan
#define SAMP_BUFF_SIZE  8	// Size of the input buffer per analog input
#define NUM_CHS2SCAN    7	// Number of channels enabled for channel scan

// Number of locations for ADC buffer = 7 (AN0 to AN6) x 8 = 56 words
// Align the buffer to 64 words or 128 bytes. This is needed for peripheral indirect mode
static int BufferA[MAX_CHNUM+1][SAMP_BUFF_SIZE] __attribute__((space(dma),aligned(128)));
static int BufferB[MAX_CHNUM+1][SAMP_BUFF_SIZE] __attribute__((space(dma),aligned(128)));

static int result[MAX_CHNUM];

static long double sumSquareVLoad;
static long double sumSquareILoad;
static long double sumVILoad;
static long double sumVLoad;
static long double sumILoad;

static double vLoadOffset;
static double iLoadOffset;

static double sumVPV;
static double sumIPV;
static double sumVBatt;
static double sumIBatt;
static double sumVLdr;

static unsigned int sampleCount;
static unsigned int ldrSampleCount;

static AdcData data;
static char stringData[512];

static unsigned char dataComplete = 0;
static unsigned char offsetComplete = 0;

//Function Prototype

void DMA_Init(void);
double ADC_ToVolt(unsigned int adcValue);

// Volt Measurement

double ADC_PvVoltMeasurement(unsigned int adcValue) {
    return (ADC_ToVolt(adcValue) - VPV_OFFSET) * VPV_REVERSE_GAIN ;
}

double ADC_BatteryVoltMeasurement(unsigned int adcValue) {
    return (ADC_ToVolt(adcValue) - VBATT_OFFSET) * VBATT_REVERSE_GAIN ;
}

double ADC_LoadVoltMeasurement(unsigned int adcValue, double offset) {    
    return (ADC_ToVolt(adcValue) - offset) * VLOAD_REVERSE_GAIN ;
}


// Current Measurement

double ADC_PvCurrentMeasurement(unsigned int adcValue) {    
    return (ADC_ToVolt(adcValue) - IPV_OFFSET) * IPV_REVERSE_GAIN ;
}

double ADC_BatteryCurrentMeasurement(unsigned int adcValue) {
    return (ADC_ToVolt(adcValue) - IBATT_OFFSET) * IBATT_REVERSE_GAIN ;
}

double ADC_LoadCurrentMeasurement(unsigned int adcValue, double offset) {
    double current = (ADC_ToVolt(adcValue) - offset) * ILOAD_REVERSE_GAIN;
    //if (current > 0) {
    //    if (current < 0) current = 0;
    //}
    //else if (current < 0) {
    //    if (current > 0) current = 0;
    //}
    //return current;
    return current;
}

// Basic ADC function

double ADC_ToVolt(unsigned int adcValue) {
    return (adcValue/4096.0) * VREF;
}

AdcData ADC_GetData(void) {
    while(!dataComplete);
    return data;
}

char* ADC_GetStringData(void) {
    return stringData;
}

double ADC_GetLdrVolt(void) {
    while(!dataComplete);
    return data.vLdr;
}

// DMA Interrupt

unsigned int DmaBuffer = 0;

void __attribute__((interrupt, no_auto_psv)) _DMA0Interrupt(void)
{
    int i;
    for (i=0; i<MAX_CHNUM; ++i) {
        if (DmaBuffer == 0)
            result[i] = BufferA[i][0];
        else
            result[i] = BufferB[i][0];
    }
    
    if (offsetComplete == 2) {
        sumVPV += ADC_PvVoltMeasurement(result[VPV]);
        sumIPV += ADC_PvCurrentMeasurement(result[IPV]);
        sumVBatt += ADC_BatteryVoltMeasurement(result[VBATT]);
        sumIBatt += ADC_BatteryCurrentMeasurement(result[IBATT]);    
        sumVLdr += ADC_ToVolt(result[VLDR]); 
    
        data.vLoad = ADC_LoadVoltMeasurement(result[VLOAD], vLoadOffset);
        data.iLoad = ADC_LoadCurrentMeasurement(result[ILOAD], iLoadOffset); 
        
        sumSquareVLoad += data.vLoad * data.vLoad;
        sumSquareILoad += data.iLoad * data.iLoad;
        sumVILoad += data.vLoad * data.iLoad;
    }    
    else {
        sumVLoad += ADC_ToVolt(result[VLOAD]);
        sumILoad += ADC_ToVolt(result[ILOAD]);
    }

    ++ldrSampleCount;
    ++sampleCount; 
    
    if (ldrSampleCount == LDR_SAMPLE_COUNT) {
        data.vLdr = sumVLdr / LDR_SAMPLE_COUNT;
        sumVLdr = 0;
        ldrSampleCount = 0;
    }
    
    if (sampleCount == SAMPLE_COUNT) { 
        
        if (offsetComplete == 2) {
            data.vLoadRms = sqrtl(sumSquareVLoad / SAMPLE_COUNT);
            data.iLoadRms = sqrtl(sumSquareILoad / SAMPLE_COUNT);
        
            data.vPV = sumVPV / SAMPLE_COUNT;
            data.iPV = sumIPV / SAMPLE_COUNT;
            data.vBatt = sumVBatt / SAMPLE_COUNT;
            data.iBatt = sumIBatt / SAMPLE_COUNT;            
        
            // error fix
        
            const double iPvErrorFix = 0.1748 - data.iPV*0.0283;
            if(iPvErrorFix > 0) {
                data.iPV += iPvErrorFix;
            }                
        
            if(data.vPV <= 10) {
                const double vPvErrorFix =  0.3498 - data.vPV*0.026;
                data.vPV += vPvErrorFix;
            }            
            else if(data.vPV > 20) {
                data.vPV -= 0.1;
            }
            
        
            data.iBatt += 0.07;
            if(data.iBatt <= 0.8)
                data.iBatt += 0.05;
            else if(data.iBatt <= 2)
                data.iBatt += 0.02;
                
            if (data.vBatt < 12.0) {
                const double vBattErrorFix = 0.4102 - 0.0237 * data.vBatt;
                data.vBatt += vBattErrorFix;
            }
            
            // set zero if near zero
            
            if(data.vLoadRms <  10) {                
                data.vLoadRms = 0.0;
            }
            if(data.iLoadRms < 0.05) {
                data.iLoadRms = 0.0;
            }
        
            if ((data.iPV > 0 && data.iPV < 0.15) || (data.iPV < 0 && data.iPV > -0.15))
                data.iPV = 0;
        
            if ((data.vPV > 0 && data.vPV < 1.5) || (data.vPV < 0 && data.vPV > -1.5))
                data.vPV = 0;
        
            if ((data.vBatt > 0 && data.vBatt < 1.5) || (data.vBatt < 0 && data.vBatt > -1.5))
                data.vBatt = 0;
        
            if ((data.iBatt > 0 && data.iBatt < 0.15) || (data.iBatt < 0 && data.iBatt > -0.15))
                data.iBatt = 0;
            
            // Calculate Power & PF
            data.sLoad = data.vLoadRms * data.iLoadRms;   
            
            if (data.sLoad != 0 ) data.pLoad = sumVILoad / SAMPLE_COUNT;
            else data.pLoad = 0;
            
            if (data.sLoad >= 5) data.pfLoad = data.pLoad / data.sLoad;
            else data.pfLoad = 0.0;            
            if (1.0 - data.pfLoad < 0.025)
                data.pfLoad = 1.0;                        
            
            data.pBatt =  data.vBatt * data.iBatt;            
            data.pPv = data.vPV * data.iPV;
            
            if (data.pBatt == 0) {
                data.efficiency = 100.00;
            }
            else data.efficiency = 100.00 * (data.pLoad/data.pBatt);
            
            const double sec = T89_GetSec();
            T89_Reset();
            data.eLoad += (data.pLoad * sec) / (3600000.0);
            data.eBatt += (data.pBatt * sec) / (3600000.0);           
            data.ePv += (data.pPv * sec) / (3600000.0);
        
            sprintf(stringData, 
                "%.1f, %.3f, %.2f, %.4Lf, %.2f, "
                "%.2f, %.2f, %.2f, %.2f, %.4Lf, "
                "%.2f, %.2f, %.2f, %.4Lf, "                
                "%.2f",
                data.vLoadRms, data.iLoadRms,data.pLoad, data.eLoad, data.pfLoad,
                data.vBatt, data.iBatt, data.pBatt, data.percentBatt, data.eBatt,
                data.vPV, data.iPV, data.pPv, data.ePv, 
                data.efficiency);                          
        
            dataComplete = 1;
            SDC_Update();
        }
        else {
            vLoadOffset = sumVLoad/SAMPLE_COUNT;
            iLoadOffset = sumILoad/SAMPLE_COUNT;
        
            sumVLoad = 0;
            sumILoad = 0;
            offsetComplete += 1;   
            T89_Reset();
        }
        
        //Reset All
        sumVPV = 0;
        sumIPV = 0;
        sumVBatt = 0;
        sumIBatt = 0; 
        
        sumSquareVLoad = 0;
        sumSquareILoad = 0;
        sumVILoad = 0;
        sampleCount = 0;
    }    
     
    DmaBuffer ^= 1;  // ^ = OR
    IFS0bits.DMA0IF = 0;		// Clear the DMA0 Interrupt Flag
}

// ADC

void ADC_Init(void) {

    AD1CON1bits.FORM = 0;	  	// Unsinged Integer Format
    AD1CON1bits.SSRC = 7;		// Auto conversion
    AD1CON1bits.ASAM = 0;		// Sampling begins immediately after last conversion. SAMP bit is auto-set    
    
    AD1CON1bits.AD12B = 1;		// 12-bit ADC operation	

    AD1CON2bits.ALTS = 0;
    AD1CHS0bits.CH0NA = 0;   
    AD1CON2bits.CSCNA = 1;		// Scan Input Selections for CH0+ during Sample A bit
    AD1CON2bits.CHPS = 0;		// Converts CH0
    
    AD1CON3bits.ADRC = 0;		// ADC Clock is derived from Systems Clock
    AD1CON3bits.SAMC = 6;		// Auto Sample Time = 6 * Tad	
    AD1CON3bits.ADCS = 5;		// 1 Tad = 6/40
    
    AD1CON1bits.ADDMABM = 0;                // DMA buffers are built in scatter/gather mode
    AD1CON2bits.SMPI = (NUM_CHS2SCAN-1);	// Genertate DMA interrupt every NUM_CHS2SCAN sample
        
    AD1CON4bits.DMABL = 3;                  // Each buffer contains 8 words

    AD1CSSH = 0x0000;
    AD1CSSL = 0x007F;	 // Enable AN0-AN7 (except AN6) for channel scan
 
    AD1PCFGH=0xFFFF;
    AD1PCFGL=0xFF80;    // AN0-AN7 (except AN6) as Analog Input
    
    IFS0bits.AD1IF   = 0;	// Clear the A/D interrupt flag bit
    IEC0bits.AD1IE   = 0;	// Do Not Enable A/D interrupt
           
    AD1CON1bits.ADON = 1;	// Turn on the A/D converter
    delay_ms(1);			// Delay to allow ADC to settle         
    AD1CON1bits.ASAM = 1;
        
    DMA_Init();             // Settle DMA
}

void DMA_Init(void) {
    
    DMA0CONbits.AMODE = 2;			// Configure DMA for Peripheral indirect mode
    DMA0CONbits.MODE  = 2;			// Configure DMA for Continuous Ping-Pong mode
    DMA0PAD=(int)&ADC1BUF0;         // Peripheral Address Register bits
    DMA0CNT = (SAMP_BUFF_SIZE*NUM_CHS2SCAN)-1; //DMA Transfer Count Register bits		
    DMA0REQ = 13;					// Select ADC1 as DMA Request source

    DMA0STA = __builtin_dmaoffset(BufferA);		
    DMA0STB = __builtin_dmaoffset(BufferB);

    IFS0bits.DMA0IF = 0;			//Clear the DMA interrupt flag bit
    IEC0bits.DMA0IE = 1;			//Set the DMA interrupt enable bit

    DMA0CONbits.CHEN=1;				// Enable DMA
}

double ADC_GetLux(void) {
    const double volt = ADC_GetLdrVolt();
    return 28642000000 * powl((volt * 10000 / 3.3 - volt), -2.6028);
}

double ADC_GetPercentBattery(void) {
    return data.percentBatt;
}