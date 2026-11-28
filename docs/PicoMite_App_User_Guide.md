# PicoMite App User Guide

## Introduction

Your PicoComputer 3 can run much more than MMBasic. This guide covers the programs that come with the firmware: MicroPython, Doom, a NES emulator, a BBC Micro emulator, ScummVM for the LucasArts adventures, curl for fetching from the internet, and the SQLite engine of MMBasic's SQL command. It tells you how to install each one, start it, use it and leave it.

You do not need to be a programmer to use any of them, apart from MicroPython. They are built from the same sources as the Colour Maximite 2's programs, and the firmware lends them its files, display, keyboard, gamepads, mouse, sound, I/O pins and network, so they behave as they do there. The release notes (*New in V8: loadable programs on the PicoMitePC3*) and `tools/sdk/README.md` in the firmware's repository describe how to write programs of this kind yourself.

### What Is Here

<!-- widths: 22,56,22 -->
| Program | What it does | Folder |
|---|---|---|
| MicroPython | Python, with the screen, sound, files, I/O pins and the network | B:/ |
| Doom | The classic game, with the free Freedoom game data | B:/Doom |
| NES | A Nintendo Entertainment System emulator, with two free games | B:/nes |
| BBC Micro | The BBC Model B and BBC Master 128 emulator B-em | B:/bbc |
| ScummVM | The LucasArts adventures, from Monkey Island to Full Throttle (bring your own games) | B:/quest |
| curl | Fetches files and pages from the internet, and sends forms and data, over HTTP and HTTPS | B:/curl |
| SQL | SQLite, the database engine of MMBasic's SQL command, with a sample database | B:/sql |

### Which Boards Run Them

The programs run on the **PicoMitePC3** firmware, which is for the PicoComputer 3, the PicoComputer 2 and the Olimex RP2350pc, with their PSRAM. They also run on the **PicoMiteHDMIUSB** firmware on a board with PSRAM, once the PSRAM is enabled with `OPTION PSRAM PIN`. curl also needs Wi-Fi, so it runs only on the PicoMitePC3 firmware, the one build with both loadable programs and Wi-Fi, with `OPTION WIFI` set. The speeds in this guide were measured on a PicoComputer 3 at 315 MHz, unless they say otherwise.

<!-- widths: 50,50 -->
| Program | Needs firmware |
|---|---|
| MicroPython, Doom, NES | V8.00.00b1 or later |
| BBC Micro, ScummVM | V8.00.00b2 or later |
| SQL | V8.00.00b3 or later |
| curl | V8.00.00b5 or later, PicoMitePC3 only |

### Installing the Programs

The programs are in the programs zip posted with each release of the firmware, named after the firmware's version, such as `PicoMite_Programs-8.00.00b5.zip`.

1. Update the firmware first. Some of these programs need the firmware of the same release.
2. Unzip the file on a PC.
3. Copy everything in it to the root of the SD card, keeping the folders. Use a card reader: Doom's game data alone is 28 MB.

You need only copy the folders you want. Keep the `licences` folder with the programs if you pass them on to anyone else.

A program can also be on the A: drive or a USB flash drive (C:), except Doom, whose game data is too big for A:. Copy a program to the board only with a transfer that keeps its exact size, such as TFTP. XMODEM pads a file to a whole number of blocks, and `RUN` then refuses it.

### Starting a Program

A program is a file ending `.app`, and MMBasic runs it with `RUN`. Change to its folder first, as most of them look there for their files and keep their settings there:

```
DRIVE "B:" : CHDIR "/nes"
RUN "nes.app"
```

A string after a comma is passed to the program. Each chapter says what its program accepts:

```
RUN "bbc.app", "disk/elite.ssd"
```

The program runs at full speed from the PSRAM and returns to MMBasic when it ends. A BASIC program can start one too: when it ends, the BASIC program carries on with its next statement, with its variables as they were. A program's threads wait while it runs.

### Leaving a Program

