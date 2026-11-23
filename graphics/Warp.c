/***********************************************************************************************************************
FRAMEBUFFER WARPH and WARPV: each row (WARPH) or column (WARPV) of one framebuffer copied to another, moved by its own
offset from an integer array (docs/FRAMEBUFFER_WARP_Design.md, Peter's design of 23 Nov 2026). For racing roads,
parallax, wobble and column effects.

    FRAMEBUFFER WARPH from, to, xoff%() [, step] [, first] [, phase] [, edge] [, transparent] [, rows%()]
    FRAMEBUFFER WARPV from, to, yoff%() [, step] [, first] [, phase] [, edge] [, transparent]

Band b is the step rows (WARPH) or columns (WARPV) from first + b * step, the last cut at the buffer's edge, and it
takes element (b + phase) MOD n of the array (n elements, n bands). A destination pixel (x, y) is the source's (x - off, sy) in WARPH,
sy being y or from rows%(), and (x, y + off) in WARPV: positive moves right, and up, as SPRITE SCROLL does. With no
edge the source wraps round; edge -1 leaves the pixels nothing moves onto, and a colour fills them. A source pixel of
the transparent value is not written.

VGA and HDMI builds only, MODE 2, 3, 4 and 5 (MODE 1 is refused). The core below (WARP_HOST_TEST) is plain C so that
it can be checked on a PC against a pixel-by-pixel model of the design.
***********************************************************************************************************************/
#ifndef WARP_HOST_TEST
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "Memory.h"
#endif

#if defined(PICOMITEVGA) || defined(WARP_HOST_TEST)
#include <stdint.h>
#include <string.h>

#define WARP_WRAP (-2) // edge: wrap round (the default)
#define WARP_LEAVE (-1) // edge: leave the destination's pixel

typedef struct
{
    uint8_t *dst;
    const uint8_t *src;
    int w, h, stride, bpp; // the buffers' size in pixels, bytes a row, bits a pixel (4, 8 or 16)
    int vertical;          // WARPV
    int n, step, first;    // the bands
    const int *boff;       // each band's offset: 0 to size-1 when wrapping, else clamped to -size..size
    const int *brow;       // WARPH: each band's first source row, -1 to leave it; NULL for none
    int edge, trans;       // WARP_WRAP, WARP_LEAVE or a pixel value; -1 or a pixel value
} warp_t;

// the 4-bit pixels are RGB121, two to a byte, the left one in the low nibble. Always inlined: at -Os a call for
// each pixel cost more than the pixel (WARPV with step 1 took 37 ms in MODE 2)
#define WARP_INLINE static inline __attribute__((always_inline))
WARP_INLINE int warp_get(const uint8_t *row, int x, int bpp)
{
    if (bpp == 4)
        return (row[x >> 1] >> ((x & 1) << 2)) & 0xF;
    if (bpp == 8)
        return row[x];
    return ((const uint16_t *)row)[x];
}

WARP_INLINE void warp_put(uint8_t *row, int x, int bpp, int c)
{
    if (bpp == 4)
    {
        uint8_t *p = row + (x >> 1);
        *p = (x & 1) ? (uint8_t)((*p & 0x0F) | (c << 4)) : (uint8_t)((*p & 0xF0) | c);
    }
    else if (bpp == 8)
        row[x] = (uint8_t)c;
    else
        ((uint16_t *)row)[x] = (uint16_t)c;
}

