# Bulk reads and the SD read buffer

A design for correcting `FileGetData` and the position arithmetic around it.

Written against the tree at 6.03.02b12. Line numbers are approximate; the
function names are the things to search for. All of it is in `misc/FileIO.c`
unless said otherwise.

## 1. The defect

`SEEK` followed by `MEMORY INPUT` reads from the wrong place on an SD card and
from the right place on the flash filesystem.

Found from the outside: a program that did

```basic
Open "levels.dat" For Input As #1
Seek #1, 2305
Memory Input #1, 2304, packed()
```

got the correct 2304 bytes from `A:` and bytes 2560 onwards from `B:` - 256
late. The files were byte-identical; only the drive differed.

## 2. Why

Three variables describe where a FAT file is being read from:

| | |
|---|---|
| `FileTable[f].fptr->fptr` | FatFS's own byte offset |
| `SDbuffer[f]`, `bw[f]` | a 512-byte read buffer and how many bytes the last fill actually put in it |
| `buffpointer[f]` | how far into that buffer the reader has got |
| `lastfptr[f]` | the `FIL*` when the buffer was filled, used as "this buffer belongs to this file" |

`FileGetChar` treats the buffer as authoritative:

```c
if (!(lastfptr[fnbr] == (uint32_t)FileTable[fnbr].fptr && buffpointer[fnbr] < SDbufferSize))
    { f_read(fptr, buff, SDbufferSize, &bw[fnbr]); buffpointer[fnbr] = 0; lastfptr[fnbr] = fptr; }
ch = buff[buffpointer[fnbr]];
buffpointer[fnbr]++;
```

so while that condition holds, **the logical position is not
`fptr->fptr`** - it is `fptr->fptr - bw + buffpointer`, because the fill read
`bw` bytes past the point the reader has actually reached.

`positionfile` puts a FAT file into exactly that state. For a reader it does
not seek to the target at all:

```c
f_lseek(FileTable[fnbr].fptr, idx - (idx % 512));   /* the 512 boundary below */
f_read(FileTable[fnbr].fptr, buff, SDbufferSize, &bw[fnbr]);
buffpointer[fnbr] = idx % 512;
lastfptr[fnbr] = (uint32_t)FileTable[fnbr].fptr;
```

After `SEEK #1, 2305`: FatFS is at 2560, the buffer covers 2048-2559, and the
logical position is 2304.

`FileGetData` then ignores all of it:

```c
FSerror = f_read(FileTable[fnbr].fptr, buff, count, (UINT *)read);
```

and reads from 2560.

The flash filesystem takes the other branch of `positionfile`, a plain
`lfs_file_seek`, and `lfs_file_read` continues from there. Hence the
difference between the drives.

**It is not only `SEEK`.** The first `FileGetChar` on a file fills the buffer
and leaves FatFS 512 bytes ahead, so a bulk read after any character read has
the same fault.

## 3. What is affected

Bulk reads a BASIC program can get in front of:

* `MEMORY INPUT` - `cmd_memory`, `core/Memory.c`
* `LOAD` struct-from-file - `core/Commands.c`
* `LONGSTRING LOAD` - `fun_linputstr`, `core/MM_Misc.c`

`INPUT$` is **not** one of these: it loops on `FileGetChar` (`fun_inputstr`,
this file) and is correct as it stands.

The firmware's own bulk readers - `onBMPRead` and the BMP line readers in
`graphics/BmpDecoder.c`, the JPG stream callback, `PLAY WAV` in `io/Audio.c`,
the firmware-flashing loop - each open their own handle and only ever read
forwards, so none of them can hit this today. The two internal callers of
`positionfile` that matter are consistent by luck rather than by rule:
`io/Audio.c` uses `noread = true` where a bulk read follows and `false` where
`FileGetChar` follows, and the `LIST` paging code in `core/Commands.c` reads
characters. Neither would survive someone adding a bulk read later, which is
an argument for fixing this centrally rather than at the three call sites.

## 4. Two more defects in the same arithmetic

Found while working out the fix, and both need resolving for the main fix to
be sound.

