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

FRAMEBUFFER SPAN and AFFINE (docs/FRAMEBUFFER_AFFINE_SPAN_Design.md, 25 Nov 2026) are here too, on the same pieces.

    FRAMEBUFFER SPAN from, to, left%(), right%() [, step] [, first] [, fill] [, transparent] [, outside]
    FRAMEBUFFER AFFINE from, to, u%(), v%(), du%(), dv%() [, step] [, first] [, left] [, width] [, edge] [, transparent]

SPAN copies, on each row of band b, the columns left(b) to right(b) (or, with outside, the others); the pixels it does
not copy are left, or get the fill colour. AFFINE reads each row from the source along a straight line: column i of row
j of band b shows the source's pixel at (u - j*dv + i*du, v + j*du + i*dv) / 65536, rounded down, so one element
turns and scales a whole picture and an element a row makes a floor in perspective. Its edge and transparent are WARPH's.

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
    if (w <= 0)
        return;
    if (bpp == 8)
        memset(row + x, c, w);
    else if (bpp == 4) // whole bytes between an odd start and an odd end (SPAN fills most of a screen)
    {
        if (x & 1)
        {
            warp_put(row, x, 4, c);
            x++, w--;
        }
        memset(row + (x >> 1), c * 0x11, w >> 1);
        if (w & 1)
            warp_put(row, x + w - 1, 4, c);
    }
    else
        for (int i = 0; i < w; i++)
            ((uint16_t *)row)[x + i] = (uint16_t)c;
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

/* ---------------------------------------------------------------------------------------------------------------------
FRAMEBUFFER SPAN: band b is the step rows from first + b * step (those off the buffer are not drawn), and its span is
the columns lf[b] to rt[b], cut to the row and empty when lf > rt. The span's pixels (outside: the row's others) are
copied from the same place in the source, those of the transparent value excepted; the row's other pixels get the fill
colour, if there is one. The source may be the destination: nothing is copied then, and only the fill is drawn.
--------------------------------------------------------------------------------------------------------------------- */
typedef struct
{
    uint8_t *dst;
    const uint8_t *src;
    int w, h, stride, bpp;
    int n, step, first;       // the bands
    const int64_t *lf, *rt;   // each band's first and last column
    int fill, trans, outside; // -1 or a pixel value; -1 or a pixel value; 0 or 1
} span_t;

void span_run(const span_t *a)
{
    int w = a->w, bpp = a->bpp;
    for (int b = 0; b < a->n; b++)
    {
        int64_t y0 = a->first + (int64_t)b * a->step, y1 = y0 + a->step;
        if (y0 >= a->h)
            break;
        if (y1 <= 0)
            continue;
        if (y0 < 0)
            y0 = 0;
        if (y1 > a->h)
            y1 = a->h;
        int64_t l = a->lf[b], r = a->rt[b];
        int lo = l < 0 ? 0 : l > w ? w : (int)l, hi = r < 0 ? 0 : r >= w ? w : (int)r + 1; // the span is lo to hi-1
        if (hi < lo)
            hi = lo;
        for (int y = (int)y0; y < (int)y1; y++)
        {
            uint8_t *d = a->dst + y * a->stride;
            const uint8_t *s = a->src + y * a->stride;
            if (!a->outside)
            {
                if (s != d)
                    warp_seg(d, lo, s, lo, hi - lo, bpp, a->trans);
                if (a->fill >= 0)
                {
                    warp_fill(d, 0, lo, bpp, a->fill);
                    warp_fill(d, hi, w - hi, bpp, a->fill);
                }
            }
            else
            {
                if (s != d)
                {
                    warp_seg(d, 0, s, 0, lo, bpp, a->trans);
                    warp_seg(d, hi, s, hi, w - hi, bpp, a->trans);
                }
                if (a->fill >= 0)
                    warp_fill(d, lo, hi - lo, bpp, a->fill);
            }
        }
    }
}

/* ---------------------------------------------------------------------------------------------------------------------
FRAMEBUFFER AFFINE: band b is the step rows from first + b * step, and the window is the width columns from left; what
is off the buffer is not drawn. Column i of row j of the band shows the source's pixel (sx, sy):

    U = u[b] - j * dv[b] + i * du[b]      sx = U / 65536, rounded down
    V = v[b] + j * du[b] + i * dv[b]      sy = V / 65536, rounded down

With no edge (WARP_WRAP) sx and sy are taken round the source, and any value is allowed. With an edge a pixel whose
place is outside the source is left (WARP_LEAVE) or gets the edge colour, and a place beyond +-AFF_POS or a step beyond
+-AFF_STEP is taken as that limit, so that no sum here can overflow. A source pixel of the transparent value is not
written.
--------------------------------------------------------------------------------------------------------------------- */
#define AFF_POS ((int64_t)1 << 60)
#define AFF_STEP ((int64_t)1 << 35)
#define WARP_FAR (1 << 24) // first, left and width go no further than this either way

