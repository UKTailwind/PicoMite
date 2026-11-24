"""make_sample.py [OUT]: computers.db, the sample database for the SQL chapter's example (computers.bas), made with
Python's own SQLite (the file is the same on the Colour Maximite 2): a table COMPUTER of home computers, its columns the
members of the example's TYPE Computer. Peter, 13 Nov 2026: "The release should include a sample database the example
can work on". The models and years are the machines' launches as usually given; RAMKB is the base model's memory."""
# The column is PROCESSOR, not CPU (24 Nov 2026): the example's TYPE has a member of each column's name, and on
# the PicoMite a member line that starts with a command's name (CPU) is not a member (it is the CPU command).

import os, sqlite3, sys

ROWS = [   # name, maker, year, processor, ramkb
    ('Apple II', 'Apple', 1977, '6502', 4),
    ('PET 2001', 'Commodore', 1977, '6502', 4),
    ('TRS-80 Model I', 'Tandy', 1977, 'Z80', 4),
    ('Atari 800', 'Atari', 1979, '6502', 8),
    ('ZX81', 'Sinclair', 1981, 'Z80', 1),
    ('BBC Micro Model B', 'Acorn', 1981, '6502', 32),
    ('IBM PC', 'IBM', 1981, '8088', 16),
    ('VIC-20', 'Commodore', 1981, '6502', 5),
    ('Commodore 64', 'Commodore', 1982, '6510', 64),
    ('ZX Spectrum', 'Sinclair', 1982, 'Z80', 48),
    ('Dragon 32', 'Dragon Data', 1982, '6809', 32),
    ('Oric-1', 'Oric', 1983, '6502', 48),
    ('Electron', 'Acorn', 1983, '6502', 32),
    ('CPC 464', 'Amstrad', 1984, 'Z80', 64),
    ('QL', 'Sinclair', 1984, '68008', 128),
    ('Macintosh', 'Apple', 1984, '68000', 128),
    ('520ST', 'Atari', 1985, '68000', 512),
    ('Amiga 500', 'Commodore', 1987, '68000', 512),
]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'computers.db')
    if os.path.exists(out):
        os.remove(out)
    db = sqlite3.connect(out)
    db.execute('PRAGMA page_size = 4096')
    db.execute('CREATE TABLE computer(id INTEGER PRIMARY KEY, name TEXT, maker TEXT, year INTEGER, processor TEXT, ramkb INTEGER)')
    db.execute('CREATE INDEX computer_year ON computer(year)')
    db.executemany('INSERT INTO computer(name, maker, year, processor, ramkb) VALUES(?, ?, ?, ?, ?)', ROWS)
    db.commit()
    db.execute('VACUUM')
    db.close()
    print(out, os.path.getsize(out), 'bytes,', len(ROWS), 'rows')


if __name__ == '__main__':
    main()
