/***********************************************************************************************************************
PicoMite MMBasic

SPI.c

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed
    on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed
    by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software
    without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/
/**
 * @file SPI.c
 * @author Geoff Graham, Peter Mather
 * @brief Source for SPI MMBasic commands and functions
 */
/**
 * @cond
 * The following section will be excluded from the documentation.
 */

#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "hardware/spi.h"

unsigned int *GetSendDataList(unsigned char *p, unsigned int *nbr);
unsigned int *GetReceiveDataBuffer(unsigned char *p, unsigned int *nbr, CommsRxDest *dest);

uint8_t spibits = 8;
uint8_t spi2bits = 8;
/* Each BASIC SPI CLOSE invalidates any prior native C descriptor. */
static unsigned spi_native_epoch[2];

static void SPI_MasterConfigure(int bus, unsigned speed, int mode, int bits)
{
    spi_inst_t *inst = bus == 0 ? spi0 : spi1;
    uint8_t tx = bus == 0 ? SPI0TXpin : SPI1TXpin;
    uint8_t rx = bus == 0 ? SPI0RXpin : SPI1RXpin;
    uint8_t sck = bus == 0 ? SPI0SCKpin : SPI1SCKpin;
    spi_init(inst, speed);
    spi_set_format(inst, bits, (mode & 2) != 0, (mode & 1) != 0, SPI_MSB_FIRST);
    if (bus == 0) spibits = bits;
    else spi2bits = bits;
    ExtCfg(tx, EXT_COM_RESERVED, 0);
    ExtCfg(rx, EXT_COM_RESERVED, 0);
    ExtCfg(sck, EXT_COM_RESERVED, 0);
}

unsigned SPI_MasterEpoch(int bus)
{
    return bus == 0 || bus == 1 ? spi_native_epoch[bus] : 0;
}

int SPI_MasterIsOpen(int bus)
{
    int tx, rx, sck;
    if (bus != 0 && bus != 1) return 0;
    if (bus == 0 ? SPI0locked : SPI1locked) return 0;
    tx = bus == 0 ? SPI0TXpin : SPI1TXpin;
    rx = bus == 0 ? SPI0RXpin : SPI1RXpin;
    sck = bus == 0 ? SPI0SCKpin : SPI1SCKpin;
    return tx != 99 && rx != 99 && sck != 99 &&
           ExtCurrentConfig[tx] == EXT_COM_RESERVED &&
           ExtCurrentConfig[rx] == EXT_COM_RESERVED &&
           ExtCurrentConfig[sck] == EXT_COM_RESERVED;
}

void SPI_MasterOpen(int bus, unsigned speed, int mode, int bits)
{
    int tx, rx, sck;
    if (bus != 0 && bus != 1) error("Invalid SPI bus");
    if (!speed || mode < 0 || mode > 3 || bits < 4 || bits > 16)
        error("Invalid SPI configuration");
    if (bus == 0 ? SPI0locked : SPI1locked)
        error("Allocated to System SPI");
    tx = bus == 0 ? SPI0TXpin : SPI1TXpin;
    rx = bus == 0 ? SPI0RXpin : SPI1RXpin;
    sck = bus == 0 ? SPI0SCKpin : SPI1SCKpin;
    if (tx == 99 || rx == 99 || sck == 99)
        error("Not all pins set for SPI");
    if (ExtCurrentConfig[tx] >= EXT_COM_RESERVED ||
        ExtCurrentConfig[rx] >= EXT_COM_RESERVED ||
        ExtCurrentConfig[sck] >= EXT_COM_RESERVED)
        error("SPI pins already in use");
    SPI_MasterConfigure(bus, speed, mode, bits);
}