Programs differ in how you leave them, as an emulator has to pass most keys on to the machine it is emulating:

<!-- widths: 25,75 -->
| Program | How to leave it |
|---|---|
| MicroPython | `machine.reset()` at its prompt |
| Doom | Ctrl-C |
| NES | Ctrl+Alt+End, or Ctrl-C |
| BBC Micro | Ctrl+Alt+End, or Ctrl-] on the serial console. Ctrl-C is a key of the BBC |
| ScummVM | Ctrl-C. In a game, Ctrl+Alt+Del goes back to the list of games |
| curl | It ends when it has finished. Ctrl-C stops it, and a BASIC program that ran it |
| sql.app | It ends when it has run its statements |

When a program ends, MMBasic tidies up after it: its files and directories are closed, its sound is stopped, its I/O pins, PWM, buses and serial ports are released, its network connections are closed, its memory is freed, and BASIC's display mode, colours and font are put back.

### What the Programs Use

**The keyboard and mouse.** All the programs use a USB keyboard. ScummVM and MicroPython use a USB mouse.

**USB gamepads.** The NES and BBC Micro emulators use them, and MicroPython reads them. A generic pad is usually a low speed device: if it gives no response after a restart, unplug it and plug it in again.

**The screen.** A program sets the display mode it needs, and MMBasic puts yours back when it ends. Three of them need the 640x480 resolution, as their pictures are 320x240 or 640x480. At 720x576 the display modes are other sizes, and they say so:

<!-- widths: 25,75 -->
| Program | Resolution |
|---|---|
| MicroPython | `OPTION RESOLUTION 640x480` for its display modes |
| Doom | `OPTION RESOLUTION 640x480` (it uses MODE 5 with a palette) |
| ScummVM | `OPTION RESOLUTION 640x480` (it shows its games in MODE 5, 320x240) |
| BBC Micro | Either. At `OPTION RESOLUTION 720x576` (with the CPU at 404 MHz) each of the BBC's lines is shown twice |

`OPTION RESOLUTION` restarts the board, and its setting is kept.

**Sound.** The programs play their sound through `OPTION AUDIO`'s output, as MMBasic does.

**The network.** curl, and MicroPython's network modules, use the PicoMite's Wi-Fi, once `OPTION WIFI` has joined it to your network: `MM.INFO(IP ADDRESS)` shows whether it has.

**Memory.** A program is loaded into the PSRAM, and its working memory comes from the PSRAM's part of MMBasic's heap. The BBC Micro also copies its 6502 emulation into the processor's own memory (the SRAM) for speed, so it needs a free block of the SRAM heap. At 720x576 the PicoComputer 3's SRAM heap is 55.5 KB rather than 108 KB, as the screen's framebuffer is larger.

### If a Program Will Not Start

<!-- widths: 38,62 -->
| Message | What it means |
|---|---|
| Loadable programs need PSRAM, enabled with OPTION PSRAM PIN | A PicoMiteHDMIUSB without its PSRAM enabled. Enable it with `OPTION PSRAM PIN`. |
| This firmware cannot run loadable programs (.app) | This build of the firmware has no loader. Use the PicoMitePC3 or PicoMiteHDMIUSB firmware. |
| Needs a newer firmware | The program needs a later version of the firmware than you have. Update it. |
| Not a loadable program | The file is not a program, or is not the one you meant. |
| Not a program this firmware can load | The file is a program, but not one built for this firmware. Use the copy from the PicoMite's programs zip. |
| The program file is damaged | Copy it to the card again, with a card reader or TFTP (not XMODEM). |
| Cannot read the program | The file could not be read. Check the card, and copy the file again. |
| Needs n KB, n KB free in one block | There is not enough memory. Restart the board and start the program from the command prompt, not from inside a BASIC program. |
| Needs n KB of the SRAM heap in one block | The same, for the BBC Micro's SRAM part. At 720x576 the SRAM heap is smaller: try it at 640x480. |

