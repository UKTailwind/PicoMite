/* g3 prototype (Route B, gate G3): a typed wordcode VM, run as a CSUB.

   VMRUN entry%, code%(), slots%(), arrd%(), fk!(), sys%(), res%()

   It stands in for what a firmware VM would do, so the gate measures the
   design and not a toy:
   - 16-bit wordcode: opcode in the low byte, a small operand in the high
     byte, extension words after it.  Dispatched by computed goto.
   - A stack of 64-bit cells; typed int and float operations.  Doubles and
     64-bit multiply/modulo go through the firmware's CallTable routines, as
     a firmware VM compiled for the M0+ would call __aeabi_*.
   - Globals bound once, by data address (slots%()); arrays by descriptor
     {base, lower bound, upper bound} with a bounds check on every access.
   - Locals in a frame; BYREF parameters held as pointers.
   - STMT at every statement boundary does what the interpreter's tail does
     now: stores the current line, writes the trace ring, tests the temp
     memory flag and the interrupt word, and reads the hardware timer for the
     100 us housekeeping cadence.
   - CALL writes, and RET clears, a 40-byte record per local in a stand-in
     variable table, for the cost of making g_vartbl locals.

   sys%(): 0 timer register address, 1 interrupt word address, 2 current
   line address, 3 trace ring (128 words), 4 variable table stand-in,
   5 temp memory flag address.
   res%(): 0 status (0 = END, 1 = index out of bounds, 2 = interrupt,
   3 = bad opcode), 1 statements executed. */
#include "PicoCFunctions.h"

typedef union
{
    long long i;
    MMFLOAT f;
    void *p;
} cell;
typedef MMFLOAT (*ff2_t)(MMFLOAT, MMFLOAT);
typedef int (*fcmp_t)(MMFLOAT, MMFLOAT);
typedef long long (*ll2_t)(long long, long long);
typedef MMFLOAT (*i2f_t)(long long);

enum
{
    OP_END, OP_STMT, OP_JMP, OP_JZ, OP_JNZ,
    OP_LDL, OP_STL, OP_LAD, OP_LDP, OP_STP, OP_LDG, OP_STG,
    OP_LCI, OP_LCW, OP_LCF,
    OP_ADDI, OP_SUBI, OP_MULI, OP_ANDI, OP_MODI,
    OP_ADDF, OP_SUBF, OP_MULF, OP_DIVF, OP_CVIF,
    OP_LTI, OP_LEI, OP_GTI, OP_GEI, OP_EQI,
    OP_LTF, OP_LEF, OP_GTF, OP_GEF,
    OP_ALD, OP_AST, OP_FNXI, OP_FNXF, OP_CALL, OP_RET,
    OP_COUNT
};

#define LMAX 256 // local cells
#define FMAX 32  // call depth
#define SMAX 32  // evaluation stack

