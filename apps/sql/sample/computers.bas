' computers.bas: the sample database B:/sql/computers.db, its rows as a TYPE (SQL_Databases.pdf's example)
TYPE Computer
  id AS INTEGER
  name AS STRING LENGTH 30
  maker AS STRING LENGTH 20
  year AS INTEGER
  processor AS STRING LENGTH 10
  ramkb AS INTEGER
END TYPE
DIM c AS Computer, z80(20) AS Computer
DIM INTEGER i, n

SQL OPEN "B:/sql/computers.db"

PRINT "By year:"
SQL QUERY "SELECT * FROM computer ORDER BY year, name"
DO WHILE SQL(FETCH c)
  PRINT c.year, c.name, c.maker
LOOP

n = SQL(SELECT "SELECT * FROM computer WHERE processor = ? ORDER BY ramkb DESC", z80(), "Z80")
PRINT : PRINT n; " with a Z80:"
FOR i = 0 TO n - 1
  PRINT z80(i).name, z80(i).ramkb; "K"
NEXT i

c.id = 0 : c.name = "ZX80" : c.maker = "Sinclair" : c.year = 1980 : c.processor = "Z80" : c.ramkb = 1
SQL EXEC "DELETE FROM computer WHERE name = ?", c.name
SQL INSERT c
PRINT : PRINT "The ZX80 added as number"; c.id

PRINT : PRINT "Makers:"
SQL QUERY "SELECT maker, count(*) FROM computer GROUP BY maker ORDER BY 2 DESC, 1"
DO WHILE SQL(NEXT)
  PRINT SQL(1), SQL(2)
LOOP
SQL CLOSE
