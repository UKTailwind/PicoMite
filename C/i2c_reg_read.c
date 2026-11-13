/* Read an 8-bit register over I2C (repeated START). Configure I2C pins
   with BASIC SETPIN before running. The device must support register reads.
   RUN "B:/i2c_reg_read.c", "0x68 0x00" */
#include <picomite/i2c.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        puts("Usage: i2c_reg_read.c ADDRESS REGISTER (e.g. 0x68 0x00)");
        return 1;
    }
    char *end;
    unsigned address = (unsigned)strtoul(argv[1], &end, 0);
    if (*end || address > 127)
    {
        puts("Invalid 7-bit address");
        return 1;
    }
    unsigned regno = (unsigned)strtoul(argv[2], &end, 0);
    if (*end || regno > 255)
    {
        puts("Invalid 8-bit register");
        return 1;
    }
    unsigned char reg = (unsigned char)regno, value = 0;
    pm_i2c_t bus = pm_i2c_open(0, 100, 1000);
    int status = pm_i2c_transfer(bus, address, &reg, 1, &value, 1);
    if (status == 0)
        printf("I2C 0x%02X register 0x%02X = 0x%02X\n", address, regno, value);
    else
        printf("I2C error: %s\n", status == 2 ? "timeout" : "NACK/short transfer");
    pm_i2c_close(bus);
    return status != 0;
}