If a program cannot find its files, you have probably started it from the wrong folder. `CHDIR` to its folder and try again. A fault in a program restarts the board.

## MicroPython

MicroPython is the Python language for small computers. This is MicroPython 1.29 of the Pico Computer 3, with the same modules as its own MicroPython firmware, so programs written for that run here too.

The book *Programming the Pico Computer 3*, a course in Python from the first keystroke to games, graphics and real applications, works here: every one of its example programs runs, except those for the RGB512 and RGB1024 display modes, which the PicoMite cannot show at 640x480. The book, its examples and their source are in the pico-computer-3 branch of github.com/UKTailwind/micropython (ports/rp2/boards/PICO_COMPUTER_3/book).

### Starting It

`micropython.app` goes in the root of the card. At the MMBasic prompt:

```
RUN "B:/micropython.app"
```

It runs `boot.py` and `main.py` if they are in the root of the A: drive, and then shows its `>>>` prompt. To run one program and go straight back to MMBasic, name it:

```
RUN "B:/micropython.app", "myprog.py"
```

Python's current folder starts as MMBasic's, so the program is found as MMBasic would find it.

### Using It

The prompt appears on the screen and on the serial console. Type Python at it:

```
>>> print("Hello", 6 * 7)
Hello 42
```

These are ready at the prompt as it starts:

<!-- widths: 40,60 -->
| Name | What it does |
|---|---|
| ls(), cd(), pwd(), cat() | List a folder, change folder, show the folder, show a file |
| cp(), mv(), rm(), mkdir(), rmdir() | Copy, rename and delete files, make and delete folders |
| edit(), run() | The editor; run a program |
| fm() | The file manager |
| plot(), Turtle() | Drawing and turtle graphics |
| play(), tone(), beep() | Sound: WAV, MP3, FLAC and MOD files, tones and beeps |
| draw_jpg(), draw_bmp(), draw_png() | Show a picture |
| keydown(), mouse(), gamepad() | Read the keyboard, the mouse and the gamepads |
| settime(), gettime() | Set and read the clock |
| wifi(), ntpsync() | Join a Wi-Fi network; set the clock from the internet |

### The Keys

<!-- widths: 25,75 -->
| Keys | What they do |
|---|---|
| Ctrl-C | Cancels a line, or stops a running program |
| Ctrl-D | A soft reset, which starts MicroPython again |

To leave MicroPython and go back to MMBasic, type:

```
import machine
machine.reset()
```

### Your Files

The A: drive is at `/`, the SD card at `/sd`, and a USB flash drive at `/usb` when one is plugged in. Modules of your own can go in `/lib`.

### What It Has

- The Pico Computer 3's modules for the display (`hdmi`), the keyboard, the mouse, sound and pictures, and its libraries for sprites, tile maps, a GUI and games.
- SQLite, the database, and ulab, for arrays and fast numerical work.
- The `machine` module for the I/O pins, by the PicoMite's GP numbers: `Pin`, `ADC`, `PWM`, `UART`, `I2C`, `SPI`, `RTC` and `WDT`.
- `asyncio`.
- With the Wi-Fi module and `OPTION WIFI`: sockets, `requests` (HTTPS too), MQTT, `ntptime` and `mip`.

### The Display

The display modes of 640x480 and 320x240 need `OPTION RESOLUTION 640x480`. RGB640 (640x480 in 256 colours) is shown with 16 colours chosen from the picture: exactly, when it has 16 colours or fewer, as most of the book's programs do. A mode that the PicoMite cannot show at its resolution gives an OSError.

## Doom

The original game, through doomgeneric, playing Freedoom Phase 1, a free replacement for Doom's game data. It runs at 28 frames a second in its timedemo.

### Starting It

```
DRIVE "B:" : CHDIR "/Doom"
RUN "doom.app"
```

Run it from its folder, so that its settings and saved games are kept there. It takes about 30 seconds to start. It needs `OPTION RESOLUTION 640x480`.

### The Keys

