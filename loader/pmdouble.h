/* pmdouble.h: the firmware's double-precision routines that a program uses in place of its own (the services'
   doubles: loader/pmimg.h). They are the Pico SDK's, accelerated by the RP2350's DCP, which the firmware has linked
   in for MMBasic itself: libgcc's and newlib's software doubles are several times slower on the Cortex-M33 (sin()
   three times). tools/sdk/fwdouble.S defines each name as a jump through the table, so a program's own copies are
   never linked. The SDK's routines save and restore the DCP's state if an interrupt finds it in use, so an
   interrupt's handler (a sound's fill) may use them too.

   Only the routines the firmware already has are listed, so the table costs it no code; __aeabi_drsub and
   __aeabi_f2d too, so that libgcc's object of __aeabi_dadd (which has them as well) is never wanted. The order is the
   table's and never changes: new ones go at the end. Plain #defines, for C and for the assembler. MIT License, as pmimg.h. */
#ifndef PMDOUBLE_H
#define PMDOUBLE_H

#define PMDOUBLE_LIST(X) \
    X(__aeabi_dadd)  X(__aeabi_dsub)  X(__aeabi_drsub) X(__aeabi_dmul)  X(__aeabi_ddiv) \
    X(__aeabi_dcmpeq) X(__aeabi_dcmplt) X(__aeabi_dcmple) X(__aeabi_dcmpge) X(__aeabi_dcmpgt) X(__aeabi_dcmpun) \
    X(__aeabi_i2d)   X(__aeabi_ui2d)  X(__aeabi_l2d)   X(__aeabi_ul2d) \
    X(__aeabi_d2iz)  X(__aeabi_d2uiz) X(__aeabi_d2lz)  X(__aeabi_d2ulz) X(__aeabi_d2f) X(__aeabi_f2d) \
    X(sqrt) X(sin) X(cos) X(tan) X(asin) X(atan) X(atan2) X(sinh) X(cosh) X(tanh) \
    X(exp) X(exp2) X(log) X(log10) X(pow) X(hypot) X(ldexp) X(fmod) X(floor) X(trunc) X(round) X(sincos)

#endif
