"""steprun18.py PORT - the stepper subsystem across RUN (bug report item 18).
RP2350, with audio disabled (OPTION AUDIO DISABLE on the PC3): one axis on
GP10/GP22, no motor.

STEPPER INIT puts the planner's block buffer on the MMBasic heap and starts
the 100 kHz stepper interrupt.  RUN (like NEW, LOAD, CHAIN...) wiped the heap
but left the subsystem running, so a queued job went on executing from memory
the next program was reusing.  Program A queues about 80 s of moves and ends
(the job carries on, by design); program B fills the heap and looks.  RUN must
have shut the subsystem down: PEEK(STEPPER ACTIVE) = -1."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

A = """Stepper Init
Stepper Axis X, GP10, GP22, , , 80, 50, 200
Stepper Position X, 0
Stepper Run
For i = 1 To 8 : Stepper GCODE G1, X, i * 100, F, 600 : Next
Pause 500
Print "A active"; Peek(STEPPER ACTIVE); " X>0"; Peek(STEPPER X) > 0
"""
B = """Dim integer a%(3999)
Math Set -1, a%()
Print "B active"; Peek(STEPPER ACTIVE)
If Peek(STEPPER ACTIVE) >= 0 Then
  x1 = Peek(STEPPER X) : Pause 1000 : x2 = Peek(STEPPER X)
  Print "B still moving"; x2 <> x1
  Stepper Close
EndIf
"""

b = pc3.PC3(sys.argv[1])
b.attention()
got = []
for name, src in (("A", A), ("B", B)):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(30))
    lines = [l.strip() for l in out.splitlines() if l.strip() and l.strip() not in ("RUN", ">")]
    print(name, " | ".join(lines))
    got += lines
b.close()
ok = any(re.match(r"A active 1 X>0 1", l) for l in got) and "B active-1" in got
print("STEPRUN18", "PASS" if ok else "FAIL")