/* Transfer count is in SPI words, not bytes. NULL selects write- or read-only. */
int SPI_MasterTransfer(int bus, const void *tx, void *rx, unsigned count)
{
    spi_inst_t *inst = bus == 0 ? spi0 : spi1;
    int bits = bus == 0 ? spibits : spi2bits;
    if (!count) return 0;
    if (bits <= 8)
    {
        if (tx && rx) return spi_write_read_blocking(inst, tx, rx, count);
        if (tx) return spi_write_blocking(inst, tx, count);
        return spi_read_blocking(inst, 0, rx, count);
    }
    if (tx && rx) return spi_write16_read16_blocking(inst, tx, rx, count);
    if (tx) return spi_write16_blocking(inst, tx, count);
    return spi_read16_blocking(inst, 0, rx, count);
}
/*  @endcond */
void cmd_spi(void)
{
    int speed;
    unsigned char *p;
    unsigned int nbr, *d;
    if (SPI0TXpin == 99 || SPI0RXpin == 99 || SPI0SCKpin == 99)
        error("Not all pins set for SPI");

    if (checkstring(cmdline, (unsigned char *)"CLOSE"))
    {
        if (!SPI0locked)
            SPIClose();
        else
            error("Allocated to System SPI");
        return;
    }

    if ((p = checkstring(cmdline, (unsigned char *)"WRITE")) != NULL)
    {
        union car
        {
            uint32_t aTxBuffer;
            uint16_t bTXBuffer[2];
            uint8_t cTxBuffer[4];
        } mybuff;
        if (ExtCurrentConfig[SPI0TXpin] < EXT_COM_RESERVED)
            error("Not open");
        d = GetSendDataList(p, &nbr);
        while (nbr--)
        {
            mybuff.aTxBuffer = *d++;
            SPI_MasterTransfer(0, &mybuff, NULL, 1);
        }
        return;
    }

    if ((p = checkstring(cmdline, (unsigned char *)"READ")) != NULL)
    {
        union car
        {
            uint32_t aRxBuffer;
            uint16_t bRXBuffer[2];
            uint8_t cRxBuffer[4];
        } mybuff;
        CommsRxDest dest;
        unsigned int *rbuf, i;
        if (ExtCurrentConfig[SPI0RXpin] < EXT_COM_RESERVED)
            error("Not open");
        rbuf = GetReceiveDataBuffer(p, &nbr, &dest);
        for (i = 0; i < nbr; i++)
        {
            mybuff.aRxBuffer = 0;
            SPI_MasterTransfer(0, NULL, &mybuff, 1);
            rbuf[i] = mybuff.aRxBuffer;
        }
        PutCommsRxData(&dest, rbuf);
        return;
    }

    p = checkstring(cmdline, (unsigned char *)"OPEN");
    if (p == NULL)
        SyntaxError();
    if (ExtCurrentConfig[SPI0TXpin] >= EXT_COM_RESERVED)
        StandardError(31);

    { // start a new block for getcsargs()
        int mode;
        getcsargs(&p, 5);
        if (argc < 3)
            StandardError(2);
        mode = getinteger(argv[2]);
        speed = getinteger(argv[0]);
        int bits = argc == 5 ? getint(argv[4], 4, 16) : 8;
        SPI_MasterConfigure(0, speed, mode, bits);
    }
}

// output and get a byte via SPI
void fun_spi(void)
{
    union car
    {
        uint64_t aTxBuffer;
        uint16_t bTXBuffer[4];
        uint8_t cTxBuffer[8];
    } inbuff, outbuff;
    inbuff.aTxBuffer = 0;
    outbuff.aTxBuffer = getinteger(ep);
    if (ExtCurrentConfig[SPI0TXpin] < EXT_COM_RESERVED)
        error("Not open");
    SPI_MasterTransfer(0, &outbuff, &inbuff, 1);
    iret = inbuff.aTxBuffer;
    targ = T_INT;
}

/*
 * @cond
 * The following section will be excluded from the documentation.
 */

void SPIClose(void)
{
    if (SPI0TXpin != 99)
    {
        if (ExtCurrentConfig[SPI0TXpin] < EXT_BOOT_RESERVED)
        {
            if (ExtCurrentConfig[SPI0TXpin] == EXT_COM_RESERVED)
                ++spi_native_epoch[0];
            spi_deinit(spi0);
            ExtCfg(SPI0TXpin, EXT_NOT_CONFIG, 0);
            ExtCfg(SPI0RXpin, EXT_NOT_CONFIG, 0); // reset to not in use
            ExtCfg(SPI0SCKpin, EXT_NOT_CONFIG, 0);
        }
    }
}
/*  @endcond */

