# Pico Frog with threads

`picofrog6_b9_3.bas` is Pico Frog by Martin Herhaus (2022), with later work by Thomas Williams, Volhout and harm, as it
was received but for its display test (below). `picofrog_threads.bas` is the same game with its sound rewritten as two
BASIC threads. It needs PicoMite
V8.00.00b4 or later, on a VGA or HDMI build, a PicoMitePC3, or an LCD panel with FRAMEBUFFER CREATE.

## What changed

The original drives its sound with interrupts:

- **Music**: a SETTICK every 23 ms (33 ms on the RP2040) runs `music`. That routine READs the next pair of notes from
  the DATA and steps them through a volume envelope on `PLAY SOUND` channels 1 to 3. The game changes song by
  `RESTORE`ing the one DATA pointer from wherever it is, so `loadlevel` has to stop the music while it READs the level
  data. The same interrupt also does the LCD's `FRAMEBUFFER MERGE`.
- **Effects**: the jump chirp and the death wail are each a SETTICK 1 routine on channel 4, so each cuts the other off.
- **Timing**: the game loop waits for its next frame in a busy loop.

The threaded version keeps the notes, envelopes, songs and effects exactly as they were, and changes how they run:

- **`MusicThread`** (thread 1) plays the songs in a loop of its own. A thread has its own place in the DATA, so its
  READs and RESTOREs never move the game's: `loadlevel` no longer stops the music. The game asks for a song with
  `PlaySong S_YANKEE` and so on, and the thread starts it at its next note. The beat is kept with `PAUSE` against a
  running deadline, so the tempo does not drift.
- **`SfxThread`** (thread 2) sleeps in `THREAD WAIT FLAG 1` until the game calls `PlaySfx X_JUMP` or `PlaySfx X_DIE`.
  A new effect cuts the one playing short.
- **The game loop** does its own LCD merge, and waits for its frame with `PAUSE`, so the threads run meanwhile.
- There are no SETTICKs left. Game over still RUNs the program again, and RUN ends the threads.

One more change, in both files, needed on the PicoMitePC3: the program picked its display path from
`MM.DEVICE$`, and the PC3's ("PicoMitePC3") has neither "VGA" nor "HDMI" in it, so it took the LCD path and stopped with
"Frame buffer 2 not created". It now asks `MM.INFO(LCDPANEL)`, which is "MODEn" on every VGA or HDMI display and the
panel's name on an LCD: `const lcd%=not(instr(mm.info(lcdpanel),"MODE"))`.

## How it was tested

On a PicoMitePC3 (V8.00.00b4 and later), driven over the console: the start screen, a game started with space, jumps,
deaths and play through to game over and the RUN after it, with no error, and the music and effects listened to on
the board ("sound was perfect", Peter). At a Ctrl-C during play, THREAD LIST shows the main program in its frame's
PAUSE, the music thread in its loop and the effects thread waiting for flag 1; after game over, the start screen
again with both threads started afresh.
