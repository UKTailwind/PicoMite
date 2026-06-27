/* bubble.c - "bubble universe" inner loop as a PicoMite CSUB.
 *
 * One frame row (66 points): the sin/cos recurrence + scale + offset, producing
 * integer screen coordinates in c() and d(). Moving this out of the BASIC
 * interpreter took the demo from 233 ms/frame to ~23 ms.
 *
 * Build (compute-heavy -> ask for -Os; -I points at PicoCFunctions.h):
 *     python armcfgen.py bubble.c --compile -n bubblerow -e bubblerow -O s -I <firmware dir>
 *
 * Call from BASIC:
 *     CSUB bubblerow INTEGER, INTEGER, FLOAT      ' c(), d(), pf()
 *     ...
 *     pf(0)=i : pf(1)=r*i+t
 *     bubblerow c(), d(), pf()
 *
 * pf(): 0=iang(i)  1=b(r*i+t)  2=v(state)  3=x(state)  4=pi/2
 *       5=xs  6=ys  7=xc  8=yc      (v,x persist in pf(2)/pf(3) across calls)
 *
 * Uses PicoCFunctions.h: all double maths goes through the firmware CallTable
 * (Sine/FAdd/FMul/FloatToInt), which the header locates at runtime via VTOR - so
 * no CallTable argument and no chip-/build-specific address, one blob for every
 * variant and both chips. (The per-call base re-read the wrappers do is lost in
 * the noise here - the software Sine calls dominate.)
 */
#include "PicoCFunctions.h"

long long bubblerow(long long *c, long long *d, double *pf)
{
    double iang = pf[0], b = pf[1], v = pf[2], x = pf[3], hp = pf[4];
    double xs = pf[5], ys = pf[6], xc = pf[7], yc = pf[8];
    int j;

    for (j = 0; j < 66; j++) {
        double A   = FAdd(iang, v);
        double siv = Sine(A);
        double civ = Sine(FAdd(A, hp));     /* cos(i+v) = sin(i+v + pi/2) */
        double sx  = Sine(x);
        double cx  = Sine(FAdd(x, hp));     /* cos(x) */
        double uu  = FAdd(siv, sx);
        v = FAdd(civ, cx);
        x = FAdd(uu, b);
        c[j] = FloatToInt(FAdd(FMul(uu, xs), xc));
        d[j] = FloatToInt(FAdd(FMul(v,  ys), yc));
    }
    pf[2] = v;
    pf[3] = x;
    return 0;
}