void cmd_spi2(void)
{
    int speed;
    unsigned char *p;
    unsigned int nbr, *d;
    if (SPI1TXpin == 99 || SPI1RXpin == 99 || SPI1SCKpin == 99)
        error("Not all pins set for SPI2");

    if (checkstring(cmdline, (unsigned char *)"CLOSE"))
    {
        if (!SPI1locked)
            SPI2Close();
        else
            error("Allocated to System SPI");
        return;
    }

    if ((p = checkstring(cmdline, (unsigned char *)"WRITE")) != NULL)
    {
        union car
        {
            uint32_t aTxBuffer;
            uint16_t bTXBuffer[2];
            uint8_t cTxBuffer[4];
        } mybuff;
        if (ExtCurrentConfig[SPI1TXpin] < EXT_COM_RESERVED)
            error("Not open");
        d = GetSendDataList(p, &nbr);
        while (nbr--)
        {
            mybuff.aTxBuffer = *d++;
            SPI_MasterTransfer(1, &mybuff, NULL, 1);
        }
        return;
    }

    if ((p = checkstring(cmdline, (unsigned char *)"READ")) != NULL)
    {
        union car
        {
            uint32_t aRxBuffer;
            uint16_t bRXBuffer[2];
            uint8_t cRxBuffer[4];
        } mybuff;
        CommsRxDest dest;
        unsigned int *rbuf, i;
        if (ExtCurrentConfig[SPI1TXpin] < EXT_COM_RESERVED)
            error("Not open");
        rbuf = GetReceiveDataBuffer(p, &nbr, &dest);
        for (i = 0; i < nbr; i++)
        {
            mybuff.aRxBuffer = 0;
            SPI_MasterTransfer(1, NULL, &mybuff, 1);
            rbuf[i] = mybuff.aRxBuffer;
        }
        PutCommsRxData(&dest, rbuf);
        return;
    }

    p = checkstring(cmdline, (unsigned char *)"OPEN");
    if (p == NULL)
        SyntaxError();
    if (ExtCurrentConfig[SPI1TXpin] >= EXT_COM_RESERVED)
        StandardError(31);

    { // start a new block for getcsargs()
        int mode;
        getcsargs(&p, 5);
        if (argc < 3)
            StandardError(2);
        mode = getinteger(argv[2]);
        speed = getinteger(argv[0]);
        int bits = argc == 5 ? getint(argv[4], 4, 16) : 8;
        SPI_MasterConfigure(1, speed, mode, bits);
    }
}

// output and get a byte via SPI
void fun_spi2(void)
{
    union car
    {
        uint64_t aTxBuffer;
        uint16_t bTXBuffer[4];
        uint8_t cTxBuffer[8];
    } inbuff, outbuff;
    inbuff.aTxBuffer = 0;
    outbuff.aTxBuffer = getinteger(ep);
    if (ExtCurrentConfig[SPI1TXpin] < EXT_COM_RESERVED)
        error("Not open");
    SPI_MasterTransfer(1, &outbuff, &inbuff, 1);
    iret = inbuff.aTxBuffer;
    targ = T_INT;
}

/*
 * @cond
 * The following section will be excluded from the documentation.
 */

void SPI2Close(void)
{
    if (SPI1TXpin != 99)
    {
        if (ExtCurrentConfig[SPI1TXpin] < EXT_BOOT_RESERVED)
        {
            if (ExtCurrentConfig[SPI1TXpin] == EXT_COM_RESERVED)
                ++spi_native_epoch[1];
            spi_deinit(spi1);
            ExtCfg(SPI1TXpin, EXT_NOT_CONFIG, 0);
            ExtCfg(SPI1RXpin, EXT_NOT_CONFIG, 0); // reset to not in use
            ExtCfg(SPI1SCKpin, EXT_NOT_CONFIG, 0);
        }
    }
}
void SPI_MasterClose(int bus)
{
    if (!SPI_MasterIsOpen(bus)) return;
    if (bus == 0) SPIClose();
    else SPI2Close();
}

unsigned int *GetSendDataList(unsigned char *p, unsigned int *nbr)
{
    unsigned int *buf;

    getcsargs(&p, MAX_ARG_COUNT);
    if (!(argc & 1))
        SyntaxError();
    *nbr = getint(argv[0], 0, 9999999);
    if (!*nbr)
        return NULL;
    buf = GetTempMainMemory(*nbr * sizeof(unsigned int));
    GetCommsTxData(argv, argc, 2, *nbr, buf);
    return buf;
}

unsigned int *GetReceiveDataBuffer(unsigned char *p, unsigned int *nbr, CommsRxDest *dest)
{
    getcsargs(&p, MAX_ARG_COUNT);
    if (argc < 3 || !(argc & 1))
        SyntaxError();
    *nbr = getinteger(argv[0]);
    if ((int)*nbr < 1)
        StandardError(21);
    GetCommsRxDest(argv, argc, 2, *nbr, dest);
    return GetTempMainMemory(*nbr * sizeof(unsigned int));
}
/*  @endcond */
