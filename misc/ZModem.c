/***********************************************************************************************************************
PicoMite MMBasic

ZModem.c

ZMODEM RECEIVE ["file"]: receive one or more files from a ZMODEM sender (Tera Term, lrzsz sz) over the console.

The protocol engine (frame parser, header handling, CRC checks, ZRPOS recovery) is adapted from the receive side of
Tera Term's teraterm/ttpfile/zmodem.c, under the licence below. The console, file, timing and flow-control layers are
MMBasic's: file data is held in RAM and written only while the sender waits for an acknowledgement, because a flash
write stops the console from receiving.

************************************************************************************************************************/
/*
 * Copyright (C) 1994-1998 T. Teranishi
 * (C) 2007- TeraTerm Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

#if defined(rp2350)

uint32_t lfs_crc(uint32_t crc, const void *buffer, size_t size); // littlefs: reflected CRC-32, the one ZMODEM uses

#define ZPAD '*'
#define ZDLE 0x18
#define ZBIN 'A'
#define ZHEX 'B'
#define ZBIN32 'C'
#define XON 0x11

#define ZRQINIT 0
#define ZRINIT 1
#define ZSINIT 2
#define ZACK 3
#define ZFILE 4
#define ZSKIP 5
#define ZNAK 6
#define ZABORT 7
#define ZFIN 8
#define ZRPOS 9
#define ZDATA 10
#define ZEOF 11
#define ZFERR 12

#define ZCRCE 'h'
#define ZCRCG 'i'
#define ZCRCQ 'j'
#define ZCRCW 'k'
#define ZRUB0 'l'
#define ZRUB1 'm'

#define ZF0 3 // header byte order: flags count down, position counts up
#define ZP0 0
#define ZP1 1
#define ZP2 2
#define ZP3 3

#define CANFDX 0x01
#define CANFC32 0x20

// File data is kept in RAM until the sender pauses for a ZACK (ZCRCQ/ZCRCW). Tera Term pauses after its
// window (32 KB by default). lrzsz streams on regardless of the buffer length in ZRINIT, so when the RAM
// is full the receiver asks for the data again from its position (ZRPOS): lrzsz restarts with a ZCRCW
// subpacket and waits for the ZACK, and the RAM is written while it waits. A sender that streams on
// after that is written to mid-stream; the bytes lost meanwhile fail a CRC and ZRPOS recovers them.
// A USB console loses nothing during a write (USB holds the sender back), so there the RAM is simply
// written when it is full.
#define ZSTAGE (36 * 1024)
#define ZRXBUFLEN 32768
#define ZMAXDATA 1024 // largest data subpacket the spec allows

#define ZT_INIT 3000000 // us between ZRINITs while waiting for a sender
#define ZT_INIT_TRIES 20
#define ZT_DATA 3000000 // us of silence in the middle of data before asking for it again
#define ZT_REPLY 1000000 // us to wait for the answer to a ZACK, ZRPOS or a later ZRINIT: a lost reply costs this
#define ZT_DATA_TRIES 30
#define ZT_FIN 1000000 // us to wait for the sender's "OO"

enum
{
    Z_RecvInit = 1,
    Z_RecvInit2,
    Z_RecvData,
    Z_RecvFIN,
    Z_End
};

enum
{
    Z_PktGetPAD = 1,
    Z_PktGetDLE,
    Z_PktHdrFrm,
    Z_PktGetBin,
    Z_PktGetHex,
    Z_PktGetHexEOL,
    Z_PktGetData,
    Z_PktGetCRC
};

typedef struct
{
    uint8_t RxHdr[4], TxHdr[4];
    uint8_t RxType, TERM;
    uint8_t PktIn[ZMAXDATA + 8];
    int PktInPtr, PktInCount;
    bool CRC32, HexLo, Quoted;
    uint16_t CRC;
    uint32_t CRC3;
    int ZState, ZPktState, CanCount;
    uint32_t Pos;      // bytes of the current file accepted so far
    uint8_t *stage;    // accepted bytes not yet written to the file
    int stagelen;
    int fnbr;
    bool fileopen;
    bool held; // the RAM was full and a ZRPOS asked the sender to restart and pause
    const char *target; // name given to ZMODEM RECEIVE, or NULL to use the sender's names
    char name[FF_MAX_LFN];
    int files;     // files completed
    int tries;     // timeouts in a row
    uint32_t timeout; // us of silence before ZTimeOut
    uint32_t bytes;   // total bytes received
    char err[MAXERRMSG];
} zrx_t;

static uint16_t zcrc16(uint8_t b, uint16_t crc)
{
    crc ^= (uint16_t)b << 8;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    return crc;
}

static uint32_t zcrc32(uint8_t b, uint32_t crc)
{
    return lfs_crc(crc, &b, 1);
}

// next byte from the console, or -1 after timeout_us of silence
static int zgetc(uint32_t timeout_us)
{
    uint64_t end = time_us_64() + timeout_us;
    while (1)
    {
        if (ConsoleRxBufHead != ConsoleRxBufTail)
        {
            int c = (uint8_t)ConsoleRxBuf[ConsoleRxBufTail];
            ConsoleRxBufTail = (ConsoleRxBufTail + 1) % CONSOLE_RX_BUF_SIZE;
            return c;
        }
        int c = getConsole(); // the line is idle: let the background work (WiFi, Bluetooth, USB) run
        if (c != -1)
            return c;
        if (time_us_64() >= end)
            return -1;
    }
}

static void zputhex(uint8_t b)
{
    static const char hex[] = "0123456789abcdef";
    SerialConsolePutC(hex[b >> 4], 0);
    SerialConsolePutC(hex[b & 15], 0);
}

static void ZStoHdr(zrx_t *z, uint32_t pos)
{
    z->TxHdr[ZP0] = pos;
    z->TxHdr[ZP1] = pos >> 8;
    z->TxHdr[ZP2] = pos >> 16;
    z->TxHdr[ZP3] = pos >> 24;
}

static uint32_t ZRclHdr(zrx_t *z)
{
    return z->RxHdr[ZP0] | (z->RxHdr[ZP1] << 8) | (z->RxHdr[ZP2] << 16) | ((uint32_t)z->RxHdr[ZP3] << 24);
}

// send a hex header (what a receiver always uses)
static void ZShHdr(zrx_t *z, uint8_t type)
{
    uint16_t crc = zcrc16(type, 0);
    SerialConsolePutC(ZPAD, 0);
    SerialConsolePutC(ZPAD, 0);
    SerialConsolePutC(ZDLE, 0);
    SerialConsolePutC(ZHEX, 0);
    zputhex(type);
    for (int i = 0; i < 4; i++)
    {
        zputhex(z->TxHdr[i]);
        crc = zcrc16(z->TxHdr[i], crc);
    }
    zputhex(crc >> 8);
    zputhex(crc);
    SerialConsolePutC(0x0D, 0);
    if (type == ZFIN || type == ZACK)
        SerialConsolePutC(0x8A, 1);
    else
    {
        SerialConsolePutC(0x8A, 0);
        SerialConsolePutC(XON, 1);
    }
}

static void ZSendRInit(zrx_t *z)
{
    z->Pos = 0;
    ZStoHdr(z, 0);
    z->TxHdr[ZP0] = ZRXBUFLEN & 0xFF; // receive buffer length: lrzsz pauses for a ZACK after this much
    z->TxHdr[ZP1] = ZRXBUFLEN >> 8;
    z->TxHdr[ZF0] = CANFDX | CANFC32;
    ZShHdr(z, ZRINIT);
    z->timeout = z->files ? ZT_REPLY : ZT_INIT; // the first waits for a person to start the sender
}

static void ZSendRPOS(zrx_t *z)
{
    ZStoHdr(z, z->Pos);
    ZShHdr(z, ZRPOS);
    z->timeout = ZT_REPLY;
}

static void ZSendACK(zrx_t *z)
{
    ZStoHdr(z, z->Pos);
    ZShHdr(z, ZACK);
    z->timeout = ZT_REPLY;
}

static void ZSendCancel(void)
{
    for (int i = 0; i < 8; i++)
        SerialConsolePutC(ZDLE, 0);
    for (int i = 0; i < 10; i++)
        SerialConsolePutC(0x08, i == 9);
}

// end the session with an error: the sender is told, the reason is reported when the command returns
static void zfail(zrx_t *z, const char *msg)
{
    if (!*z->err)
        strncpy(z->err, msg, MAXERRMSG - 1);
    ZSendCancel();
    z->ZState = Z_End;
}

// write the bytes held in RAM to the file (the sender is paused, or has run past the buffer)
static void zflush(zrx_t *z)
{
    if (z->stagelen == 0 || !z->fileopen)
        return;
    FSerror = 0;
    FilePutData((char *)z->stage, z->fnbr, z->stagelen);
    z->stagelen = 0;
    if (FSerror)
        zfail(z, *MMErrMsg ? MMErrMsg : "File write failed");
}

static void zclose(zrx_t *z)
{
    if (!z->fileopen)
        return;
    zflush(z);
    z->fileopen = false;
    FSerror = 0;
    FileClose(z->fnbr);
    if (FSerror)
        zfail(z, *MMErrMsg ? MMErrMsg : "File close failed");
}

// ZFILE data: "name\0size mtime ..."; open the file the data will go to
static bool ZParseFile(zrx_t *z)
{
    if (z->ZState != Z_RecvInit && z->ZState != Z_RecvInit2)
        return false;
    z->PktIn[z->PktInPtr] = 0; // for safety
    if (z->target)
    {
        if (z->files)
            return false; // one file was asked for: skip the rest of a batch
        strncpy(z->name, z->target, FF_MAX_LFN - 1);
    }
    else
    {
        // the sender's name, without any path it sent
        char *n = (char *)z->PktIn, *p;
        if ((p = strrchr(n, '/')))
            n = p + 1;
        if ((p = strrchr(n, '\\')))
            n = p + 1;
        if (!*n)
            return false;
        strncpy(z->name, n, FF_MAX_LFN - 1);
    }
    MMErrMsg[0] = 0;
    if (!BasicFileOpen(z->name, z->fnbr, FA_WRITE | FA_CREATE_ALWAYS))
    {
        zfail(z, *MMErrMsg ? MMErrMsg : "Cannot create the file");
        return false;
    }
    z->fileopen = true;
    z->held = false;
    z->stagelen = 0;
    z->Pos = 0;
    ZStoHdr(z, 0);
    z->ZState = Z_RecvData;
    z->timeout = ZT_DATA;
    return true;
}

// a good data subpacket: hold its bytes until the sender pauses
static bool ZWriteData(zrx_t *z)
{
    if (z->ZState != Z_RecvData)
        return false;
    if (z->stagelen + z->PktInPtr > ZSTAGE)
    {
        if (z->TERM == ZCRCG && !z->held && Option.SerialConsole)
        {
            // full, and the sender is still streaming over a UART: drop this subpacket and ask for it
            // again; lrzsz restarts with a ZCRCW and waits, so the write below happens while it waits
            z->held = true;
            ZSendRPOS(z);
            return false;
        }
        zflush(z); // the sender waits (ZCRCW/ZCRCQ), streamed on after the ZRPOS, or is on USB: write now
        if (z->ZState == Z_End)
            return false;
    }
    z->held = false;
    memcpy(z->stage + z->stagelen, z->PktIn, z->PktInPtr);
    z->stagelen += z->PktInPtr;
    z->Pos += z->PktInPtr;
    z->bytes += z->PktInPtr;
    ZStoHdr(z, z->Pos);
    z->timeout = ZT_DATA;
    return true;
}

static void ZResetPkt(zrx_t *z)
{
    z->Quoted = false;
    z->CRC = 0;
    z->CRC3 = 0xFFFFFFFF;
    z->PktInPtr = 0;
    z->PktInCount = 0;
}

// a data subpacket and its CRC have arrived
static void ZCheckData(zrx_t *z)
{
    bool ok;
    if ((z->CRC32 && z->CRC3 != 0xDEBB20E3) || (!z->CRC32 && z->CRC != 0))
    {
        if (z->ZState == Z_RecvData)
            ZSendRPOS(z);
        else if (z->ZState == Z_RecvInit || z->ZState == Z_RecvInit2)
        {
            ZStoHdr(z, 0);
            ZShHdr(z, ZNAK);
        }
        z->ZPktState = Z_PktGetPAD;
        return;
    }
    switch (z->RxType)
    {
    case ZSINIT:
        ok = (z->ZState == Z_RecvInit);
        if (ok)
            z->ZState = Z_RecvInit2;
        break;
    case ZFILE:
        ok = ZParseFile(z);
        if (!ok && z->ZState != Z_End)
        {
            ZStoHdr(z, 0);
            ZShHdr(z, ZSKIP);
        }
        break;
    case ZDATA:
        ok = ZWriteData(z);
        break;
    default:
        ok = false;
    }
    if (!ok || z->ZState == Z_End)
    {
        z->ZPktState = Z_PktGetPAD;
        return;
    }
    if (z->RxType == ZFILE)
        ZShHdr(z, ZRPOS); // start from 0 (ZParseFile set the position)

    switch (z->TERM)
    {
    case ZCRCG: // more data follows, no reply
        z->ZPktState = Z_PktGetData;
        break;
    case ZCRCQ: // more data follows, the sender waits for a ZACK
        z->ZPktState = Z_PktGetData;
        if (z->RxType != ZFILE)
        {
            zflush(z);
            if (z->ZState != Z_End)
                ZSendACK(z);
        }
        break;
    case ZCRCW: // end of frame, the sender waits for a ZACK
        z->ZPktState = Z_PktGetPAD;
        if (z->RxType != ZFILE)
        {
            zflush(z);
            if (z->ZState != Z_End)
                ZSendACK(z);
        }
        break;
    default: // ZCRCE: end of frame, a header follows
        z->ZPktState = Z_PktGetPAD;
    }
    if (z->ZPktState == Z_PktGetData)
        ZResetPkt(z);
}

// a header has arrived: check its CRC and pick it apart
static bool ZCheckHdr(zrx_t *z)
{
    bool ok;
    if (z->CRC32)
    {
        uint32_t crc = 0xFFFFFFFF;
        for (int i = 0; i <= 8; i++)
            crc = zcrc32(z->PktIn[i], crc);
        ok = crc == 0xDEBB20E3;
    }
    else
    {
        uint16_t crc = 0;
        for (int i = 0; i <= 6; i++)
            crc = zcrc16(z->PktIn[i], crc);
        ok = crc == 0;
    }
    if (!ok)
    {
        if (z->ZState == Z_RecvInit)
            ZSendRInit(z);
        else if (z->ZState == Z_RecvData)
            ZSendRPOS(z);
    }
    z->RxType = z->PktIn[0];
    for (int i = 1; i <= 4; i++)
        z->RxHdr[i - 1] = z->PktIn[i];
    return ok;
}

static void ZParseHdr(zrx_t *z)
{
    switch (z->RxType)
    {
    case ZRQINIT:
        if (z->ZState == Z_RecvInit)
            ZSendRInit(z);
        break;
    case ZSINIT:
        z->ZPktState = Z_PktGetData;
        break;
    case ZFILE:
        z->ZPktState = Z_PktGetData;
        break;
    case ZDATA:
        if (z->ZState != Z_RecvData)
            break;
        if (z->Pos != ZRclHdr(z))
        {
            ZSendRPOS(z); // not where we are: ask for the data from our position
            return;
        }
        z->ZPktState = Z_PktGetData;
        z->timeout = ZT_DATA;
        break;
    case ZEOF:
        if (z->ZState != Z_RecvData)
            break;
        if (z->Pos != ZRclHdr(z))
            break; // sent before our ZRPOS reached the sender, which is coming back for the data; answering
                   // with another ZRPOS makes it send the data twice (as lrzsz rz, a lost ZRPOS times out)
        zclose(z);
        if (z->ZState == Z_End)
            return;
        z->files++;
        z->ZState = Z_RecvInit;
        ZSendRInit(z); // ready for the next file, or ZFIN
        break;
    case ZFIN:
        zclose(z);
        if (z->ZState == Z_End)
            return;
        z->ZState = Z_RecvFIN;
        ZStoHdr(z, 0);
        ZShHdr(z, ZFIN);
        z->CanCount = 2; // now wait for "OO"
        z->timeout = ZT_FIN;
        break;
    case ZABORT:
    case ZFERR:
        zfail(z, "Cancelled by remote");
        break;
    default:
        break;
    }
    ZResetPkt(z);
}

// one byte from the line through the frame parser
static void ZParse(zrx_t *z, uint8_t b)
{
    if ((b & 0x7F) == 0x11 || (b & 0x7F) == 0x13)
        return; // flow control characters are escaped in data: a bare one is noise
    if (z->ZState == Z_RecvFIN)
    {
        if (b == 'O' && --z->CanCount <= 0)
            z->ZState = Z_End;
        return;
    }
    if (b == ZDLE)
    {
        if (--z->CanCount <= 0)
        {
            if (!*z->err)
                strcpy(z->err, "Cancelled by remote");
            z->ZState = Z_End;
            return;
        }
    }
    else
        z->CanCount = 5;

    switch (z->ZPktState)
    {
    case Z_PktGetPAD:
        if (b == ZPAD)
            z->ZPktState = Z_PktGetDLE;
        break;
    case Z_PktGetDLE:
        if (b == ZDLE)
            z->ZPktState = Z_PktHdrFrm;
        else if (b != ZPAD)
            z->ZPktState = Z_PktGetPAD;
        break;
    case Z_PktHdrFrm:
        switch (b)
        {
        case ZBIN:
            z->CRC32 = false;
            z->PktInCount = 7;
            z->ZPktState = Z_PktGetBin;
            break;
        case ZHEX:
            z->HexLo = false;
            z->CRC32 = false;
            z->PktInCount = 7;
            z->ZPktState = Z_PktGetHex;
            break;
        case ZBIN32:
            z->CRC32 = true;
            z->PktInCount = 9;
            z->ZPktState = Z_PktGetBin;
            break;
        default:
            z->ZPktState = Z_PktGetPAD;
        }
        z->Quoted = false;
        z->PktInPtr = 0;
        break;
    case Z_PktGetBin:
        if (b == ZDLE)
        {
            z->Quoted = true;
            break;
        }
        if (z->Quoted)
        {
            b = (b == ZRUB0) ? 0x7F : (b == ZRUB1) ? 0xFF : b ^ 0x40;
            z->Quoted = false;
        }
        z->PktIn[z->PktInPtr++] = b;
        if (--z->PktInCount == 0)
        {
            z->ZPktState = Z_PktGetPAD;
            if (ZCheckHdr(z))
                ZParseHdr(z);
        }
        break;
    case Z_PktGetHex:
        if (b >= '0' && b <= '9')
            b -= '0';
        else if (b >= 'a' && b <= 'f')
            b -= 'a' - 10;
        else
        {
            z->ZPktState = Z_PktGetPAD;
            break;
        }
        if (z->HexLo)
        {
            z->PktIn[z->PktInPtr++] += b;
            z->HexLo = false;
            if (--z->PktInCount <= 0)
            {
                z->ZPktState = Z_PktGetHexEOL;
                z->PktInCount = 2;
            }
        }
        else
        {
            z->PktIn[z->PktInPtr] = b << 4;
            z->HexLo = true;
        }
        break;
    case Z_PktGetHexEOL:
        if (--z->PktInCount <= 0)
        {
            z->ZPktState = Z_PktGetPAD;
            if (ZCheckHdr(z))
                ZParseHdr(z);
        }
        break;
    case Z_PktGetData:
        if (b == ZDLE)
        {
            z->Quoted = true;
            break;
        }
        if (z->Quoted)
        {
            z->Quoted = false;
            switch (b)
            {
            case ZCRCE:
            case ZCRCG:
            case ZCRCQ:
            case ZCRCW:
                z->TERM = b;
                z->PktInCount = z->CRC32 ? 4 : 2;
                z->ZPktState = Z_PktGetCRC;
                break;
            case ZRUB0:
                b = 0x7F;
                break;
            case ZRUB1:
                b = 0xFF;
                break;
            default:
                b ^= 0x40;
            }
        }
        if (z->CRC32)
            z->CRC3 = zcrc32(b, z->CRC3);
        else
            z->CRC = zcrc16(b, z->CRC);
        if (z->ZPktState == Z_PktGetData)
        {
            if (z->PktInPtr < ZMAXDATA)
                z->PktIn[z->PktInPtr++] = b;
            else
            {
                // longer than any subpacket: we have lost the frame end, ask again now rather than time out
                z->ZPktState = Z_PktGetPAD;
                if (z->ZState == Z_RecvData)
                    ZSendRPOS(z);
            }
        }
        break;
    case Z_PktGetCRC:
        if (b == ZDLE)
        {
            z->Quoted = true;
            break;
        }
        if (z->Quoted)
        {
            b = (b == ZRUB0) ? 0x7F : (b == ZRUB1) ? 0xFF : b ^ 0x40;
            z->Quoted = false;
        }
        if (z->CRC32)
            z->CRC3 = zcrc32(b, z->CRC3);
        else
            z->CRC = zcrc16(b, z->CRC);
        if (--z->PktInCount <= 0)
            ZCheckData(z);
        break;
    }
}

// nothing arrived for z->timeout: prompt the sender again, or give up
static void ZTimeOut(zrx_t *z)
{
    switch (z->ZState)
    {
    case Z_RecvInit:
        if (++z->tries > ZT_INIT_TRIES)
            zfail(z, "Sender did not respond");
        else
            ZSendRInit(z);
        break;
    case Z_RecvInit2:
        if (++z->tries > ZT_DATA_TRIES)
            zfail(z, "Sender stopped responding");
        else
            ZSendACK(z);
        break;
    case Z_RecvData:
        if (++z->tries > ZT_DATA_TRIES)
            zfail(z, "Sender stopped responding");
        else
            ZSendRPOS(z);
        break;
    case Z_RecvFIN:
        z->ZState = Z_End; // the "OO" is a courtesy
        break;
    }
    z->ZPktState = Z_PktGetPAD;
}

static void zreceive(zrx_t *z)
{
    z->ZState = Z_RecvInit;
    z->ZPktState = Z_PktGetPAD;
    z->CanCount = 5;
    ZResetPkt(z);
    ZSendRInit(z);
    while (z->ZState != Z_End)
    {
        int c = zgetc(z->timeout);
        if (c < 0)
        {
            ZTimeOut(z);
            continue;
        }
        z->tries = 0;
        ZParse(z, c);
    }
}

void cmd_zmodem(void)
{
    if (mytoupper(*cmdline) != 'R')
        error("Only ZMODEM RECEIVE is available");
    while (isalpha(*cmdline))
        cmdline++;
    skipspace(cmdline);
    zrx_t *z = GetTempMemory(sizeof(zrx_t));
    if (*cmdline && *cmdline != '\'')
        z->target = (char *)getFstring(cmdline);
    z->stage = GetTempMemory(ZSTAGE);
    z->fnbr = FindFreeFileNbr();

    ClearExternalIO();
    char BreakKeySave = BreakKey;
    BreakKey = 0; // the data can hold any byte, Ctrl-C included
    int abortsave = OptionFileErrorAbort;
    OptionFileErrorAbort = 0; // a file error must end the session cleanly, not jump out of it
    zreceive(z);
    if (z->fileopen) // the session ended part way through a file: keep what arrived
    {
        zflush(z);
        z->fileopen = false;
        FileClose(z->fnbr);
    }
    OptionFileErrorAbort = abortsave;
    BreakKey = BreakKeySave;
    busy_wait_ms(50); // let the sender finish before anything is printed
    while (getConsole() != -1)
        ;
    if (*z->err)
        error("$", z->err);
    if (z->target)
        MMPrintString("Received ");
    else
    {
        PInt(z->files);
        MMPrintString(z->files == 1 ? " file received, " : " files received, ");
    }
    if (z->target)
    {
        MMPrintString(z->name);
        MMPrintString(", ");
    }
    PInt(z->bytes);
    MMPrintString(" bytes\r\n");
}

#endif