### 4a. `filegetpos()` is wrong when the buffer fill read nothing

```c
int pos = (int)((((uint64_t)(fptr->fptr) + 511ULL) & ~511ULL) - 512 + buffpointer[fnbr]);
if (pos < 0) pos += 512;
```

This recovers the buffer's base address by rounding FatFS's position **up** to
a 512 boundary and subtracting 512. That is right whenever the fill returned
between 1 and 512 bytes, because the base was 512-aligned and the pointer
then sits strictly inside the following block. It is wrong when the fill
returned **zero** - seeking at or beyond end of file - because the pointer is
then still on the boundary, the round-up is a no-op, and the answer is 512
low. The `pos < 0` guard only rescues the case at the very start of the file.

### 4b. `FileEOF()` underflows

```c
if (buffpointer[fnbr] <= bw[fnbr] - 1 && !(fmode[fnbr] & FA_WRITE))
    i = 0;
```

`bw` is `static unsigned int`. When a fill returns zero bytes, `bw - 1` is
`UINT_MAX`, the test passes, and `FileEOF` reports **not** at end of file.
`FileGetChar` then returns whatever is in the buffer. So `SEEK` to or past the
end followed by `INPUT$` yields rubbish instead of stopping. Both `EOF()` and
`INPUT$`'s own end check reach this through `MMfeof`, so the one correction
covers both.

### 4c. The position arithmetic exists twice

`LOC()` (`fun_loc`, just below the `RoundUptoBlock` macro) has its own copy of the
same reconstruction, `RoundUptoBlock(fptr) - 511 + buffpointer`, which is
`filegetpos() + 1` - `LOC` is 1-based, `filegetpos` is 0-based. The two agree
today, and would both need the 4a correction. Better that only one of them
exists.

## 5. The design

One predicate, one position function, one alignment helper, and every other
piece of code asks those rather than doing the arithmetic itself.

### 5.1 The predicate

```c
/* True while the read buffer, and not FatFS's own offset, holds this file's
   logical read position.  This is the condition FileGetChar already uses to
   decide the buffer is usable; nothing else may reimplement it. */
#define BUFFERLIVE(f) (filesource[f] == FATFSFILE           \
                    && !(fmode[f] & FA_WRITE)               \
                    && lastfptr[f] == (uint32_t)FileTable[f].fptr \
                    && buffpointer[f] < SDbufferSize)
```

Writers are excluded because the write paths never fill the buffer and
`positionfile` already seeks them exactly.

### 5.2 `filegetpos()` - resolving 4a

Stop reconstructing the base by rounding. `bw[f]` is the length of the fill,
so the base is `fptr->fptr - bw[f]` exactly, for every fill length including
zero.

```c
int filegetpos(int fnbr)
{
    if (filesource[fnbr] == FLASHFILE)
        return (int)lfs_file_tell(&lfs, FileTable[fnbr].lfsptr);
    if (!BUFFERLIVE(fnbr))
        return (int)(*(FileTable[fnbr].fptr)).fptr;
    /* The buffer was filled by one read of bw bytes, so it begins bw bytes
       back from where FatFS now is.  No rounding, no sign fix-up, and right
       when the fill returned nothing. */
    return (int)((*(FileTable[fnbr].fptr)).fptr - bw[fnbr] + buffpointer[fnbr]);
}
```

Check it against both ways the buffer gets filled:

* `positionfile`: base `= idx - idx%512`, pointer `= base + bw`, buffpointer
  `= idx%512`. Result `base + bw - bw + idx%512 = idx`.
* `FileGetChar`: base is wherever FatFS was, pointer `= base + bw`,
  buffpointer counts bytes consumed. Result `base + buffpointer`.

### 5.3 `filealign()` - the main fix

```c
/* Bring FatFS's own offset up to the logical position before a bulk read.
   f_read knows nothing about the buffer, so anything that reads through it
   must call this first.  A no-op unless the buffer is live, which it is not
   after an open or after any previous bulk read. */
static void filealign(int fnbr)
{
    if (!BUFFERLIVE(fnbr)) return;
    FSerror = f_lseek(FileTable[fnbr].fptr, filegetpos(fnbr));
    ErrorCheck(fnbr);
    lastfptr[fnbr] = -1;              /* the buffer no longer describes it */
    buffpointer[fnbr] = 0;
}
```

