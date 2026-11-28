# curl

curl fetches files and pages from the internet over HTTP and HTTPS, and sends forms and data to web servers. It is the same curl that is on almost every other computer, version 8.23.0, with the same options, so the examples you find on the web work here too.

It runs on the PicoMitePC3 with firmware V8.00.00b5 or later. It needs the PSRAM enabled (OPTION PSRAM PIN), the Wi-Fi of the Pico 2 W or RM2 module, and OPTION WIFI set so that the PicoMite has joined your network. MM.INFO(IP ADDRESS) shows whether it has. curl is a loadable program, and the PicoMitePC3 is the build that has both loadable programs and Wi-Fi: on a build without Wi-Fi curl says so and stops.

## Installing It

Copy the curl folder from the programs zip to the root of the SD card (B:/curl) or of the A: drive (A:/curl). It holds curl.app and cacert.pem, the list of the certificate authorities that HTTPS sites are checked against.

## Starting It

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

## Leaving It

curl ends by itself when it has finished. **Ctrl-C** stops it, in the middle of a transfer or while it prints. A program that ran it then stops too.

## Some Examples

{widths: 26,26}
| The string after RUN "B:/curl/curl.app", | What it does |
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

## Options in a File

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

## Help

-h shows the commonest options. --help all lists every one, and --manual shows curl's whole manual (it is long: Ctrl-C stops it). curl's manual on the web, https://curl.se/docs/manpage.html, is the same.

## Certificates and the Clock

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

## Files on the PicoMite: file: Addresses

curl reads a file on the PicoMite with a file: address, such as file:///curl/notes.txt. It must not have a drive letter (curl accepts those only on Windows, and refuses file:///B:/notes.txt with code 3): the file is on the current drive.

## When It Fails

If curl stops with an error, it says why, and MMBasic gives its number: B:/curl/curl.app ended with code n. A program that ran it goes on with its next statement, but cannot read the number. The commonest are:

{widths: 10,42}
| Code | What it means |
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

## What It Does Not Do

- Only HTTP, HTTPS and file: addresses. FTP, SFTP, SCP, email and the other protocols curl has elsewhere are left out.
- HTTP/1.1 only, which every web server still speaks. Not HTTP/2 or HTTP/3.
- No IPv6, and no international domain names (names with letters other than a to z).
- Compression: gzip and deflate, not Brotli or zstd. Servers send gzip when that is all curl asks for.
- No .curlrc file read at the start, and --remote-time cannot set a file's date.
- No drive letter in a file: address.

## Speed

On a PicoComputer 3, saving to the SD card, curl downloads at about 400 to 500KB a second over plain HTTP from a computer on the same network, and about 300KB a second over HTTPS. From the internet it goes as fast as the site and the Wi-Fi allow: a 200KB file from curl.se came at 70 to 90KB a second. Connecting to a new site over HTTPS takes about 1.7 seconds.

While it runs curl takes about 1.3MB of the PSRAM: the program itself (800KB) and up to 470KB of memory for its work. It gives it all back when it ends.
