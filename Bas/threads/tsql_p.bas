' tsql_p.bas - threads: SQL in a thread, as the CMM2 allows it (Peter, 26 Nov: "make the Picomite consistent with the
' cmm2"). The SQL module runs on a stack of its own, the process stack, which a thread runs on too: a thread's call
' keeps its stack (Loader.c, LoaderSqlCall). A thread makes a table in a database in memory and reads it back while
' another thread counts; then the main program does the same while a thread counts. The PicoMite builds with SQL
' (the PC3, HDMIUSB), with the module on A: or B: (sql/sql.mod).
Option Explicit
Dim Integer tests, pass, busy, spins, rows, total
Dim names$

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Spin
  Do While busy
    Inc spins
  Loop
End Sub

Sub Db
  rows = 0 : total = 0 : names$ = ""
  SQL Open ":memory:"
  SQL Exec "CREATE TABLE t(a INTEGER, b TEXT)"
  SQL Exec "INSERT INTO t VALUES(1,'one'),(2,'two'),(3,'three'),(4,'four')"
  SQL Query "SELECT a, b FROM t WHERE a > 1 ORDER BY a"
  Do While SQL(NEXT)
    Inc rows
    Inc total, SQL(1)
    names$ = names$ + SQL(2) + " "
  Loop
  SQL Close
End Sub

busy = 1 : spins = 0
Thread Start 2, Spin
Thread Start 1, Db
Thread Wait 1
Check "SQL in a thread: three rows", rows, 3
Check "their sum", total, 9
Check "their names, in order", names$ = "two three four ", 1
Check "and the busy thread ran", spins > 0, 1
spins = 0
Db
busy = 0
Thread Wait 2
Check "SQL in the main program while a thread runs", rows = 3 And total = 9, 1
Check "and the thread ran", spins > 0, 1
Print "tsql:"; pass; " of"; tests; " passed"
