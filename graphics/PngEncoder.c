/***********************************************************************************************************************
PicoMite MMBasic

PngEncoder.c

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
 * @file PngEncoder.c
 * @author Geoff Graham, Peter Mather
 * @brief SAVE PNG: the display, or part of it, as an 8-bit RGB PNG (colour type 2), written a row at a time.
 *
 * cmd_save (FileIO.c) reads the rows and hands each one to PngRow. Each row gets the PNG filter whose output has
 * the smallest sum of absolute values, taken as signed bytes, as the PNG specification suggests. The filtered rows
 * are one zlib stream: a single deflate block of the fixed Huffman codes, with LZ77 matches found through a hash
 * of the next three bytes, within a 16 KB ring of the stream. The length and distance tables are upng.c's, and
 * the chunk CRC is littlefs's (the standard CRC-32). The memory is temporary, so an error frees it.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

#ifdef rp2350
// upng.c: the base values and extra bits of the length codes 257-285 and of the 30 distance codes
extern const unsigned LENGTH_BASE[29], LENGTH_EXTRA[29], DISTANCE_BASE[30], DISTANCE_EXTRA[30];

#define PNG_WINDOW 16384 // the ring of the zlib stream that matches are found in (a power of two, at most 32 KB)
#define PNG_HASHBITS 12  // the match finder's hash: the last place each of 4096 values of three bytes was seen
#define PNG_IDAT 4096    // compressed bytes per IDAT chunk

static struct
{
    int fnbr, rowbytes;     // rowbytes = 3 x the width: RGB, without the filter type byte
    uint8_t *prev, *cur;    // the row above (zeros above the first) and this row, RGB
    uint8_t *win;           // the zlib stream's last PNG_WINDOW bytes
    uint32_t *head;         // hash -> 1 + where those three bytes were last seen (0 = not seen)
    uint32_t pos;           // the bytes in the zlib stream so far
    uint8_t *out;           // the chunk being filled: 8 bytes for its length and type, then the data
    int outlen;             // the data in it
    uint32_t bits;          // the bits not yet written, the first in bit 0
    int nbits;              // how many
    uint32_t adler1, adler2; // the zlib stream's Adler-32, in its two halves
} png;

static void PngPut32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

// write a chunk whose len bytes of data are at p + 8: its length, type and data, then the CRC of the type and data
static void PngChunk(uint8_t *p, const char *type, int len)
{
    uint8_t crc[4];
    PngPut32(p, len);
    memcpy(p + 4, type, 4);
    PngPut32(crc, lfs_crc(0xffffffff, p + 4, len + 4) ^ 0xffffffff);
    FilePutData((char *)p, png.fnbr, len + 8);
    FilePutData((char *)crc, png.fnbr, 4);
}

static void PngByte(uint8_t b)
{
    png.out[8 + png.outlen++] = b;
    if (png.outlen == PNG_IDAT)
    {
        PngChunk(png.out, "IDAT", PNG_IDAT);
        png.outlen = 0;
    }
}

// n bits of v, the lowest first (deflate's order for everything but the Huffman codes)
static void PngBits(uint32_t v, int n)
{
    png.bits |= v << png.nbits;
    png.nbits += n;
    while (png.nbits >= 8)
    {
        PngByte(png.bits);
        png.bits >>= 8;
        png.nbits -= 8;
    }
}

// an n-bit Huffman code, its top bit first
static void PngCode(uint32_t code, int n)
{
    uint32_t r = 0;
    for (int i = 0; i < n; i++, code >>= 1)
        r = (r << 1) | (code & 1);
    PngBits(r, n);
}

// a byte (0-255), the end of the block (256) or a length (257-285), in deflate's fixed codes
static void PngSymbol(int s)
{
    if (s < 144)
        PngCode(0x30 + s, 8);
    else if (s < 256)
        PngCode(0x190 + s - 144, 9);
    else if (s < 280)
        PngCode(s - 256, 7);
    else
        PngCode(0xC0 + s - 280, 8);
}

// a copy of len bytes (3-258) from dist bytes back (1-32768)
static void PngMatch(unsigned len, unsigned dist)
{
    int i = 28;
    while (LENGTH_BASE[i] > len)
        i--;
    PngSymbol(257 + i);
    PngBits(len - LENGTH_BASE[i], LENGTH_EXTRA[i]);
    i = 29;
    while (DISTANCE_BASE[i] > dist)
        i--;
    PngCode(i, 5); // the fixed distance codes are the code numbers in 5 bits
    PngBits(dist - DISTANCE_BASE[i], DISTANCE_EXTRA[i]);
}

// compress the zlib stream from png.pos to end, which is in the ring
static void PngDeflate(uint32_t end)
{
    const uint32_t m = PNG_WINDOW - 1;
    uint8_t *w = png.win;
    uint32_t p = png.pos;
    while (p < end)
    {
        unsigned len = 0, dist = 0;
        if (end - p >= 3)
        {
            uint32_t h = ((w[p & m] | w[(p + 1) & m] << 8 | w[(p + 2) & m] << 16) * 2654435761u) >> (32 - PNG_HASHBITS);
            uint32_t c = png.head[h];
            png.head[h] = p + 1;
            // a match may run on past p, as deflate copies byte by byte, but not past what is in the ring
            if (c-- && end - c <= PNG_WINDOW)
            {
                unsigned max = end - p > 258 ? 258 : end - p;
                while (len < max && w[(c + len) & m] == w[(p + len) & m])
                    len++;
                dist = p - c;
            }
        }
        if (len >= 3)
        {
            PngMatch(len, dist);
            p += len;
        }
        else
            PngSymbol(w[p++ & m]);
    }
    png.pos = end;
}

static int PngPaeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (pa <= pb && pa <= pc) ? a : pb <= pc ? b : c;
}

// byte i of this row under filter f: 0 none, 1 sub (the byte to the left), 2 up, 3 average, 4 Paeth
static uint8_t PngFilter(int f, int i)
{
    int x = png.cur[i], a = i >= 3 ? png.cur[i - 3] : 0, b = png.prev[i], c = i >= 3 ? png.prev[i - 3] : 0;
    switch (f)
    {
    case 1:
        return x - a;
    case 2:
        return x - b;
    case 3:
        return x - ((a + b) >> 1);
    case 4:
        return x - PngPaeth(a, b, c);
    }
    return x;
}

/* start the file: the signature, the header (8-bit RGB) and the start of the zlib stream */
void PngBegin(int fnbr, int width, int height)
{
    static const uint8_t signature[8] = {137, 'P', 'N', 'G', '\r', '\n', 26, '\n'};
    if (width * 3 + 1 > PNG_WINDOW) // a row and its filter type byte must fit in the ring
        error("Too wide");
    memset(&png, 0, sizeof(png));
    png.fnbr = fnbr;
    png.rowbytes = width * 3;
    png.prev = GetTempMainMemory(png.rowbytes);
    png.cur = GetTempMainMemory(png.rowbytes);
    png.win = GetTempMainMemory(PNG_WINDOW);
    png.head = GetTempMainMemory(sizeof(uint32_t) << PNG_HASHBITS);
    png.out = GetTempMainMemory(8 + PNG_IDAT);
    memset(png.prev, 0, png.rowbytes);
    memset(png.head, 0, sizeof(uint32_t) << PNG_HASHBITS);
    png.adler1 = 1;
    FilePutData((char *)signature, fnbr, 8);
    uint8_t *h = png.out + 8;
    PngPut32(h, width);
    PngPut32(h + 4, height);
    h[8] = 8;                    // bits per sample
    h[9] = 2;                    // RGB
    h[10] = h[11] = h[12] = 0;   // deflate, the five filters, not interlaced
    PngChunk(png.out, "IHDR", 13);
    PngByte(0x78);               // zlib: deflate with a 32 KB window
    PngByte(0x01);               // no dictionary, and the check bits
    PngBits(1, 1);               // the last block
    PngBits(1, 2);               // of the fixed Huffman codes
}

