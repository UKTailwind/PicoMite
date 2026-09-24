# bench_micro.py - MicroPython twin of bench_micro.bas (Phase 0, 0c)
# Each test times N passes of a loop holding one statement, subtracts the
# empty loop, and prints: BENCH name us_per_stmt cycles_per_stmt
# Module-level tests mimic BASIC (globals, LOAD_NAME); the [def] tests run
# the same statements on locals inside a function (LOAD_FAST).
import gc, machine
from time import ticks_us, ticks_diff
from math import sin

N = 20000
K1 = 1.5
MHZ = machine.freq() / 1000000
arr = [0] * 1025
arr2 = [[0] * 33 for _ in range(33)]
t = "Hello world"; u = "abc"; fx = 1.25; b = 7; a = 0; s = 0; y = 0; fy = 0.0; st = ""
abcdefghijklmnopqrstuvwxyz1234 = 0
print("BENCHSTART", machine.freq())

def rep(name, us_total, base):
    us = (us_total - base) / N
    print("BENCH %s %.3f %d" % (name, us, round(us * MHZ)))

gc.collect(); t0 = ticks_us()
for i in range(N):
    pass
base = ticks_diff(ticks_us(), t0)
print("BENCH empty_for_loop %.3f %d" % (base / N, round(base / N * MHZ)))

gc.collect(); t0 = ticks_us()
for i in range(N):
    a = b
rep("a=b (1-char names)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    abcdefghijklmnopqrstuvwxyz1234 = abcdefghijklmnopqrstuvwxyz1234
rep("30-char name = 30-char name", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    a = a + 1
rep("a=a+1", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    s = (s + i) & 65535
rep("s=(s+i)&65535", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    fx = fx * 1.000001 + 0.5
rep("fx=fx*1.000001+0.5", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    fy = fx
rep("fy=fx (float)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    fy = 1.23456789
rep("fy=1.23456789 (literal)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    fy = K1
rep("fy=K1 (constant)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    y = arr[i & 1023]
rep("y=arr[i&1023]", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    arr[i & 1023] = y
rep("arr[i&1023]=y", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    y = arr2[i & 31][7]
rep("y=arr2[i&31][7]", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    fy = abs(fx)
rep("fy=abs(fx)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    fy = abs(abs(abs(abs(abs(fx)))))
rep("fy=abs nested 5 deep", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    fy = sin(fx)
rep("fy=sin(fx)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    st = str(i)
rep("st=str(i)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    st = t[:3]
rep("st=t[:3]", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    st = t + u
rep("st=t+u", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    a = 1 if i & 1 else 2
rep("a=1 if i&1 else 2", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    if i & 1:
        a = 1
    else:
        a = 2
rep("multi-line if/else (per pass)", ticks_diff(ticks_us(), t0), base)

def noargs():
    pass

def add2(p, q):
    return p + q

def sixparams(p1, p2, p3, p4, p5, p6):
    l1 = p1; l2 = 0; l3 = 0; l4 = 0

def localarrays():
    la = [0.0] * 3; lb = [0.0] * 3; lc = [0.0] * 3
    la[0] = 1

gc.collect(); t0 = ticks_us()
for i in range(N):
    noargs()
rep("call function with no arguments", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    y = add2(i, 3)
rep("y=add2(i,3)", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    sixparams(i, 2, 3, 4, 5, 6)
rep("call, 6 params + 4 locals", ticks_diff(ticks_us(), t0), base)

gc.collect(); t0 = ticks_us()
for i in range(N):
    localarrays()
rep("call creating 3 local lists", ticks_diff(ticks_us(), t0), base)


def insub():
    la = 0; lb = 7; lx = 1.25; ly = 0.0
    gc.collect(); lt = ticks_us()
    for li in range(N):
        pass
    lbase = ticks_diff(ticks_us(), lt)
    print("BENCH [def] empty_for_loop %.3f %d" % (lbase / N, round(lbase / N * MHZ)))
    gc.collect(); lt = ticks_us()
    for li in range(N):
        la = lb
    rep("[def] la=lb (locals)", ticks_diff(ticks_us(), lt), lbase)
    gc.collect(); lt = ticks_us()
    for li in range(N):
        la = b
    rep("[def] la=b (global read)", ticks_diff(ticks_us(), lt), lbase)
    gc.collect(); lt = ticks_us()
    for li in range(N):
        ly = K1
    rep("[def] ly=K1 (constant from def)", ticks_diff(ticks_us(), lt), lbase)
    gc.collect(); lt = ticks_us()
    for li in range(N):
        ly = lx * 1.000001 + 0.5
    rep("[def] ly=lx*1.000001+0.5", ticks_diff(ticks_us(), lt), lbase)
    gc.collect(); lt = ticks_us()
    for li in range(N):
        ly = sin(lx)
    rep("[def] ly=sin(lx)", ticks_diff(ticks_us(), lt), lbase)
    gc.collect(); lt = ticks_us()
    for li in range(N):
        la = arr[li & 1023]
    rep("[def] la=arr[li&1023] (global list)", ticks_diff(ticks_us(), lt), lbase)

insub()
print("BENCHEND")
