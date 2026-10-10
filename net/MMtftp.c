/***********************************************************************************************************************
PicoMite MMBasic

MMtftp.c

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed
    on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed
    by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software
    without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "lwip/apps/tftp_common.h"
#include "lwip/apps/tftp_server.h"
struct tftp_context ctx;
int tftp_fnbr;

// The TFTP callbacks run from the network poll, in the middle of whatever the
// interpreter is doing. A file error must go back to the client as a TFTP
// error: error() would longjmp out of lwIP, and stop a running program with
// an error it did not cause. So file errors do not abort here, and the
// foreground's drive selection, FSerror, MM.ERRNO and MM.ERRMSG$ are put back.
typedef struct
{
    int drive, abort, fserror, errnum;
    char msg[MAXERRMSG];
} tftp_guard_t;

static void __attribute__((noinline)) tftp_guard_enter(tftp_guard_t *g)
{
    g->drive = FatFSFileSystem;
    g->abort = OptionFileErrorAbort;
    g->fserror = FSerror;
    g->errnum = MMerrno;
    memcpy(g->msg, MMErrMsg, MAXERRMSG);
    OptionFileErrorAbort = 0;
    MMerrno = 0; // so a failure reports its own message, not the foreground's
    *MMErrMsg = 0;
}

static void __attribute__((noinline)) tftp_guard_leave(tftp_guard_t *g)
{
    FatFSFileSystem = g->drive;
    OptionFileErrorAbort = g->abort;
    FSerror = g->fserror;
    MMerrno = g->errnum;
    memcpy(MMErrMsg, g->msg, MAXERRMSG);
}

void *tftp_open(const char *fname, const char *fmode, u8_t write)
{
    // Take the drive from the name the client sent. getfullfilename() only
    // honours an "X:/" prefix while cmdline holds a command, and this runs from
    // the network poll whatever the interpreter is doing: after an error, EDIT
    // or XMODEM cmdline is NULL, and after a command with no arguments (CLS)
    // it is empty, so "B:/name" went to the current drive.
    const char *name = fname;
    int target = FatFSFileSystem;
    if (fname[0] && fname[1] == ':' && fname[2] == '/')
    {
        switch (mytoupper(fname[0]))
        {
        case 'A':
            target = 0;
            break;
        case 'B':
            target = 1;
            break;
#if HAS_USB_MSC
        case 'C':
            target = 2;
            break;
#endif
        default:
            return NULL;
        }
        name = fname + 2;
    }
    // Refuse what would raise an error directly rather than through
    // ErrorThrow(): no free file number (FindFreeFileNbr), or B: without an
    // SD card configured (InitSDCard).
    int fnbr = 0;
    for (int i = MAXOPENFILES; i >= 1 && fnbr == 0; i--)
        if (FileTable[i].com == 0)
            fnbr = i;
    if (fnbr == 0 || (target == 1 && !SDCardConfigured()))
        return NULL;
    tftp_guard_t g;
    tftp_guard_enter(&g);
    void *handle = NULL;
    FatFSFileSystem = target;
    if (InitSDCard())
    {
        BYTE mode = write ? FA_WRITE | FA_CREATE_ALWAYS : FA_READ;
        if (!optionsuppressstatus)
        {
            MMPrintString(write ? "TFTP request to create " : "TFTP request to read ");
            MMPrintString(strcmp(fmode, "octet") == 0 ? "binary file : " : "ascii file : ");
            MMPrintString((char *)fname);
            PRet();
        }
        // Select the drive again: InitSDCard() calls ErrorThrow(), which resets
        // FatFSFileSystem to the current drive.
        FatFSFileSystem = target;
        if (BasicFileOpen((char *)name, fnbr, mode))
        {
            tftp_fnbr = fnbr;
            handle = &tftp_fnbr;
        }
    }
    if (handle == NULL && !optionsuppressstatus)
    {
        MMPrintString("TFTP error: ");
        MMPrintString(*MMErrMsg ? MMErrMsg : "cannot open the file");
        PRet();
    }
    tftp_guard_leave(&g);
    return handle;
}

void tftp_close(void *handle)
{
    int fnbr = *(int *)handle;
    tftp_guard_t g;
    tftp_guard_enter(&g);
    FileClose(fnbr);
    tftp_guard_leave(&g);
    if (!optionsuppressstatus)
        MMPrintString("TFTP transfer complete\r\n");
}
int tftp_read(void *handle, void *buf, int bytes)
{
    unsigned int n_read = 0;
    int fnbr = *(int *)handle;
    tftp_guard_t g;
    tftp_guard_enter(&g);
    int err = FileGetData(fnbr, buf, bytes, &n_read);
    tftp_guard_leave(&g);
    return err ? -1 : (int)n_read; // -1: the server sends an error and closes
}
int tftp_write(void *handle, struct pbuf *p)
{
    int fnbr = *(int *)handle;
    tftp_guard_t g;
    tftp_guard_enter(&g);
    FSerror = 0;
    FilePutData(p->payload, fnbr, p->tot_len);
    int err = FSerror;
    tftp_guard_leave(&g);
    return err ? -1 : p->tot_len; // -1: the server sends an error and closes
}
void tftp_error(void *handle, int err, const char *msg, int size)
{
    int fnbr = *(int *)handle;
    tftp_guard_t g;
    tftp_guard_enter(&g);
    ForceFileClose(fnbr);
    tftp_guard_leave(&g);
    MMPrintString("TFTP Error: ");
    MMPrintString((char *)msg);
    PRet();
}
int cmd_tftp_server_init(void)
{
    ctx.open = tftp_open;
    ctx.close = tftp_close;
    ctx.error = tftp_error;
    ctx.write = tftp_write;
    ctx.read = tftp_read;
    tftp_init_server(&ctx);
    return 1;
}