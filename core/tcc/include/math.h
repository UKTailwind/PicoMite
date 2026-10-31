/* PicoMite TinyCC: <math.h>.  The firmware's own maths: double precision in
   software (the RP2350 double coprocessor where it helps), float on the FPU. */
#ifndef _MATH_H
#define _MATH_H

#define M_E 2.7182818284590452354
#define M_LOG2E 1.4426950408889634074
#define M_LOG10E 0.43429448190325182765
#define M_LN2 0.69314718055994530942
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_1_PI 0.31830988618379067154
#define M_2_PI 0.63661977236758134308
#define M_SQRT2 1.41421356237309504880
#define M_SQRT1_2 0.70710678118654752440

#define INFINITY __inf__
#define NAN __nan__
#define HUGE_VAL ((double)__inf__)
#define HUGE_VALF __inf__

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double exp(double x);
double exp2(double x);
double log(double x);
double log2(double x);
double log10(double x);
double pow(double x, double y);
double sqrt(double x);
double cbrt(double x);
double hypot(double x, double y);
double fabs(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double trunc(double x);
double fmod(double x, double y);
double ldexp(double x, int e);
double frexp(double x, int *e);
double modf(double x, double *ip);

float sinf(float x);
float cosf(float x);
float tanf(float x);
float atan2f(float y, float x);
float sqrtf(float x);
float powf(float x, float y);
float expf(float x);
float logf(float x);
float fabsf(float x);
float floorf(float x);
float ceilf(float x);

#endif /* _MATH_H */