typedef struct
{
    uint8_t *dst;
    const uint8_t *src;
    int w, h, stride, bpp;
    int n, step, first, left, width; // the bands, and the window's first column and its width
    const int64_t *u, *v, *du, *dv;  // each band: the place its first pixel shows, and the step along a row
    int edge, trans;                 // as warp_t
} affine_t;

static int64_t aff_mod(int64_t a, int64_t m) // 0 to m-1
{
    a %= m;
    return a < 0 ? a + m : a;
}

static int64_t aff_floor(int64_t a, int64_t b) // a / b rounded down, for b > 0
{
    int64_t q = a / b;
    return a % b < 0 ? q - 1 : q;
}

static int64_t aff_clamp(int64_t a, int64_t m)
{
    return a > m ? m : a < -m ? -m : a;
}

// the columns *i0 to *i1-1 cut down to those i for which f + i * d is 0 to m-1 (a straight line: one run of them)
static void aff_inside(int64_t f, int64_t d, int64_t m, int64_t *i0, int64_t *i1)
{
    int64_t lo, hi;
    if (d == 0)
    {
        if (f < 0 || f >= m)
            *i1 = *i0;
        return;
    }
    if (d > 0)
        lo = -aff_floor(f, d), hi = aff_floor(m - 1 - f, d);
    else
        lo = -aff_floor(m - 1 - f, -d), hi = aff_floor(f, -d);
    if (lo > *i0)
        *i0 = lo;
    if (hi + 1 < *i1)
        *i1 = hi + 1;
}

typedef struct
{
    uint32_t du, dv, mw, mh; // the steps along a row, and the source's size: all in 1/65536 pixel
    const uint8_t *src;
    int stride, trans;
} aff_step_t;

/* cnt pixels of a destination row from pixel x: each is the source's pixel at (U >> 16, V >> 16), and U and V then
   step by du and dv round the source. All four are 0 to mw-1 or mh-1, so a step is an addition and one compare, and
   no pixel needs a division. Made once for each depth, each a function of its own: inlined into affine_run the loop
   had no registers left and read its steps from the stack for every pixel (5.5 ms a MODE 2 screen). */
WARP_INLINE void aff_row(uint8_t *d, int x, int cnt, uint32_t U, uint32_t V, const aff_step_t *s, int bpp)
{
    uint32_t du = s->du, dv = s->dv, mw = s->mw, mh = s->mh; // in locals, as in warp_v
    const uint8_t *src = s->src;
    int stride = s->stride, trans = s->trans;
#define AFF_NEXT(c)                                           \
    {                                                         \
        c = warp_get(src + (V >> 16) * stride, U >> 16, bpp); \
        if ((U += du) >= mw)                                  \
            U -= mw;                                          \
        if ((V += dv) >= mh)                                  \
            V -= mh;                                          \
    }
    int c, c2;
    if (bpp == 4 && trans < 0) // two pixels gathered for each whole byte of the destination
    {
        if (cnt && (x & 1))
        {
            AFF_NEXT(c);
            warp_put(d, x, 4, c);
            x++, cnt--;
        }
        uint8_t *q = d + (x >> 1);
        for (; cnt >= 2; cnt -= 2)
        {
            AFF_NEXT(c);
            AFF_NEXT(c2);
            *q++ = (uint8_t)(c | (c2 << 4));
        }
        if (cnt)
        {
            AFF_NEXT(c);
            *q = (uint8_t)((*q & 0xF0) | c);
        }
        return;
    }
    for (; cnt > 0; cnt--, x++)
    {
        AFF_NEXT(c);
        if (c != trans)
            warp_put(d, x, bpp, c);
    }
#undef AFF_NEXT
}

static __attribute__((noinline)) void aff_row4(uint8_t *d, int x, int cnt, uint32_t U, uint32_t V, const aff_step_t *s)
{
    aff_row(d, x, cnt, U, V, s, 4);
}