<!-- widths: 35,65 -->
| Keys | What they do |
|---|---|
| The arrows | Move and turn |
| Ctrl | Fire |
| Space | Open doors, use switches |
| Shift | Run |
| Alt with the arrows, or , and . | Strafe |
| Esc | The menu |
| Ctrl-C | Back to MMBasic |

### Sound

It has the game's sound effects, through `OPTION AUDIO`'s output, but no music: the PicoMite has no FM synthesiser.

### Your Own Game Data

To play with a WAD file of your own, copy it into the folder as `doom.wad` or `doom1.wad` and remove `freedoom1.wad`, or name it when you start the game:

```
RUN "doom.app", "-iwad x.wad"
```

To measure its speed, run its timedemo:

```
RUN "doom.app", "-timedemo demo1"
```

## The NES Emulator

This is frank-nes, Mikhail Matveev's NES emulator, on the QuickNES core by Shay Green. It runs in MODE 5 at about 55 frames a second.

### Starting It

```
DRIVE "B:" : CHDIR "/nes"
RUN "nes.app"
```

Its cartridges are the `.nes` files in its folder. It comes with two free games, Nova the Squirrel and Thwaite. To add more, copy their `.nes` files into the folder.

The welcome screen moves on by itself after 10 seconds, whether or not a button has worked.

### The Controls

It uses USB gamepads and the USB keyboard.

<!-- widths: 34,33,33 -->
| Pad | Keyboard | NES |
|---|---|---|
| D-pad or stick | The arrows or WASD | D-pad |
| Circle, Triangle | X | A |
| Cross, Square | Z | B |
| Start | Enter | Start |
| Select | Space | Select |

These work at any time:

<!-- widths: 45,55 -->
| Keys | What they do |
|---|---|
| Start and Select together, or F12 | The menu |
| F11 | Back to the cartridges |
| Ctrl+Alt+End, or Ctrl-C | Back to MMBasic |

### If a Gamepad Does Nothing

A generic pad is usually a low speed device. If it gives no response after a restart, unplug it and plug it in again.

## The BBC Micro

This is frank-micro, rh1tech's port of Tom Walker's B-em by way of Graham Sanderson's B-em for the Pico: the BBC Micro Model B and the BBC Master 128, at the BBC's own 50 frames a second, with its sound, disc images and sideways ROMs. The BBC's ROMs are built in.

### Starting It

```
DRIVE "B:" : CHDIR "/bbc"
RUN "bbc.app"
```

It starts as a Master 128; F12 changes the model.

With `OPTION RESOLUTION 720x576` (the CPU at 404 MHz) each of the BBC's 256 lines is shown twice, as on the Colour Maximite 2; at 640x480 its lines are spread over the screen's 480. At 720x576 it has a third of its time to spare at MODE 7's prompt.

### Discs

Put disc images (`.ssd`, `.dsd`, `.adf`, `.adl`) in `B:/bbc/disk` and open them with F11, or name one when you start it, which puts it in drive 0 and starts it:

```
RUN "bbc.app", "disk/elite.ssd"
```

What the BBC writes goes into the image on the card. To keep the discs as they are, add `-ro`.

### Sideways ROMs

Sideways ROM images (`.rom` or `.bin`) in `B:/bbc/roms` go into its sideways RAM as it starts, on either model. `*SRLOAD` loads one from a disc; then press Ctrl+Pause (CTRL-BREAK) for the BBC to find it.

### The Keys

<!-- widths: 35,65 -->
| Keys | The BBC's key |
|---|---|
| F1 to F10 | f0 to f9 |
| Pause | BREAK (Shift+Pause is SHIFT-BREAK, Ctrl+Pause CTRL-BREAK) |
| End | COPY |
| F11 | The discs |
| F12 | The settings |
| Ctrl+Alt+End | Back to MMBasic (or Ctrl-] on the serial console) |

The other keys give what is printed on them. The first USB gamepad presses Z, X, : and / for its d-pad, and RETURN and SPACE for its buttons.

