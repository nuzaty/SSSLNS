
#ifndef ADC_H
#define ADC_H

typedef struct AdcData {
    double vLoadRms;
    double iLoadRms;
    double vLoad;
    double iLoad;
    double pLoad;
    double sLoad;
    double pfLoad;
    long double eLoad;

    double vPV;
    double iPV;
    double pPv;
    long double ePv;

    double iBatt;
    double vBatt;
    double pBatt;
    double percentBatt;
    long double eBatt;
    
    double efficiency;

    double vLdr;
} AdcData;

void ADC_Init(void);
double ADC_GetVolt(unsigned int ch);
AdcData ADC_GetData(void);
char* ADC_GetStringData(void);
double ADC_GetLdrVolt(void);

double ADC_GetPercentBattery(void);

#endif