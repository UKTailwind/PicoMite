' SeekBulkReadTest.bas - SEEK, character reads and bulk reads on both drives.
'
' A bulk read (MEMORY INPUT, LONGSTRING LOAD, LOAD struct) goes to the
' filesystem directly, while SEEK and any character read can leave the logical
' position inside a 512-byte buffer that the bulk read knows nothing about.
' On the flash filesystem there is no such buffer and everything agrees; on an
' SD card a bulk read after a seek used to start up to 512 bytes late.
'
' Needs seektest.bin on each drive to be tested: 4096 bytes, byte i = i MOD 251,
' so every byte says where it is.  Make it with
'
'     open("seektest.bin","wb").write(bytes((i % 251) for i in range(4096)))
'
' Every check must give the same answer on A: and on B:.

Option Explicit
Option Default None

Const NBYTES = 4096
Const WRAP = 251

Dim INTEGER tests, fails
Dim INTEGER packed(63), byt(519)

Print "SEEK, character reads and bulk reads"
Print

Chk "A:/"
Chk "B:/"

Print
If fails = 0 Then
  Print "PASS  "; Str$(tests); " of "; Str$(tests)
Else
  Print "FAIL  "; Str$(fails); " of "; Str$(tests); " wrong"
End If
End

Sub Chk(dr As STRING)
  Local STRING f
  f = dr + "seektest.bin"
  If Dir$(f, File) = "" Then
    Print dr; "  no seektest.bin - skipped"
    Exit Sub
  End If
  Print dr

  ' 1. Seek to an offset that is not a multiple of 512, then read in bulk.
  '    SEEK is one-based, so 2305 is byte 2304.
  Open f For Input As #1
  Seek #1, 2305
  Memory Input #1, 16, packed()
  Close #1
  Memory Unpack packed(), byt(), 16, 8
  One "bulk after seek to 2304", byt(0), 2304 Mod WRAP

  ' 2. The same on a 512 boundary, where the old code was out by a whole block.
  Open f For Input As #1
  Seek #1, 2049
  Memory Input #1, 16, packed()
  Close #1
  Memory Unpack packed(), byt(), 16, 8
  One "bulk after seek to 2048", byt(0), 2048 Mod WRAP

  ' 3. A character read fills the buffer too, so a bulk read after one must
  '    carry on from where the reader got to, not from the end of the buffer.
  Open f For Input As #1
  If Asc(Input$(1, #1)) <> 0 Then Print "  (first byte was not 0)"
  Memory Input #1, 16, packed()
  Close #1
  Memory Unpack packed(), byt(), 16, 8
  One "bulk after one char read", byt(0), 1

  ' 4. LOC reports the logical position, not the buffer's far end.
  Open f For Input As #1
  Seek #1, 2305
  One "LOC after seek to 2304", Loc(#1), 2305
  Close #1

  ' 5. Seek to the end: there is nothing to read and EOF must say so.  bw is
  '    unsigned, and the old test said there was data whenever a fill had
  '    returned none.
  Open f For Input As #1
  Seek #1, NBYTES + 1
  One "EOF at the end of the file", Choice(Eof(#1), 1, 0), 1
  Close #1

  ' 6. And past the end.
  Open f For Input As #1
  Seek #1, NBYTES + 600
  One "EOF past the end of the file", Choice(Eof(#1), 1, 0), 1
  Close #1

  ' 7. Two bulk reads in a row continue from one another.
  Open f For Input As #1
  Memory Input #1, 16, packed()
  Memory Input #1, 16, packed()
  Close #1
  Memory Unpack packed(), byt(), 16, 8
  One "second bulk read follows the first", byt(0), 16
End Sub

Sub One(what As STRING, got As INTEGER, want As INTEGER)
  tests = tests + 1
  If got = want Then
    Print "  ok    "; what; " = "; Str$(got)
  Else
    Print "  FAIL  "; what; " want "; Str$(want); " got "; Str$(got)
    fails = fails + 1
  End If
End Sub