## ScummVM

The interpreter for the LucasArts adventures, from FRANK Quest, rh1tech's port of ScummVM to the RP2350: ScummVM's SCUMM engine, versions 1 to 7. It has the games' speech, music and sound effects, and takes the USB mouse. No games are included.

### Starting It

It shows the games in MODE 5, 320x240 in 256 colours, so it needs `OPTION RESOLUTION 640x480`.

```
DRIVE "B:" : CHDIR "/quest"
RUN "scummvm.app"
```

It lists the games it finds in folders of `/quest` on the current drive: choose one with the arrow keys and Enter. A game can also be started at once by its folder's name:

```
DRIVE "B:" : CHDIR "/quest" : RUN "scummvm.app", "dott"
```

### Your Games

Games you own go in folders of `/quest`. The folder's name tells ScummVM which game it is, so it must be one of these (in any case); a folder of any other name is not listed. Copy the game's data files only, not its `.EXE` files.

<!-- widths: 22,50,28 -->
| Folder | Game | It needs |
|---|---|---|
| mi1 | The Secret of Monkey Island, VGA floppy | 000.LFL |
| mi1ega | The Secret of Monkey Island, EGA floppy | 000.LFL |
| monkey, mi1cd | The Secret of Monkey Island, CD | monkey.000 |
| monkey2, mi2 | Monkey Island 2: LeChuck's Revenge | monkey2.000 |
| loom | Loom, VGA | 000.LFL |
| indy4, atlantis | Indiana Jones and the Fate of Atlantis | atlantis.000 |
| dott, tentacle | Day of the Tentacle | tentacle.000 |
| samnmax, sam | Sam & Max Hit the Road | its data files |
| ft, fulltp | Full Throttle | ft.la0 |

Day of the Tentacle (CD) has been played on a PicoComputer 3 with its speech, music and the mouse. The Dig (SCUMM v7, which FRANK Quest does not build in) and The Curse of Monkey Island (SCUMM v8) are not supported, nor are the other engines of the Colour Maximite 2's ScummVM (Sierra's, Gobliiins and Kyrandia), which are left out to leave the PSRAM to the games.

### Saving

Saved games are kept in `/quest/saves`. You cannot save during a cutscene.

<!-- widths: 35,65 -->
| Keys | What they do |
|---|---|
| F5 | A menu of nine slots: Up, Down or 1 to 9 choose one, S saves, R restores, Esc goes back |
| Alt-1 to Alt-9 | Save at once, in that slot |
| Ctrl-1 to Ctrl-9 | Restore at once, from that slot |
| Ctrl+Alt+Del | Back to the list of games |
| Ctrl-C | Back to MMBasic |

## curl

curl fetches files and pages from the internet over HTTP and HTTPS, and sends forms and data to web servers. It is the same curl that is on almost every other computer, version 8.23.0, with the same options, so the examples you find on the web work here too.

It runs on the PicoMitePC3 with firmware V8.00.00b5 or later. It needs the PSRAM enabled (OPTION PSRAM PIN), the Wi-Fi of the Pico 2 W or RM2 module, and OPTION WIFI set so that the PicoMite has joined your network. MM.INFO(IP ADDRESS) shows whether it has. curl is a loadable program, and the PicoMitePC3 is the build that has both loadable programs and Wi-Fi: on a build without Wi-Fi curl says so and stops.

### Installing It

Copy the curl folder from the programs zip to the root of the SD card (B:/curl) or of the A: drive (A:/curl). It holds curl.app and cacert.pem, the list of the certificate authorities that HTTPS sites are checked against.

### Starting It

```
RUN "B:/curl/curl.app", "-o page.html https://example.com/"
```

After the program's name comes a string: curl's options and the address, separated by spaces, as on any other computer.

A file curl saves goes in the current folder unless you give a path. The A: drive is in the PicoMite's flash, which is small and slow to write, so save downloads to the SD card: DRIVE "B:" makes it the current drive, or give curl a path such as -o B:/page.html. CHDIR only moves within the current drive.

