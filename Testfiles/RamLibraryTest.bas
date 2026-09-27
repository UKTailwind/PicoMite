' RamLibraryTest.bas - exercises LIBRARY LOAD file$, RAM (RP2350 with PSRAM).
'
' The library goes into RAM slot 5 in PSRAM and shadows the flash library
' until END or the next RUN; the flash library is not touched.  This loads
' Testfiles/ramlib_lib.bas (put it on A:/ first), then checks that its CONST,
' SUB, FUNCTION and CSUB all work from PSRAM, that the slot carries the hash
' tag, and that the slot cannot be taken over while the library is active.
' Run it twice: the second time the load is a re-attach and takes no time.
' Then, at the prompt, RLDouble must be unknown again and RAM LIST must no
' longer say "the RAM library".
LIBRARY LOAD "A:/ramlib_lib.bas", RAM
Option EXPLICIT
Option BASE 0
Option CONSOLE SERIAL

Dim integer fails, tests, a(2), r(15), x, y, n, slot, base, tag

Print "RAM library test on "; MM.DEVICE$; " "; Str$(MM.VER, 1, 6)
If MM.Info(PSRAM SIZE) = 0 Then
  Print "No PSRAM: the library went to flash, nothing to test here"
  End
EndIf

' ---- the library's own top level ran: its CONST exists
Check "CONST from the library", RL_MAGIC, 6011

' ---- a FUNCTION and a SUB
Check "FUNCTION from the library", RLDouble(21), 42
RLHello "called from the program"

' ---- the CSUB runs from PSRAM
x = 1234567890123 : y = 98765 : n = 17
a(0) = x : a(1) = y : a(2) = n
lltest a(), r()
Check "CSUB LMul", r(0), x * y
Check "CSUB add", r(6), x + y
Check "CSUB shl", r(11), x << 5
Check "CSUB marker", r(15), &H600302B8

' ---- the slot: image slot 8 is RAM slot 5; its last two words are the tag
base = MM.Info(FLASH ADDRESS 8)
slot = MM.Info(FLASH ADDRESS 5) - MM.Info(FLASH ADDRESS 4)   ' MAX_PROG_SIZE
Check "library starts with a line", Peek(BYTE base), 1
tag = base + slot - 8
Check "hash tag magic", Peek(WORD tag), &H32494C52
Check "hash is non-zero", Peek(WORD tag + 4) <> 0, 1

' ---- the slot is the library's while it is active
tests = tests + 1
On Error Skip 1
RAM SAVE 5
If MM.ErrNo <> 0 And Instr(MM.ErrMsg$, "holds the library") > 0 Then
  Print "ok:   RAM SAVE 5 refused ->" + MM.ErrMsg$
Else
  Print "FAIL: RAM SAVE 5 was not refused"
  fails = fails + 1
EndIf
On Error Clear
tests = tests + 1
On Error Skip 1
Flash LOAD IMAGE 8, "A:/nothere.bmp"
If MM.ErrNo <> 0 And Instr(MM.ErrMsg$, "holds the library") > 0 Then
  Print "ok:   FLASH LOAD IMAGE 8 refused ->" + MM.ErrMsg$
Else
  Print "FAIL: FLASH LOAD IMAGE 8 was not refused: " + MM.ErrMsg$
  fails = fails + 1
EndIf
On Error Clear

Print "RAM LIST while the library is active:"
RAM LIST

Print "RAM LIBRARY:" + Str$(tests - fails) + " of" + Str$(tests) + " checks passed"
If fails = 0 Then Print "PASS" Else Print "FAIL"
Option CONSOLE BOTH
End

Sub Check(what$, got As integer, want As integer)
  tests = tests + 1
  If got = want Then
    Print "ok:   " + what$ + " =" + Str$(got)
  Else
    Print "FAIL: " + what$ + " got" + Str$(got) + " want" + Str$(want)
    fails = fails + 1
  EndIf
End Sub
