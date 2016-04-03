
#include "all.h"

//ANx Pin Configure
#define VPV 0
#define IPV 1
#define VBATT 2
#define IBATT 3
#define VLOAD 4
#define ILOAD 5
#define VLDR 6

#define SAMPLE_COUNT 4096

#define  MAX_CHNUM	 			7		// Highest Analog input number in Channel Scan
#define  SAMP_BUFF_SIZE	 		1		// Size of the input buffer per analog input
#define  NUM_CHS2SCAN			7		// Number of channels enabled for channel scan

// Number of locations for ADC buffer = 7 (AN0 to AN6) x 1 = 7 words
// Align the buffer to 7 words or 14 bytes. This is needed for peripheral indirect mode
static int BufferA[MAX_CHNUM+1][SAMP_BUFF_SIZE] __attribute__((space(dma),aligned(32)));
static int BufferB[MAX_CHNUM+1][SAMP_BUFF_SIZE] __attribute__((space(dma),aligned(32)));

static int result[MAX_CHNUM];

static long double sumSquareVLoad;
static long double sumSquareILoad;
static long double sumVILoad;

static long double sumVPV;
static long double sumIPV;
static long double sumVBatt;
static long double sumIBatt;
static long double sumVLdr;

static unsigned int sampleCount;
static char dataComplete;

static AdcData data;
static char stringData[256];

//Function Prototype
void DMA_Init(void);
double ADC_ToVolt(unsigned int adcValue);
double ADC_DcVoltMeasurement(unsigned int adcValue);
double ADC_AcVoltMeasurement(unsigned int adcValue);
double ADC_CurrentMeasurement(unsigned int adcValue);
double ADC_LoadCurrentMeasurement(unsigned int adcValue);

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
    
    sumVPV += ADC_DcVoltMeasurement(result[VPV]);;
    sumIPV += ADC_CurrentMeasurement(result[IPV]);
    sumVBatt += ADC_DcVoltMeasurement(result[VBATT]);
    sumIBatt += ADC_CurrentMeasurement(result[IBATT]);    
    sumVLdr += ADC_ToVolt(result[VLDR]);    

    data.vLoad = ADC_AcVoltMeasurement(result[VLOAD]);
    data.iLoad = ADC_LoadCurrentMeasurement(result[ILOAD]);    
    
    sumSquareVLoad += data.vLoad * data.vLoad;
    sumSquareILoad += data.iLoad * data.iLoad;
    sumVILoad += data.vLoad * data.iLoad;    

    ++sampleCount;    
        
    if (sampleCount == SAMPLE_COUNT) { 
        
        data.vPV = sumVPV/SAMPLE_COUNT;
        data.iPV = sumIPV/SAMPLE_COUNT;
        data.vBatt = sumVBatt/SAMPLE_COUNT;
        data.iBatt = sumIBatt/SAMPLE_COUNT;  
        data.vLdr = sumVLdr/SAMPLE_COUNT;
        
        data.vLoadRms = sqrtl(sumSquareVLoad / SAMPLE_COUNT);
        data.iLoadRms = sqrtl(sumSquareILoad / SAMPLE_COUNT);
        
        // Calculate Power & PF
        data.sLoad = data.vLoadRms * data.iLoadRms;   
        data.pLoad = sumVILoad / SAMPLE_COUNT;             
        data.pfLoad = data.pLoad / data.sLoad;   
        
        sprintf(stringData, 
            "%.4f, %.4f, "
            "%.4f, %.4f, "
            "%.4f, %.4f, "
            "%.4f, %.4f",
            data.vPV, data.iPV,
            data.vBatt, data.iBatt,
            data.vLoadRms, data.iLoadRms,
            data.pLoad, data.pfLoad);
    
        //Reset All
        sumVPV = 0;
        sumIPV = 0;
        sumVBatt = 0;
        sumIBatt = 0;
        sumVLdr = 0;
        sumSquareVLoad = 0;
        sumSquareILoad = 0;
        sumVILoad = 0;
        sampleCount = 0;
        
        dataComplete = 1;
    }    
     
	DmaBuffer ^= 1;  // ^ = OR
	IFS0bits.DMA0IF = 0;		// Clear the DMA0 Interrupt Flag
}

