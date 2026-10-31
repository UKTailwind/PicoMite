/* Read an analog GPIO. First configure it from BASIC:
   SETPIN GP26, AIN (RP2350A) or SETPIN GP40, AIN (RP2350B).
   RUN "B:/adc_read.c", "26" (or "40"). */
#include <picomite/adc.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char *end;
    long gp;
    if (argc != 2)
    {
        puts("Usage: adc_read.c GPIO_NUMBER");
        return 1;
    }
    gp = strtol(argv[1], &end, 0);
    if (end == argv[1] || *end || gp < 0 || gp > 47)
    {
        puts("Invalid GPIO number");
        return 1;
    }
    int raw = pm_adc_read_raw((int)gp);
    double volts = pm_adc_read((int)gp);
    if (raw < 0)
    {
        puts("ADC conversion fault");
        return 1;
    }
    printf("GP%ld: raw=%d, filtered=%.4f V\n", gp, raw, volts);
    return 0;
}