and in `FileGetData`:

```c
if (filesource[fnbr] == FATFSFILE)
{
    filealign(fnbr);
    FSerror = f_read(FileTable[fnbr].fptr, buff, count, (UINT *)read);
    /* Existing: leave the buffer dead so a following FileGetChar refills
       from the new position and FileEOF falls through to f_eof(). */
    lastfptr[fnbr] = -1;
    bw[fnbr] = 1;
    buffpointer[fnbr] = 1;
}
```

Putting it inside `FileGetData` rather than at the three command sites means
a fourth bulk reader added later is covered without anyone remembering, and
the internal readers pay two comparisons.

### 5.4 `FileEOF()` - resolving 4b

```c
if (BUFFERLIVE(fnbr) && (unsigned int)buffpointer[fnbr] < bw[fnbr])
    i = 0;                                  /* buffered data still unread */
else
    i = f_eof(FileTable[fnbr].fptr);
```

`< bw` rather than `<= bw - 1` removes the underflow, and `BUFFERLIVE` stops
it reading stale `buffpointer`/`bw` when the buffer is not the authority.

### 5.5 `FileGetChar()` and `LOC()` - resolving 4c

`FileGetChar`'s refill test becomes `if (!BUFFERLIVE(fnbr))`, so it cannot
drift from `filealign`. `LOC` loses its copy of the arithmetic:

```c
iret = filegetpos(fnbr) + 1;
```

which keeps its 1-based result and its write-mode behaviour, since
`filegetpos` returns `fptr->fptr` for a writer.

### 5.6 `positionfile(..., noread = true)`

The exact-seek branch leaves `lastfptr`/`buffpointer` untouched, so a live
buffer from earlier would still be believed at the old position. Invalidate:

```c
if ((fmode[fnbr] & FA_WRITE) || noread)
{
    FSerror = f_lseek(FileTable[fnbr].fptr, idx);
    ErrorCheck(fnbr);
    lastfptr[fnbr] = -1;
    buffpointer[fnbr] = 0;
}
```

No caller relies on the old behaviour - `io/Audio.c` uses it on a
freshly-opened file - but the invariant should hold without depending on that.

## 6. The trap in the obvious simplification

The tidier-looking fix is to delete the pre-read from `positionfile` so that a
seek is just a seek:

```c
f_lseek(FileTable[fnbr].fptr, idx);
lastfptr[fnbr] = -1;
```

Then FatFS's offset is always the logical position unless `FileGetChar` has
buffered ahead, and `SEEK` followed by a bulk read needs no alignment at all.

**Done on its own, against the tree as it stands, this breaks `SEEK`.**
`filegetpos()` and `LOC()` both reconstruct the position from the buffer; with
the buffer dead and `buffpointer` zero, today's `filegetpos` returns
`RoundUptoBlock(idx) - 512`, which for `idx = 2304` is 2048. The `LIST` paging
code in `core/Commands.c` calls `filegetpos()` immediately after
`positionfile()` and would page from the wrong offsets.

With 5.2 in place the objection disappears: `filegetpos` returns `fptr->fptr`
whenever the buffer is not live, which is exactly right after a bare seek. So
this becomes a **safe optional second phase**, ordered strictly after 5.2:

* it costs nothing - the 512-byte read that `positionfile` does now simply
  moves to the first `FileGetChar`, which would have done it anyway;
* it saves that read entirely when the seek is followed by a bulk read;
* it removes one of the two ways the buffer can come to be live, leaving only
  `FileGetChar`.

Recommended, but separable. Phase 1 (5.1-5.6) is correct with or without it.

## 7. Order of work

1. `BUFFERLIVE` (5.1), `filegetpos` (5.2), `FileGetChar` and `LOC` onto them
   (5.5). No behaviour change yet except at end of file.
