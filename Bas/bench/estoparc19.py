"""estoparc19.py PORT - STEPPER ESTOP during an arc must not leak the arc's
buffer (bug report item 19).  RP2350; on the PC3 run OPTION AUDIO DISABLE
first.  X on GP2/GP3 and Y on GP4/GP5, no motors.

The command path of STEPPER ESTOP stopped the move but kept the executing
arc's segment buffer in current_move, and the next block the ISR loaded
overwrote the pointer, so the buffer was lost until the next RUN.  (The
hardware E-STOP and limit trips already released it.)  Stop a 157 mm arc
after a second, run a short move, and the heap must be back where it was."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Dim h0, h1
Stepper Init
Stepper Axis X, GP2, GP3, , , 80, 50, 200
Stepper Axis Y, GP4, GP5, , , 80, 50, 200
Stepper Position X, 0
Stepper Position Y, 0
Stepper Run
h0 = MM.Info(HEAP)
Stepper GCODE G2, X, 100, Y, 0, I, 50, J, 0, F, 600
Pause 1000
Print "ARC active"; Peek(STEPPER ACTIVE)
Stepper ESTOP
Stepper Run
Stepper GCODE G1, X, 1, Y, 1, F, 600
Do While Peek(STEPPER ACTIVE) = 1 : Loop
Pause 200
Stepper Run
h1 = MM.Info(HEAP)
Print "LEAK"; h0 - h1
Stepper Close
"""

b = pc3.PC3(sys.argv[1])
b.attention()
b.upload(SRC, 30)
b.drain(0.1)
out = pc3.ANSI.sub("", b.run(120))
lines = [l.strip() for l in out.splitlines() if l.strip().startswith(("ARC", "LEAK")) or "rror" in l]
print(" | ".join(lines))
ok = "ARC active 1" in lines and "LEAK 0" in lines
print("ESTOPARC19", "PASS" if ok else "FAIL")
b.close()
