' tflash_p.bas - threads: FLASH RUN, FLASH CHAIN, RAM RUN and RAM CHAIN are the main program's, as RUN and CHAIN are
' and as the CMM2's are (Peter: "make the Picomite consistent with the cmm2"). In a thread each is "Not allowed in a
' thread", before it looks at its slot. (RAM CHAIN from the main program, which ends the threads first as CHAIN does,
' is tried by hand: it needs a program in a RAM slot.)
Option Explicit
Dim Integer tests, pass
Dim m$(3)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Try
  On Error Skip
  Flash Run 1
  m$(0) = MM.ErrMsg$
  On Error Skip
  Flash Chain 1
  m$(1) = MM.ErrMsg$
  On Error Skip
  Ram Run 1
  m$(2) = MM.ErrMsg$
  On Error Skip
  Ram Chain 1
  m$(3) = MM.ErrMsg$
End Sub

Thread Start 1, Try
Thread Wait 1
Check "FLASH RUN in a thread is refused", m$(0) = "Not allowed in a thread", 1
Check "FLASH CHAIN", m$(1) = "Not allowed in a thread", 1
Check "RAM RUN", m$(2) = "Not allowed in a thread", 1
Check "RAM CHAIN", m$(3) = "Not allowed in a thread", 1
Print "tflash:"; pass; " of"; tests; " passed"