void main(long long *entry, long long *code, long long *slots, long long *arrd, MMFLOAT *fk, long long *sys, long long *res)
{
    // label offsets from L_END: link-time constants, so no relocation in the blob
    static const short offs[OP_COUNT] = {
        0, &&L_STMT - &&L_END, &&L_JMP - &&L_END, &&L_JZ - &&L_END, &&L_JNZ - &&L_END,
        &&L_LDL - &&L_END, &&L_STL - &&L_END, &&L_LAD - &&L_END, &&L_LDP - &&L_END, &&L_STP - &&L_END,
        &&L_LDG - &&L_END, &&L_STG - &&L_END,
        &&L_LCI - &&L_END, &&L_LCW - &&L_END, &&L_LCF - &&L_END,
        &&L_ADDI - &&L_END, &&L_SUBI - &&L_END, &&L_MULI - &&L_END, &&L_ANDI - &&L_END, &&L_MODI - &&L_END,
        &&L_ADDF - &&L_END, &&L_SUBF - &&L_END, &&L_MULF - &&L_END, &&L_DIVF - &&L_END, &&L_CVIF - &&L_END,
        &&L_LTI - &&L_END, &&L_LEI - &&L_END, &&L_GTI - &&L_END, &&L_GEI - &&L_END, &&L_EQI - &&L_END,
        &&L_LTF - &&L_END, &&L_LEF - &&L_END, &&L_GTF - &&L_END, &&L_GEF - &&L_END,
        &&L_ALD - &&L_END, &&L_AST - &&L_END, &&L_FNXI - &&L_END, &&L_FNXF - &&L_END,
        &&L_CALL - &&L_END, &&L_RET - &&L_END};
    unsigned int base = BaseAddress;
    ff2_t fadd = (ff2_t)(*(unsigned int *)(base + 0xA4));
    ff2_t fsub = (ff2_t)(*(unsigned int *)(base + 0xA8));
    ff2_t fmul = (ff2_t)(*(unsigned int *)(base + 0xA0));
    ff2_t fdiv = (ff2_t)(*(unsigned int *)(base + 0xAC));
    fcmp_t fcmp = (fcmp_t)(*(unsigned int *)(base + 0xB0));
    i2f_t i2f = (i2f_t)(*(unsigned int *)(base + 0x84));
    ll2_t lmod = (ll2_t)(*(unsigned int *)(base + 0x148));
#ifndef __ARM_ARCH_8M_MAIN__
    ll2_t lmul = (ll2_t)(*(unsigned int *)(base + 0x140));
#endif
    volatile unsigned int *timer = (volatile unsigned int *)(unsigned int)sys[0];
    volatile unsigned int *intready = (volatile unsigned int *)(unsigned int)sys[1];
    volatile unsigned int *curline = (volatile unsigned int *)(unsigned int)sys[2];
    volatile unsigned int *trace = (volatile unsigned int *)(unsigned int)sys[3];
    unsigned int *vartbl = (unsigned int *)(unsigned int)sys[4];
    volatile unsigned char *tempflag = (volatile unsigned char *)(unsigned int)sys[5];
    unsigned short *code16 = (unsigned short *)code;
    unsigned short *pc = code16 + *entry;
    cell lstack[LMAX], stack[SMAX], *sp = stack, *lp = lstack, *lptop = lstack + 8;
    struct
    {
        unsigned short *ret;
        cell *lp;
        unsigned int nloc;
    } fr[FMAX];
    int fd = 0, tix = 0;
    unsigned int w, a, last = *timer, nstmt = 0;
    long long t;
    for (a = 0; a < 8; a++)
        lstack[a].i = 0;

#define NEXT                                       \
    do                                             \
    {                                              \
        w = *pc++;                                 \
        a = w >> 8;                                \
        goto *(&&L_END + offs[w & 0xff]);          \
    } while (0)
#define W16 (*pc++)
#define S16 ((short)*pc++)
    NEXT;

L_END:
    res[0] = 0;
    res[1] = nstmt;
    return;
L_STMT: // a statement boundary, as the interpreter's per-statement tail
    *curline = a;
    trace[tix] = (unsigned int)pc;
    tix = (tix + 1) & 127;
    nstmt++;
    if (*tempflag)
        *tempflag = 0; // (ClearTempMemory)
    if (*intready)
    {
        res[0] = 2;
        res[1] = nstmt;
        return;
    }
    if (*timer - last >= 100)
        last = *timer; // (routinechecks' 100 us cadence)
    NEXT;
L_JMP: // offsets count from the opcode word
    t = S16;
    pc += t - 2;
    NEXT;
L_JZ:
    t = S16;
    if ((--sp)->i == 0)
        pc += t - 2;
    NEXT;
L_JNZ:
    t = S16;
    if ((--sp)->i != 0)
        pc += t - 2;
    NEXT;
L_LDL:
    *sp++ = lp[a];
    NEXT;
L_STL:
    lp[a] = *--sp;
    NEXT;
L_LAD:
    (sp++)->p = &lp[a];
    NEXT;
L_LDP:
    *sp++ = *(cell *)lp[a].p;
    NEXT;
L_STP:
    *(cell *)lp[a].p = *--sp;
    NEXT;
L_LDG:
    *sp++ = *(cell *)(unsigned int)slots[a];
    NEXT;
L_STG:
    *(cell *)(unsigned int)slots[a] = *--sp;
    NEXT;
L_LCI:
    (sp++)->i = (signed char)a;
    NEXT;
L_LCW:
    t = W16;
    t |= (long long)(short)W16 << 16;
    (sp++)->i = t;
    NEXT;
L_LCF:
    (sp++)->f = fk[a];
    NEXT;
L_ADDI:
    sp--;
    sp[-1].i += sp[0].i;
    NEXT;
L_SUBI:
    sp--;
    sp[-1].i -= sp[0].i;
    NEXT;
L_MULI:
    sp--;
#ifdef __ARM_ARCH_8M_MAIN__
    sp[-1].i *= sp[0].i;
#else
    sp[-1].i = lmul(sp[-1].i, sp[0].i);
#endif
    NEXT;
L_ANDI:
    sp--;
    sp[-1].i &= sp[0].i;
    NEXT;
L_MODI:
    sp--;
    sp[-1].i = lmod(sp[-1].i, sp[0].i);
    NEXT;
L_ADDF:
    sp--;
    sp[-1].f = fadd(sp[-1].f, sp[0].f);
    NEXT;
L_SUBF:
    sp--;
    sp[-1].f = fsub(sp[-1].f, sp[0].f);
    NEXT;
L_MULF:
    sp--;
    sp[-1].f = fmul(sp[-1].f, sp[0].f);
    NEXT;
L_DIVF:
    sp--;
    sp[-1].f = fdiv(sp[-1].f, sp[0].f);
    NEXT;
L_CVIF:
    sp[-1].f = i2f(sp[-1].i);
    NEXT;
L_LTI:
    sp--;
    sp[-1].i = sp[-1].i < sp[0].i;
    NEXT;
L_LEI:
    sp--;
    sp[-1].i = sp[-1].i <= sp[0].i;
    NEXT;
L_GTI:
    sp--;
    sp[-1].i = sp[-1].i > sp[0].i;
    NEXT;
L_GEI:
    sp--;
    sp[-1].i = sp[-1].i >= sp[0].i;
    NEXT;
L_EQI:
    sp--;
    sp[-1].i = sp[-1].i == sp[0].i;
    NEXT;
L_LTF:
    sp--;
    sp[-1].i = fcmp(sp[-1].f, sp[0].f) < 0;
    NEXT;
L_LEF:
    sp--;
    sp[-1].i = fcmp(sp[-1].f, sp[0].f) <= 0;
    NEXT;
L_GTF:
    sp--;
    sp[-1].i = fcmp(sp[-1].f, sp[0].f) > 0;
    NEXT;
L_GEF:
    sp--;
    sp[-1].i = fcmp(sp[-1].f, sp[0].f) >= 0;
    NEXT;
L_ALD: // push arr_a(index)
{
    long long *d = arrd + 3 * a, ix = sp[-1].i;
    if (ix < d[1] || ix > d[2])
        goto L_BOUNDS;
    sp[-1] = ((cell *)(unsigned int)d[0])[ix - d[1]];
    NEXT;
}
L_AST: // arr_a(index) = value
{
    long long *d = arrd + 3 * a, ix;
    sp -= 2;
    ix = sp[0].i;
    if (ix < d[1] || ix > d[2])
        goto L_BOUNDS;
    ((cell *)(unsigned int)d[0])[ix - d[1]] = sp[1];
    NEXT;
}
L_FNXI: // integer FOR/NEXT, step 1: lp[a] += 1; loop while lp[a] <= lp[lim]
{
    unsigned int lim = W16;
    t = S16;
    if (++lp[a].i <= lp[lim].i)
        pc += t - 3;
    NEXT;
}
L_FNXF: // float FOR/NEXT, step 1
{
    unsigned int lim = W16;
    t = S16;
    lp[a].f = fadd(lp[a].f, 1.0);
    if (fcmp(lp[a].f, lp[lim].f) <= 0)
        pc += t - 3;
    NEXT;
}
L_CALL: // CALL nargs, target, nlocals: arguments move from the stack into the new frame
{
    unsigned int target = W16, nloc = W16, i;
    cell *nlp = lptop;
    unsigned int *v;
    if (fd >= FMAX || lptop + nloc > lstack + LMAX)
        goto L_BAD;
    sp -= a;
    for (i = 0; i < a; i++)
        nlp[i] = sp[i];
    for (; i < nloc; i++)
        nlp[i].i = 0;
    v = vartbl + (nlp - lstack) * 10; // a g_vartbl record per local, as creating the locals would
    for (i = 0; i < nloc; i++, v += 10)
    {
        v[0] = 0x41414141;
        v[1] = 0x41414141;
        v[2] = 0;
        v[3] = 0;
        v[4] = 0;
        v[5] = 0;
        v[6] = 0;
        v[7] = 0;
        v[8] = fd + 1;
        v[9] = (unsigned int)&nlp[i];
    }
    fr[fd].ret = pc;
    fr[fd].lp = lp;
    fr[fd].nloc = nloc;
    fd++;
    lp = nlp;
    lptop = nlp + nloc;
    pc = code16 + target;
    NEXT;
}
L_RET:
{
    unsigned int i, *v;
    if (fd == 0)
        goto L_END;
    fd--;
    v = vartbl + (lp - lstack) * 10; // and freeing them clears those records
    for (i = 0; i < fr[fd].nloc * 10; i++)
        v[i] = 0;
    lptop = lp;
    lp = fr[fd].lp;
    pc = fr[fd].ret;
    NEXT;
}
L_BOUNDS:
    res[0] = 1;
    res[1] = nstmt;
    return;
L_BAD:
    res[0] = 3;
    res[1] = nstmt;
    return;
}