A word with a space in it goes in double quotes. In a BASIC string a double quote is Chr$(34):

```
q$ = Chr$(34)
RUN "B:/curl/curl.app", "-H " + q$ + "Accept: text/plain" + q$ + " https://example.com/"
```

The whole RUN line can be at most 255 characters. For anything longer, put the options in a file and use -K (see Options in a File, below).

Because the options are a string, a program can make them. RUN of curl.app is one of the program's statements: when curl has finished, the program goes on with the next one.

```
url$ = "https://example.com/"
RUN "B:/curl/curl.app", "-s -o B:/page.html " + url$
PRINT "Fetched"; MM.INFO(FILESIZE "B:/page.html"); " bytes"
```

### Leaving It

curl ends by itself when it has finished. **Ctrl-C** stops it, in the middle of a transfer or while it prints. A program that ran it then stops too.

### Some Examples

<!-- widths: 26,26 -->
| The string after RUN "B:/curl/curl.app", | What it does |
|---|---|
| "https://example.com/" | Shows the page on the screen |
| "-o page.html https://example.com/" | Saves it as page.html |
| "-O https://example.com/files/data.zip" | Saves it under its own name, data.zip |
| "-L -O https://example.com/latest" | Follows redirections to the file itself |
| "-C - -O https://example.com/files/big.zip" | Carries on with a download that was stopped |
| "-I https://example.com/" | Shows only the headers |
| "-d name=Fred&town=Leeds https://example.com/form" | Sends a form (POST) |
| "-F file=@photo.jpg https://example.com/upload" | Sends a file as a form does |
| "-H @json.txt -d @data.json https://example.com/api" | Sends JSON from a file, with the header from json.txt (its line: Content-Type: application/json) |
| "-u user:password https://example.com/private/" | Logs in with a user name and password |
| "-c cookies.txt -b cookies.txt https://example.com/" | Keeps cookies from one run to the next |
| "-sS --compressed -o page.html https://example.com/" | Quietly, with the page sent compressed |

-s turns off the progress meter, and -S still shows errors when -s is on. --compressed asks the server to send gzip, which is often less than a third of the size.

-H @file reads headers from a file, one to a line, so a header with spaces in it can go in a file instead of in quotes.

### Options in a File

-K reads options from a file, one to a line, without the leading dashes. Quotes, long headers and lists of files all go in it. Make it with the editor, or have a program write it with PRINT #:

```
# B:/fetch.cfg
silent
show-error
header = "User-Agent: PicoMite"
url = "https://example.com/one.txt"
output = "one.txt"
url = "https://example.com/two.txt"
output = "two.txt"
```

```
RUN "B:/curl/curl.app", "-K B:/fetch.cfg"
```

Add the line parallel to fetch the files at the same time, which is quicker when there are several. A line that starts with # is a comment.

### Help

-h shows the commonest options. --help all lists every one, and --manual shows curl's whole manual (it is long: Ctrl-C stops it). curl's manual on the web, https://curl.se/docs/manpage.html, is the same.

### Certificates and the Clock

For an HTTPS address curl checks that the site's certificate was signed by one of the authorities in cacert.pem, and refuses the site if not. It looks for cacert.pem in the folder curl.app is in, then for A:/cacert.pem. --cacert gives another file.

**The clock must be right.** A certificate is valid only between two dates, so with the wrong date every HTTPS site is refused. curl then says what date the PicoMite thinks it is:

```
The clock says 2020-01-01, and certificates are checked against it: WEB NTP sets it.
```

Without a real-time clock the PicoMite's date is wrong until something sets it. WEB NTP sets it from the internet, and a real-time clock keeps it (OPTION RTC AUTO ENABLE sets the clock from it when the PicoMite starts).

cacert.pem goes out of date as authorities come and go. curl can fetch a new one itself, from the people who make curl:

