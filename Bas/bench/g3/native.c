/* g3 prototype: the four kernels as plain C, the native floor for the VM.

   NATIVE which%, ia%(), fa!(), arrd%(), res%()

   The same arithmetic as the VM: doubles and 64-bit multiply/modulo go
   through the firmware's CallTable routines.  No bounds checks, no
   statement boundaries, locals in registers: what mmb2csub would give. */
#include "PicoCFunctions.h"

typedef MMFLOAT (*ff2_t)(MMFLOAT, MMFLOAT);
typedef int (*fcmp_t)(MMFLOAT, MMFLOAT);
typedef long long (*ll2_t)(long long, long long);
typedef MMFLOAT (*i2f_t)(long long);

static void findleap(MMFLOAT jday, MMFLOAT *leapsecond, MMFLOAT *jdleap, MMFLOAT *leapsec, fcmp_t fcmp)
{
    long long i;
    if (fcmp(jday, jdleap[1]) <= 0)
    {
        *leapsecond = leapsec[1];
        return;
    }
    if (fcmp(jday, jdleap[28]) >= 0)
    {
        *leapsecond = leapsec[28];
        return;
    }
    for (i = 1; i <= 27; i++)
        if ((fcmp(jday, jdleap[i]) >= 0) & (fcmp(jday, jdleap[i + 1]) < 0))
        {
            *leapsecond = leapsec[i];
            return;
        }
}

void main(long long *which, long long *ia, MMFLOAT *fa, long long *arrd, long long *res)
{
    unsigned int base = BaseAddress;
    ff2_t fadd = (ff2_t)(*(unsigned int *)(base + 0xA4));
    ff2_t fsub = (ff2_t)(*(unsigned int *)(base + 0xA8));
    ff2_t fmul = (ff2_t)(*(unsigned int *)(base + 0xA0));
    fcmp_t fcmp = (fcmp_t)(*(unsigned int *)(base + 0xB0));
    i2f_t i2f = (i2f_t)(*(unsigned int *)(base + 0x84));
    ll2_t lmod = (ll2_t)(*(unsigned int *)(base + 0x148));
#ifndef __ARM_ARCH_8M_MAIN__
    ll2_t lmul = (ll2_t)(*(unsigned int *)(base + 0x140));
#define MUL(x, y) lmul(x, y)
#else
#define MUL(x, y) ((x) * (y))
#endif
    if (*which == 1)
    { // integer loop
        long long n = ia[0], s = 0, i;
        for (i = 1; i <= n; i++)
        {
            s = s + MUL(i & 7, 3);
            if (s > 1000000)
                s = s - 1000000;
        }
        res[0] = s;
    }
    else if (*which == 2)
    { // insertion sort
        long long *a = (long long *)(unsigned int)arrd[0], n = ia[0], i, j, v;
        for (i = 1; i <= n - 1; i++)
        {
            v = a[i];
            j = i - 1;
            while (j >= 0)
            {
                if (a[j] <= v)
                    break;
                a[j + 1] = a[j];
                j--;
            }
            a[j + 1] = v;
        }
    }
    else if (*which == 3)
    { // julia, without the PIXEL
        MMFLOAT w = fa[0], h = fa[1], xd = fa[2], yd = fa[3], rOfs = fa[4], iOfs = fa[5], cRe = fa[6], cIm = fa[7], mit = fa[8];
        MMFLOAT X, Y, CX, CY, Zr, Zi, COUNT, nZr, nZi, ck = 0, limX = fsub(w, 1.0), limY = fsub(h, 1.0);
        for (X = 0; fcmp(X, limX) <= 0; X = fadd(X, 1.0))
        {
            CX = fadd(fmul(X, xd), rOfs);
            for (Y = 0; fcmp(Y, limY) <= 0; Y = fadd(Y, 1.0))
            {
                CY = fadd(fmul(Y, yd), iOfs);
                Zr = CX;
                Zi = CY;
                COUNT = 0;
                while ((fcmp(COUNT, mit) <= 0) & (fcmp(fadd(fmul(Zr, Zr), fmul(Zi, Zi)), 4.0) < 0))
                {
                    nZr = fadd(fsub(fmul(Zr, Zr), fmul(Zi, Zi)), cRe);
                    nZi = fadd(fmul(fmul(2.0, Zr), Zi), cIm);
                    Zr = nZr;
                    Zi = nZi;
                    COUNT = fadd(COUNT, 1.0);
                }
                ck = fadd(ck, COUNT);
            }
        }
        *(MMFLOAT *)&res[0] = ck;
    }
    else if (*which == 4)
    { // findleap called in a loop
        MMFLOAT *jdleap = (MMFLOAT *)(unsigned int)arrd[3], *leapsec = (MMFLOAT *)(unsigned int)arrd[6];
        MMFLOAT tot = 0, jd, ls = 0;
        long long n = ia[0], k;
        for (k = 1; k <= n; k++)
        {
            jd = i2f(2441000 + lmod(k, 17000));
            findleap(jd, &ls, jdleap, leapsec, fcmp);
            tot = fadd(tot, ls);
        }
        *(MMFLOAT *)&res[0] = tot;
    }
}
