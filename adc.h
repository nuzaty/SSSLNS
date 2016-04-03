
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

    double vPV;
    double iPV;

    double iBatt;
    double vBatt;

    double vLdr;
} AdcData;

void ADC_Init(void);
double ADC_GetVolt(unsigned int ch);
AdcData ADC_GetData(void);
char* ADC_GetStringData(void);
double ADC_GetLdrVolt(void);

#endif