// w pixels from pixel sx of a source row to pixel dx of a destination row, those of value t (-1: none) not written
static void warp_seg(uint8_t *d, int dx, const uint8_t *s, int sx, int w, int bpp, int t)
{
    if (w <= 0)
        return;
    if (bpp == 4)
    {
        int c;
        if (dx & 1) // an odd start: the destination's high nibble alone
        {
            if ((c = warp_get(s, sx, 4)) != t)
                warp_put(d, dx, 4, c);
            dx++, sx++, w--;
        }
        uint8_t *q = d + (dx >> 1); // dx is even now: whole destination bytes
        const uint8_t *r = s + (sx >> 1);
        int n = w >> 1, odd = sx & 1;
        if (t < 0 && !odd)
            memcpy(q, r, n);
        else if (t < 0) // an odd source start: each byte from two (its pixel is the high nibble)
            for (int i = 0; i < n; i++)
                q[i] = (uint8_t)((r[i] >> 4) | (r[i + 1] << 4));
        else
            for (int i = 0; i < n; i++)
            {
                // as above, with a transparent nibble masked out, so the byte is merged whole
                uint8_t v = odd ? (uint8_t)((r[i] >> 4) | (r[i + 1] << 4)) : r[i], m = 0xFF;
                if ((v & 0x0F) == t)
                    m = 0xF0;
                if ((v >> 4) == t)
                    m &= 0x0F;
                q[i] = (uint8_t)((q[i] & ~m) | (v & m));
            }
        if ((w & 1) && (c = warp_get(s, sx + w - 1, 4)) != t) // an odd end: the low nibble alone
            warp_put(d, dx + w - 1, 4, c);
        return;
    }
    if (t < 0)
        memcpy(d + dx * (bpp >> 3), s + sx * (bpp >> 3), w * (bpp >> 3));
    else if (bpp == 8)
    {
        const uint8_t *p = s + sx;
        uint8_t *q = d + dx;
        for (int i = 0; i < w; i++)
            if (p[i] != t)
                q[i] = p[i];
    }
    else
    {
        const uint16_t *p = (const uint16_t *)s + sx;
        uint16_t *q = (uint16_t *)d + dx;
        for (int i = 0; i < w; i++)
            if (p[i] != t)
                q[i] = p[i];
    }
}

/* WARPV: row by row from the top, so the destination is written down the screen. A band's pixels keep their x, so
   a 4-bit band moves whole bytes between its ends. Made once for each depth (bpp is a constant in each): a call or a
   memcpy for each band of each row cost more than its pixels (step 1 took 37 ms in MODE 2, the general code 14 ms at
   step 2) */
WARP_INLINE void warp_v(const warp_t *a, int bpp)
{
    // in locals: a store through a byte pointer could change *a, so the fields would be read again for every pixel
    const uint8_t *src = a->src;
    const int *boff = a->boff;
    int h = a->h, wd = a->w, stride = a->stride, n = a->n, step = a->step, edge = a->edge, trans = a->trans;
    if (bpp == 4 && edge == WARP_WRAP && trans < 0 && !((a->first | step) & 1))
    {
        // the usual 4-bit wave: an even first column and step, so each band is whole bytes of one source row
        for (int y = 0; y < h; y++)
        {
            uint8_t *d = a->dst + y * stride;
            for (int b = 0, x = a->first; b < n; b++, x += step)
            {
                int sy = y + boff[b], k = x >> 1, e = (x + step > wd ? wd : x + step) >> 1;
                const uint8_t *s = src + (sy >= h ? sy - h : sy) * stride;
                for (; k < e; k++)
                    d[k] = s[k];
            }
        }
        return;
    }
    for (int y = 0; y < h; y++)
    {
        uint8_t *d = a->dst + y * stride;
        for (int b = 0, x = a->first; b < n; b++, x += step)
        {
            int e = x + step > wd ? wd : x + step, sy = y + boff[b], i = x, c;
            if (edge == WARP_WRAP)
            {
                if (sy >= h)
                    sy -= h;
            }
            else if (sy < 0 || sy >= h)
            {
                if (edge >= 0)
                    for (; i < e; i++)
                        warp_put(d, i, bpp, edge);
                continue;
            }
            const uint8_t *s = src + sy * stride;
            if (bpp == 4 && trans < 0)
            {
                if (i & 1) // an odd start: the high nibble alone
                {
                    warp_put(d, i, 4, warp_get(s, i, 4));
                    i++;
                }
                for (; i + 1 < e; i += 2)
                    d[i >> 1] = s[i >> 1];
                if (i < e) // an odd end: the low nibble alone
                    warp_put(d, i, 4, warp_get(s, i, 4));
            }
            else
                for (; i < e; i++)
                    if ((c = warp_get(s, i, bpp)) != trans)
                        warp_put(d, i, bpp, c);
        }
    }
}

/* WARPV with step 1, a column a band, where warp_v's band overhead is paid for each pixel. In the 4-bit wrapping
   copy (the usual wave) each destination byte is made from two columns at once */