```
RUN "B:/curl/curl.app", "-o B:/curl/cacert.new https://curl.se/ca/cacert.pem"
```

then RENAME it over the old one when it has arrived. -k turns the checking off, for a site with a home-made certificate on your own network: never use it for anything that matters.

### Files on the PicoMite: file: Addresses

curl reads a file on the PicoMite with a file: address, such as file:///curl/notes.txt. It must not have a drive letter (curl accepts those only on Windows, and refuses file:///B:/notes.txt with code 3): the file is on the current drive.

### When It Fails

If curl stops with an error, it says why, and MMBasic gives its number: B:/curl/curl.app ended with code n. A program that ran it goes on with its next statement, but cannot read the number. The commonest are:

<!-- widths: 10,42 -->
| Code | What it means |
|---|---|
| 3 | The address is not one curl can use, such as a file: address with a drive letter |
| 6 | The site's name was not found. Check the spelling and that Wi-Fi is up |
| 7 | The site did not answer |
| 22 | The server said no (with -f), such as 404, not found |
| 23 | The file could not be written: a wrong folder, or the card is full |
| 26 | A file to send (-F or -T) could not be read |
| 28 | It took too long |
| 35 | The secure connection could not be made |
| 60 | The site's certificate was refused: see Certificates and the Clock |
| 77 | cacert.pem could not be read: it is not beside curl.app or in A:/ |
| 130 | Ctrl-C was pressed |

Some sites refuse anything that is not a full web browser, and send a page saying so, or 403. curl cannot get past those.

### What It Does Not Do

- Only HTTP, HTTPS and file: addresses. FTP, SFTP, SCP, email and the other protocols curl has elsewhere are left out.
- HTTP/1.1 only, which every web server still speaks. Not HTTP/2 or HTTP/3.
- No IPv6, and no international domain names (names with letters other than a to z).
- Compression: gzip and deflate, not Brotli or zstd. Servers send gzip when that is all curl asks for.
- No .curlrc file read at the start, and --remote-time cannot set a file's date.
- No drive letter in a file: address.

### Speed

On a PicoComputer 3, saving to the SD card, curl downloads at about 400 to 500KB a second over plain HTTP from a computer on the same network, and about 300KB a second over HTTPS. From the internet it goes as fast as the site and the Wi-Fi allow: a 200KB file from curl.se came at 70 to 90KB a second. Connecting to a new site over HTTPS takes about 1.7 seconds.

While it runs curl takes about 1.3MB of the PSRAM: the program itself (800KB) and up to 470KB of memory for its work. It gives it all back when it ends.

## SQL Databases

`sql.mod` is SQLite 3.47.0, the database engine of MMBasic's `SQL` command and `SQL(` function, which give BASIC programs SQL databases on the SD card (B:) or a USB flash drive (C:); a database on A: is refused. It is not a program to `RUN`: `SQL OPEN` loads it from `A:/sql`, or else from `B:/sql`, and `SQL CLOSE` unloads it. So the `sql` folder must be on A: or B: for SQL to work. While a database is open SQLite takes about 2.6 MB of the heap in the PSRAM, and a stack of its own of 16 KB.

```
SQL OPEN "B:/data/club.db"
```

The manual *SQL Databases in PicoMite MMBasic* (`SQL_Databases.pdf`, in the supplementary manuals) describes it all.

### The Example

`computers.db` is a sample database, a table of home computers, and `computers.bas` the manual's example program for it, which reads its rows into a TYPE:

```
RUN "B:/sql/computers.bas"
```

### sql.app

`sql.app` runs SQL statements on a database at the command prompt, each row a line:

```
RUN "B:/sql/sql.app", "B:/sql/computers.db SELECT name, year FROM computer"
```

`-h` lists the columns' names first, and `@` and a file's name runs the statements in the file.

## Appendix A: Licences and Source Code

The programs and games are free software or freely distributable, each under its own licence. The licences are in the `licences` folder of the zip.

### MicroPython