2. `FileEOF` (5.4).
3. `filealign` and `FileGetData` (5.3). This is the fix proper.
4. `positionfile` `noread` (5.6).
5. Optional: drop the pre-read (section 6).

## 8. Testing

The defect reproduces from BASIC, so the tests are BASIC. Each must give the
same answer on `A:` and on `B:`; today items 1, 2, 4 and 5 differ between them
and item 3 is wrong on both.

1. **Seek then bulk.** Write a file of 4096 bytes whose every byte is its own
   offset modulo 251. `SEEK` to 2305, `MEMORY INPUT` 16 bytes, check the first
   byte is `2304 MOD 251`. Repeat for offsets that are and are not multiples
   of 512, and for one inside the first block.
2. **Character read then bulk.** `OPEN`, one `INPUT$(1,#1)`, then
   `MEMORY INPUT` 16 bytes; the first byte must be offset 1.
3. **Seek to and past the end.** `SEEK` to the file's length plus one, then
   `EOF(#1)` must be true, and `INPUT$(1,#1)` must return an empty string.
4. **`LOC` after a seek.** `SEEK #1, n : ? LOC(#1)` must give `n`, for `n` on
   and off a 512 boundary, and at the end of the file.
5. **`LOC` and `EOF` after a bulk read.** Both must agree with a character
   read of the same length.
6. **Regressions for the internal callers**: `LIST "file"` paging past the
   first page, `PLAY WAV` including a seek into the middle, `LOAD IMAGE`,
   `LOAD JPG`, and a firmware update from the SD card.

The original failure is also a regression test: the Prince of Pico engine's
`LoadLevel` before commit `7385d46` did `Seek` then `Memory Input`, and fails
with `Index out of bounds` at startup from `B:` while working from `A:`.

## 9. Status

Both phases are in, tested on a PicoMiteHDMIWEB RP2350B with an SD card.

`Testfiles/SeekBulkReadTest.bas` runs the checks of section 8 on `A:` and `B:`
and needs `seektest.bin` on each - 4096 bytes, byte *i* = *i* MOD 251. All
fourteen pass, and the two drives now answer identically on every one of them.

The original failure serves as the end-to-end proof. The Prince of Pico engine
before commit `7385d46` did `Seek` then `Memory Input`; from `B:/board` on the
old firmware it stopped at startup with `Index out of bounds`, and that same
unchanged 145,483-byte file now loads and runs. The game's own 274-check suite
passes from `A:` on both phases, and `LIST` paging - which is what depends on
`filegetpos` being right after a `positionfile` - walks a 111-line file to its
end without repeating or skipping a line.

Phase 2 gave back 136 bytes of flash.

## 10. EOF past the end on the flash filesystem

`SEEK` beyond the end of a file and then `EOF()` answered true on an SD card
and **false** on the flash filesystem. The FAT side of that is section 4b; the
flash side was a separate defect that predates this work and read

```c
i = (lfs_file_tell(...) == lfs_file_size(...));
```

LittleFS allows a seek past the end, so `tell` is then greater than `size` and
the equality fails - reporting "not at the end" for a position there is nothing
to read from. Now `>=`, so the two filesystems agree.

It changes nothing for sequential reading: at the last byte `tell` is
`size - 1`, and the only positions where `>=` and `==` differ are ones a seek
past the end put you in. The two `COPY` loops that compare `tell != size` are
left alone - they read a freshly opened file forwards, so `tell` cannot
overshoot.

## 11. Risks

* `filegetpos` changes its answer at end of file, by design. Anything that
  relied on the old value was relying on a number that was 512 low.
* `FileEOF` will now report end of file in a case where it used to report
  data. That is the point, but a program that looped on `NOT EOF` and relied
  on the old answer to read padding would see the loop end sooner.
* `filealign` performs an `f_lseek` that was not there before. It is a
  position change only, no I/O, and only when the buffer is live.
* `BUFFERLIVE` adds the `FA_WRITE` test to `FileGetChar`'s refill condition.
  The write path returns before reaching it, so this is no change.