WARP_INLINE void warp_v1(const warp_t *a, int bpp)
{
    const uint8_t *src = a->src; // (in locals, as in warp_v)
    const int *boff = a->boff;
    int h = a->h, stride = a->stride, n = a->n, edge = a->edge, trans = a->trans;
    for (int y = 0; y < h; y++)
    {
        uint8_t *d = a->dst + y * stride;
        int b = 0, x = a->first;
        if (bpp == 4 && edge == WARP_WRAP && trans < 0)
        {
            if (x & 1) // an odd first column: the high nibble alone
            {
                int sy = y + boff[b++];
                warp_put(d, x, 4, warp_get(src + (sy >= h ? sy - h : sy) * stride, x, 4));
                x++;
            }
            for (; b + 1 < n; b += 2, x += 2)
            {
                int s0 = y + boff[b], s1 = y + boff[b + 1];
                if (s0 >= h)
                    s0 -= h;
                if (s1 >= h)
                    s1 -= h;
                d[x >> 1] = (uint8_t)((src[s0 * stride + (x >> 1)] & 0x0F) | (src[s1 * stride + (x >> 1)] & 0xF0));
            }
        }
        for (; b < n; b++, x++) // the general case, and a 4-bit odd last column
        {
            int sy = y + boff[b], c;
            if (edge == WARP_WRAP)
            {
                if (sy >= h)
                    sy -= h;
            }
            else if (sy < 0 || sy >= h)
            {
                if (edge >= 0)
                    warp_put(d, x, bpp, edge);
                continue;
            }
            if ((c = warp_get(src + sy * stride, x, bpp)) != trans)
                warp_put(d, x, bpp, c);
        }
    }
}

static void warp_fill(uint8_t *row, int x, int w, int bpp, int c)
{
    if (bpp == 8 && w > 0)
        memset(row + x, c, w);
    else
        for (int i = 0; i < w; i++)
            warp_put(row, x + i, bpp, c);
}

// one row of WARPH: the destination's pixel x is the source's x - o
static void warp_row(const warp_t *a, uint8_t *d, const uint8_t *s, int o)
{
    int w = a->w;
    if (a->edge == WARP_WRAP) // o is 0 to w-1
    {
        warp_seg(d, o, s, 0, w - o, a->bpp, a->trans);
        warp_seg(d, 0, s, w - o, o, a->bpp, a->trans);
    }
    else if (o >= 0) // o is -w to w
    {
        warp_seg(d, o, s, 0, w - o, a->bpp, a->trans);
        if (a->edge >= 0)
            warp_fill(d, 0, o, a->bpp, a->edge);
    }
    else
    {
        warp_seg(d, 0, s, -o, w + o, a->bpp, a->trans);
        if (a->edge >= 0)
            warp_fill(d, w + o, -o, a->bpp, a->edge);
    }
}

void warp_run(const warp_t *a)
{
    if (!a->vertical)
    {
        for (int b = 0; b < a->n; b++)
        {
            int y0 = a->first + b * a->step, y1 = y0 + a->step, r = a->brow ? a->brow[b] : 0;
            if (a->brow && r < 0)
                continue; // rows%() -1: the band is left as it is
            if (y1 > a->h)
                y1 = a->h;
            for (int y = y0; y < y1; y++)
            {
                int sy = y;
                if (a->brow && (sy = r + y - y0) >= a->h) // the band's next source rows, round to the top
                    sy -= a->h;
                warp_row(a, a->dst + y * a->stride, a->src + sy * a->stride, a->boff[b]);
            }
        }
        return;
    }
    if (a->step == 1)
    {
        if (a->bpp == 4)
            warp_v1(a, 4);
        else if (a->bpp == 8)
            warp_v1(a, 8);
        else
            warp_v1(a, 16);
    }
    else if (a->bpp == 4)
        warp_v(a, 4);
    else if (a->bpp == 8)
        warp_v(a, 8);
    else
        warp_v(a, 16);
}

/* the band offsets and rows from the arrays: band b takes element (b + phase) MOD n; an offset is reduced to 0 to
   size-1 when wrapping and clamped to -size..size otherwise (anything further moves the whole band off) */
void warp_bands(int *boff, int *brow, const int64_t *off, const int64_t *rows, int n, int64_t phase, int size,
                int wrap)
{
    int p = (int)(phase % n);
    if (p < 0)
        p += n;
    for (int b = 0, e = p; b < n; b++, e = (e + 1 == n ? 0 : e + 1))
    {
        int64_t o = off[e];
        if (wrap)
        {
            o %= size;
            if (o < 0)
                o += size;
        }
        else if (o > size)
            o = size;
        else if (o < -size)
            o = -size;
        boff[b] = (int)o;
        if (brow)
            brow[b] = (int)rows[e];
    }
}

