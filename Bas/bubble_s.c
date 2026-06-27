/* bubble_s.c - single-precision variant of bubble.c.
 * Same recurrence but in float (SAdd/SMul/SSin + DtoS/StoD/StoI), to compare
 * speed against the double version. Build:
 *   python armcfgen.py bubble_s.c --compile -n bubblerow -e bubblerow -O s -I <firmware dir>
 * Drop-in: same CSUB signature - CSUB bubblerow INTEGER, INTEGER, FLOAT
 */
#include "PicoCFunctions.h"

long long bubblerow(long long *c, long long *d, double *pf)
{
    float iang = DtoS(pf[0]), b = DtoS(pf[1]), v = DtoS(pf[2]), x = DtoS(pf[3]), hp = DtoS(pf[4]);
    float xs = DtoS(pf[5]), ys = DtoS(pf[6]), xc = DtoS(pf[7]), yc = DtoS(pf[8]);
    int j;

    for (j = 0; j < 66; j++) {
        float A   = SAdd(iang, v);
        float siv = SSin(A);
        float civ = SSin(SAdd(A, hp));     /* cos(i+v) = sin(i+v + pi/2) */
        float sx  = SSin(x);
        float cx  = SSin(SAdd(x, hp));     /* cos(x) */
        float uu  = SAdd(siv, sx);
        v = SAdd(civ, cx);
        x = SAdd(uu, b);
        c[j] = StoI(SAdd(SMul(uu, xs), xc));
        d[j] = StoI(SAdd(SMul(v,  ys), yc));
    }
    pf[2] = StoD(v);                       /* keep persistent state as double */
    pf[3] = StoD(x);
    return 0;
}