void ADC_Init(void) {

    AD1CON1bits.FORM = 0;	  	// Unsinged Integer Format
	AD1CON1bits.SSRC = 7;		// Auto conversion
    AD1CON1bits.ASAM = 0;		// Sampling begins when SAMP bit is set (for now)
    
	AD1CON1bits.AD12B = 0;		// 10-bit ADC operation	

	AD1CON2bits.CSCNA = 1;		// Scan Input Selections for CH0+ during Sample A bit
	AD1CON2bits.CHPS = 0;		// Converts CH0
    
    AD1CON3bits.ADRC = 0;		// ADC Clock is derived from Systems Clock
	AD1CON3bits.SAMC = 8;		// Auto Sample Time = 8*Tad		
	AD1CON3bits.ADCS = 3;		// ADC Conversion Clock Tad=Tcy*(ADCS+1)= (1/40M)*4 = 100ns
								// ADC Conversion Time for 10-bit Tc=12*Tad = 1200ns (833 KHz)
    
	AD1CON1bits.ADDMABM = 0;                // DMA buffers are built in scatter/gather mode
	AD1CON2bits.SMPI = (NUM_CHS2SCAN-1);	// Genertate DMA interrupt every NUM_CHS2SCAN sample
	AD1CON4bits.DMABL = 0;                  // Each buffer contains 1 words

    AD1CSSH = 0x0000;
	AD1CSSL = 0x007F;	 // Enable AN0-AN6 for channel scan
 
	AD1PCFGH=0xFFFF;
	AD1PCFGL=0xFF80;    // AN0-AN6 as Analog Input
	
	IFS0bits.AD1IF   = 0;	// Clear the A/D interrupt flag bit
	IEC0bits.AD1IE   = 0;	// Do Not Enable A/D interrupt
    
	AD1CON1bits.ADON = 1;	// Turn on the A/D converter
    delay_ms(1);			// Delay to allow ADC to settle     
    
    AD1CON1bits.ASAM = 1;	// Sampling begins immediately after last conversion. SAMP bit is auto-set
 
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

double ADC_GetVolt(unsigned int ch) {
    return ADC_ToVolt(result[ch]);
}

double ADC_ToVolt(unsigned int adcValue) {
    return (adcValue / 1024.0) * 3.30;
}

double ADC_DcVoltMeasurement(unsigned int adcValue) {
    const double REVERSE_GAIN = 17.27692478;
    return ADC_ToVolt(adcValue)* REVERSE_GAIN ;
}

double ADC_AcVoltMeasurement(unsigned int adcValue) {
    const double GAIN = -0.004223039;
    return (ADC_ToVolt(adcValue) - 1.651) / GAIN;
}

double ADC_CurrentMeasurement(unsigned int adcValue) {    
    const double SENSITIVTY = -0.1;
    const double VOLT_AT_ZERO_CURRENT = 1.6;
    return (ADC_ToVolt(adcValue) - VOLT_AT_ZERO_CURRENT) / SENSITIVTY;
}

double ADC_LoadCurrentMeasurement(unsigned int adcValue) {    
    const double SENSITIVTY = -2.780292214;
    const double VOLT_AT_ZERO_CURRENT = 1.270;
    return (ADC_ToVolt(adcValue) - VOLT_AT_ZERO_CURRENT) / SENSITIVTY;
}

AdcData ADC_GetData(void) {
    while(!dataComplete);
    return data;
}

double ADC_GetLdrVolt(void) {
    while(!dataComplete);
    return data.vLdr;
}

char* ADC_GetStringData(void) {
    while(!dataComplete);
    return stringData;
}