#ifndef WARP_HOST_TEST
// a framebuffer letter as FRAMEBUFFER COPY takes it, refused if it was not created
static uint8_t *warp_buffer(unsigned char *p)
{
    if (checkstring(p, (unsigned char *)"N"))
        return DisplayBuf;
    if (checkstring(p, (unsigned char *)"F"))
    {
        if (FrameBuf == DisplayBuf)
            StandardError(38);
        return FrameBuf;
    }
    if (checkstring(p, (unsigned char *)"L"))
    {
        if (LayerBuf == DisplayBuf)
            error("Layer not created");
        return LayerBuf;
    }
#ifdef rp2350
    if (checkstring(p, (unsigned char *)"2"))
    {
        if (SecondFrame == DisplayBuf)
            error("Frame buffer 2 not created");
        return SecondFrame;
    }
    if (checkstring(p, (unsigned char *)"T"))
    {
        if (SecondLayer == DisplayBuf)
            error("Layer Top not created");
        return SecondLayer;
    }
#endif
    SyntaxError();
    return NULL;
}

void cmd_warp(unsigned char *p, int vertical)
{
    getcsargs(&p, 17);
#ifdef rp2350
    int dims[MAXDIM];
#else
    short dims[MAXDIM];
#endif
    warp_t a;
    int64_t *off, *rows = NULL, phase = 0;
    if (argc < 5 || (vertical && argc > 15))
        SyntaxError();
    switch (DISPLAY_TYPE)
    {
    case SCREENMODE2:
    case SCREENMODE3:
        a.bpp = 4;
        break;
#ifdef HDMI
    case SCREENMODE4:
        a.bpp = 16;
        break;
    case SCREENMODE5:
        a.bpp = 8;
        break;
#endif
    default:
        error("Not available in mode 1");
    }
    a.src = warp_buffer(argv[0]);
    a.dst = warp_buffer(argv[2]);
    if (a.src == a.dst)
        error("Same framebuffer");
    a.w = HRes;
    a.h = VRes;
    a.stride = HRes * a.bpp / 8;
    a.vertical = vertical;
    int bands = vertical ? HRes : VRes, size = vertical ? VRes : HRes; // the bands' dimension, the offsets'
    a.n = parseintegerarray(argv[4], &off, 3, 1, dims, false, NULL);
    a.step = (argc >= 7 && *argv[6]) ? getint(argv[6], 1, bands) : 1;
    a.first = (argc >= 9 && *argv[8]) ? getint(argv[8], 0, bands - 1) : 0;
    if (argc >= 11 && *argv[10])
        phase = getinteger(argv[10]);
    a.edge = WARP_WRAP;
    if (argc >= 13 && *argv[12])
    {
        int c = getColour((char *)argv[12], 1);
        a.edge = c < 0 ? WARP_LEAVE : a.bpp == 4 ? RGB121(c) : a.bpp == 8 ? RGB332(c) : RGB555(c);
    }
    a.trans = -1;
    if (argc >= 15 && *argv[14]) // as FRAMEBUFFER MERGE takes it
        a.trans = a.bpp == 4 ? getint(argv[14], 0, 15) : a.bpp == 8 ? getint(argv[14], 0, 255) : RGB555(getColour((char *)argv[14], 0));
    if ((int64_t)(a.n - 1) * a.step + a.first >= bands) // every element must start on the buffer
        error(vertical ? "Array has % elements, step % from column % fits %" : "Array has % elements, step % from row % fits %",
              a.n, a.step, a.first, (bands - a.first + a.step - 1) / a.step);
    if (argc >= 17 && *argv[16])
    {
        int m = parseintegerarray(argv[16], &rows, 9, 1, dims, false, NULL);
        if (m != a.n)
            error("Row array has % elements, offset array has %", m, a.n);
        for (int i = 0; i < m; i++)
            if (rows[i] < -1 || rows[i] >= VRes)
                error("Row array element % is ~: rows are -1 to %", i + g_OptionBase, rows[i], VRes - 1);
    }
    int *boff = (int *)GetTempMemory(a.n * sizeof(int) * (rows ? 2 : 1));
    a.boff = boff;
    a.brow = rows ? boff + a.n : NULL;
    warp_bands(boff, rows ? boff + a.n : NULL, off, rows, a.n, phase, size, a.edge == WARP_WRAP);
    // the GUI cursor off the buffers first, and its refresh held off until the picture is whole (as MERGE)
    CursorHide();
    CursorSuspend = true;
    warp_run(&a);
    CursorSuspend = false;
}
#endif
#endif