MicroPython (MIT), ulab (MIT), usqlite (MIT), SQLite (public domain), micropython-lib's modules (MIT) and Mbed TLS (Apache-2.0): `micropython-LICENSE.txt`, `micropython-lib-LICENSE.txt`, `ulab-LICENSE.txt`, `usqlite-LICENSE.txt` and `MbedTLS-LICENSE.txt`. Its source is github.com/UKTailwind/micropython, branch picomite (ports/picomite).

### Doom

`doom.app`: the GNU GPL version 2 (`GPL-2.0.txt`). Its source is doomgeneric (github.com/ozkl/doomgeneric) at the commit named in `apps/doom/make.py` of the firmware's repository (github.com/UKTailwind/PicoMite), with the files of that folder.

`freedoom1.wad`: copyright the Freedoom project's contributors, under the BSD 3-Clause licence (`Freedoom-COPYING.txt`).

### The NES Emulator and Its Games

`nes.app`: frank-nes, copyright (c) 2026 Mikhail Matveev, the GNU GPL version 3 (`GPL-3.0.txt`); QuickNES, the GNU LGPL version 2.1 or later (`LGPL-2.1.txt`). Its source is github.com/rh1tech/frank-nes at the commit named in `apps/nes/make.py` of the firmware's repository, with the patches and the files in `apps/nes`.

`nova.nes`: Nova the Squirrel 1.0.6a, by NovaSquirrel. Its code is under the GNU GPL version 3 or later (`GPL-3.0.txt`), and its graphics and levels under Creative Commons BY-NC-SA 4.0. Its source is github.com/NovaSquirrel/NovaTheSquirrel.

`thwaite.nes`: Thwaite 0.04, by Damian Yerrick, the GNU GPL version 3 or later (`GPL-3.0.txt`). Its source is github.com/pinobatch/thwaite-nes.

### The BBC Micro

`bbc.app`: frank-micro by Mikhail Matveev, the GNU GPL version 3 (`GPL-3.0.txt`), with B-em by Tom Walker and others in it, through Graham Sanderson's version for the Pico, the GNU GPL version 2 or later (`GPL-2.0.txt`). Its source is github.com/rh1tech/frank-micro at the commit named in `apps/bbc/make.py` of the firmware's repository, with the patches and the files in `apps/bbc`.

The BBC's ROMs built into `bbc.app` are not under the GPL. The Model B's OS 1.20, BASIC 2 and DFS 2.26, and the Master 128's MOS 3.20 with BASIC 4, VIEW, ViewSheet, Edit, Terminal, ADFS and DFS, are copyright Acorn Computers and those who now hold its rights. They are built in as frank-micro has them, for emulation and preservation, as other BBC emulators include them. No permission has been obtained.

### ScummVM

`scummvm.app`: the GNU GPL version 2 or later, with parts under other licences as ScummVM's files say (`ScummVM-*`). Its source is FRANK Quest (github.com/rh1tech/frank-quest) at the commit named in `apps/scummvm/make.py` of the firmware's repository, with the files of that folder.

### curl

`curl.app`: curl 8.23.0, under the curl licence, an MIT-style licence (`curl-COPYING.txt`); Mbed TLS 3.6.8, under the Apache 2.0 licence or the GNU GPL version 2 or later (`MbedTLS-LICENSE.txt`); and zlib 1.3.1, under the zlib licence (`zlib-LICENSE.txt`). `cacert.pem`, from Mozilla's list of certificate authorities: the Mozilla Public License 2.0 (`MPL-2.0.txt`). Its source is `apps/curl` in the firmware's repository, which fetches curl, Mbed TLS and zlib from their own sites and `cacert.pem` from curl.se.

### SQL

`sql.mod` and `sql.app`: SQLite 3.47.0 (public domain), with usqlite's settings (MIT: `usqlite-LICENSE.txt`); the OS layer, the module and the program are MIT. Their source is `apps/sql` in the firmware's repository, which fetches SQLite from sqlite.org.