#if defined(HDMI) || defined(WARP_HOST_TEST) // (a VGA build has only the 4-bit modes)
static __attribute__((noinline)) void aff_row8(uint8_t *d, int x, int cnt, uint32_t U, uint32_t V, const aff_step_t *s)
{
    aff_row(d, x, cnt, U, V, s, 8);
}

static __attribute__((noinline)) void aff_row16(uint8_t *d, int x, int cnt, uint32_t U, uint32_t V, const aff_step_t *s)
{
    aff_row(d, x, cnt, U, V, s, 16);
}
#endif

void affine_run(const affine_t *a)
{
    int w = a->w, h = a->h, wrap = a->edge == WARP_WRAP;
    int64_t mw = (int64_t)w << 16, mh = (int64_t)h << 16;
    aff_step_t st = {0, 0, (uint32_t)mw, (uint32_t)mh, a->src, a->stride, a->trans};
    int64_t ia = a->left < 0 ? -(int64_t)a->left : 0, ib = a->width; // the window's columns that are on the buffer
    if (ib > (int64_t)w - a->left)
        ib = (int64_t)w - a->left;
    if (ia >= ib)
        return;
    for (int b = 0; b < a->n; b++)
    {
        int64_t y0 = a->first + (int64_t)b * a->step;
        if (y0 >= h)
            break;
        if (y0 + a->step <= 0)
            continue;
        int64_t u = a->u[b], v = a->v[b], du = a->du[b], dv = a->dv[b];
        int64_t dvu = dv, duv = du; // the steps down the band: dv as U takes it, du as V does
        if (wrap)
        {
            u = aff_mod(u, mw), v = aff_mod(v, mh);
            dvu = aff_mod(dv, mw), duv = aff_mod(du, mh);
        }
        else
        {
            u = aff_clamp(u, AFF_POS), v = aff_clamp(v, AFF_POS);
            dvu = dv = aff_clamp(dv, AFF_STEP), duv = du = aff_clamp(du, AFF_STEP);
        }
        // the steps along a row, round the source. With an edge these serve only for a run of two pixels or more
        // inside the source, whose step is then smaller than the source: the same step, a negative one made positive
        st.du = (uint32_t)aff_mod(du, mw), st.dv = (uint32_t)aff_mod(dv, mh);
        int j1 = y0 + a->step > h ? (int)(h - y0) : a->step;
        for (int j = y0 < 0 ? (int)-y0 : 0; j < j1; j++)
        {
            uint8_t *d = a->dst + (int)(y0 + j) * a->stride;
            int64_t U = u - j * dvu, V = v + j * duv, i0 = ia, i1 = ib; // the row's column 0
            if (wrap)
            {
                U = aff_mod(U + i0 * st.du, mw);
                V = aff_mod(V + i0 * st.dv, mh);
            }
            else
            {
                aff_inside(U, du, mw, &i0, &i1);
                aff_inside(V, dv, mh, &i0, &i1);
                if (i0 >= i1)
                    i0 = i1 = ia; // none of the row is in the source
                if (a->edge >= 0)
                {
                    warp_fill(d, a->left + (int)ia, (int)(i0 - ia), a->bpp, a->edge);
                    warp_fill(d, a->left + (int)i1, (int)(ib - i1), a->bpp, a->edge);
                }
                U += i0 * du, V += i0 * dv;
            }
            int x = a->left + (int)i0, cnt = (int)(i1 - i0);
#if defined(HDMI) || defined(WARP_HOST_TEST)
            if (a->bpp == 8)
                aff_row8(d, x, cnt, (uint32_t)U, (uint32_t)V, &st);
            else if (a->bpp == 16)
                aff_row16(d, x, cnt, (uint32_t)U, (uint32_t)V, &st);
            else
#endif
                aff_row4(d, x, cnt, (uint32_t)U, (uint32_t)V, &st);
        }
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

// the bits of a pixel in this mode; MODE 1 is refused
static int warp_depth(void)
{
    switch (DISPLAY_TYPE)
    {
    case SCREENMODE2:
    case SCREENMODE3:
        return 4;
#ifdef HDMI
    case SCREENMODE4:
        return 16;
    case SCREENMODE5:
        return 8;
#endif
    default:
        error("Not available in mode 1");
    }
    return 0;
}

// an edge or fill argument: -1 (WARP_LEAVE), or a colour as the pixel that BOX would draw with it
static int warp_edge(unsigned char *p, int bpp)
{
    int c = getColour((char *)p, 1);
    return c < 0 ? WARP_LEAVE : bpp == 4 ? RGB121(c) : bpp == 8 ? RGB332(c) : RGB555(c);
}

// a transparent argument, as FRAMEBUFFER MERGE takes it
static int warp_trans(unsigned char *p, int bpp)
{
    return bpp == 4 ? getint(p, 0, 15) : bpp == 8 ? getint(p, 0, 255) : RGB555(getColour((char *)p, 0));
}

#ifdef rp2350
typedef int warp_dims_t;
#else
typedef short warp_dims_t;
#endif

void cmd_span(unsigned char *p)
{
    getcsargs(&p, 17);
    warp_dims_t dims[MAXDIM];
    span_t a;
    int64_t *lf, *rt;
    if (argc < 7)
        SyntaxError();
    a.bpp = warp_depth();
    a.src = warp_buffer(argv[0]);
    a.dst = warp_buffer(argv[2]);
    a.w = HRes;
    a.h = VRes;
    a.stride = HRes * a.bpp / 8;
    a.n = parseintegerarray(argv[4], &lf, 3, 1, dims, false, NULL);
    int m = parseintegerarray(argv[6], &rt, 4, 1, dims, false, NULL);
    if (m != a.n)
        error("Right array has % elements, left array has %", m, a.n);
    a.lf = lf;
    a.rt = rt;
    a.step = (argc >= 9 && *argv[8]) ? getint(argv[8], 1, VRes) : 1;
    a.first = (argc >= 11 && *argv[10]) ? getint(argv[10], -WARP_FAR, WARP_FAR) : 0;
    a.fill = (argc >= 13 && *argv[12]) ? warp_edge(argv[12], a.bpp) : -1;
    a.trans = (argc >= 15 && *argv[14]) ? warp_trans(argv[14], a.bpp) : -1;
    a.outside = (argc >= 17 && *argv[16]) ? getint(argv[16], 0, 1) : 0;
    CursorHide(); // as cmd_warp
    CursorSuspend = true;
    span_run(&a);
    CursorSuspend = false;
}

void cmd_affine(unsigned char *p)
{
    getcsargs(&p, 23);
    warp_dims_t dims[MAXDIM];
    affine_t a;
    int64_t *q[4];
    if (argc < 11)
        SyntaxError();
    a.bpp = warp_depth();
    a.src = warp_buffer(argv[0]);
    a.dst = warp_buffer(argv[2]);
    if (a.src == a.dst)
        error("Same framebuffer");
    a.w = HRes;
    a.h = VRes;
    a.stride = HRes * a.bpp / 8;
    for (int i = 0; i < 4; i++) // u, v, du, dv
    {
        int m = parseintegerarray(argv[4 + 2 * i], &q[i], 3 + i, 1, dims, false, NULL);
        if (i == 0)
            a.n = m;
        else if (m != a.n)
            error("Array % has % elements, array 1 has %", i + 1, m, a.n);
    }
    a.u = q[0], a.v = q[1], a.du = q[2], a.dv = q[3];
    a.step = (argc >= 13 && *argv[12]) ? getint(argv[12], 1, VRes) : 1;
    a.first = (argc >= 15 && *argv[14]) ? getint(argv[14], -WARP_FAR, WARP_FAR) : 0;
    a.left = (argc >= 17 && *argv[16]) ? getint(argv[16], -WARP_FAR, WARP_FAR) : 0;
    a.width = (argc >= 19 && *argv[18]) ? getint(argv[18], 1, WARP_FAR) : HRes;
    a.edge = (argc >= 21 && *argv[20]) ? warp_edge(argv[20], a.bpp) : WARP_WRAP;
    a.trans = (argc >= 23 && *argv[22]) ? warp_trans(argv[22], a.bpp) : -1;
    CursorHide(); // as cmd_warp
    CursorSuspend = true;
    affine_run(&a);
    CursorSuspend = false;
}

void cmd_warp(unsigned char *p, int vertical)
{
    getcsargs(&p, 17);
    warp_dims_t dims[MAXDIM];
    warp_t a;
    int64_t *off, *rows = NULL, phase = 0;
    if (argc < 5 || (vertical && argc > 15))
        SyntaxError();
    a.bpp = warp_depth();
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
    a.edge = (argc >= 13 && *argv[12]) ? warp_edge(argv[12], a.bpp) : WARP_WRAP;
    a.trans = (argc >= 15 && *argv[14]) ? warp_trans(argv[14], a.bpp) : -1;
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