/* add a row of the picture: 3 bytes a pixel, blue, green, red, as ReadBuffer gives them */
void PngRow(const uint8_t *bgr)
{
    const uint32_t m = PNG_WINDOW - 1;
    int n = png.rowbytes, best = 0;
    uint32_t sum[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < n; i += 3)
    {
        png.cur[i] = bgr[i + 2];
        png.cur[i + 1] = bgr[i + 1];
        png.cur[i + 2] = bgr[i];
    }
    for (int i = 0; i < n; i++)
    {
        int x = png.cur[i], a = i >= 3 ? png.cur[i - 3] : 0, b = png.prev[i], c = i >= 3 ? png.prev[i - 3] : 0;
        sum[0] += abs((int8_t)x);
        sum[1] += abs((int8_t)(x - a));
        sum[2] += abs((int8_t)(x - b));
        sum[3] += abs((int8_t)(x - ((a + b) >> 1)));
        sum[4] += abs((int8_t)(x - PngPaeth(a, b, c)));
    }
    for (int f = 1; f < 5; f++)
        if (sum[f] < sum[best])
            best = f;
    uint32_t p = png.pos, a = png.adler1, b = png.adler2;
    png.win[p++ & m] = best;
    for (int i = 0; i < n; i++)
        png.win[p++ & m] = PngFilter(best, i);
    // Adler-32, reduced at least every 5552 bytes, the most that cannot overflow 32 bits
    for (uint32_t q = png.pos; q < p;)
    {
        uint32_t k = p - q > 5552 ? q + 5552 : p;
        for (; q < k; q++)
        {
            a += png.win[q & m];
            b += a;
        }
        a %= 65521;
        b %= 65521;
    }
    png.adler1 = a;
    png.adler2 = b;
    PngDeflate(p);
    uint8_t *t = png.prev;
    png.prev = png.cur;
    png.cur = t;
}

/* finish the file: the end of the block, the Adler-32, the last IDAT and the IEND chunk */
void PngEnd(void)
{
    uint32_t adler = png.adler2 << 16 | png.adler1;
    PngSymbol(256);
    if (png.nbits)
        PngBits(0, 8 - png.nbits);
    PngByte(adler >> 24);
    PngByte(adler >> 16);
    PngByte(adler >> 8);
    PngByte(adler);
    if (png.outlen)
        PngChunk(png.out, "IDAT", png.outlen);
    PngChunk(png.out, "IEND", 0);
}
#endif
