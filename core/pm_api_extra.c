/* PicoMite C services shared by the RP2040 ELF runner and RP2350 TinyCC.
 * There is no compiler dependency: error/abort handling belongs to
 * pm_core_call(), which dispatches through the active run context.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include "core/tcc/include/picomite/gui.h"
#include "core/tcc/include/picomite/basic.h"
#include "pico/time.h"
/* graphics/Draw.c provides these on VGA and non-VGA builds. */
extern int last_fcolour, last_bcolour;
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define PM_DEFAULT_ARG (-2)

static void pm_core_range(int v, int lo, int hi)
{
    if (v < lo || v > hi)
        error("~ is invalid (valid is ~ to ~)", (long long)v, (long long)lo, (long long)hi);
}


/* Directly dispatch the existing TOUCH()/CLICK() accessors.  The BASIC
   expression pointer is borrowed only for the duration of this call. */
typedef struct { const char *key; int value; int click; } pm_point_query;
static void pm_point_invoke(void *p)
{
    pm_point_query *a = p;
    if (a->click) {
#if defined(GUICONTROLS) || (defined(PICOMITEVGA) && defined(USBKEYBOARD))
        fun_click();
        a->value = (int)iret;
#else
        a->value = -2;
#endif
    } else {
#if !defined(PICOMITEVGA) || defined(GUICONTROLS) || defined(USBKEYBOARD)
        fun_touch();
        a->value = (int)iret;
#else
        a->value = -2;
#endif
    }
}
static void pm_point_query_do(void *p)
{
    pm_point_query *a = p;
    unsigned char buf[48];
    unsigned char *old_ep = ep;
    long long old_iret = iret;
    int old_targ = targ, old_errno = MMerrno;
    char old_msg[MAXERRMSG];
    size_t n;
    if (!a->key || (n = strlen(a->key)) >= sizeof(buf)) {
        a->value = -2;
        return;
    }
    memcpy(old_msg, MMErrMsg, sizeof(old_msg));
    memcpy(buf, a->key, n + 1);
    ep = buf;
    if (pm_try(pm_point_invoke, a)) a->value = -2;
    ep = old_ep;
    iret = old_iret;
    targ = old_targ;
    MMerrno = old_errno;
    memcpy(MMErrMsg, old_msg, sizeof(old_msg));
}
int pm_touch(const char *key)
{
    pm_point_query a = {key, -2, 0};
    pm_core_call(pm_point_query_do, &a);
    return a.value;
}
int pm_click(const char *key)
{
    pm_point_query a = {key, -2, 1};
    pm_core_call(pm_point_query_do, &a);
    return a.value;
}

static void pm_gui_badarg(void *p) { (void)p; error("GUI command too long"); }
void pm_gui(const char *subcommand)
{
    char cmd[STRINGSIZE];
    size_t n;
    if (!subcommand || (n = strlen(subcommand)) > sizeof(cmd) - 5) {
        pm_core_call(pm_gui_badarg, NULL);
        return;
    }
    memcpy(cmd, "GUI ", 4);
    memcpy(cmd + 4, subcommand, n + 1);
    pm_execute(cmd);
}

#ifdef GUICONTROLS
extern int pm_gui_ctrl_number(int, double *);
extern int pm_gui_ctrl_text(int, char *, unsigned);
extern int pm_gui_ctrl_set_number(int, double);
extern int pm_gui_ctrl_set_text(int, const char *);
#endif

typedef struct { int ref, status; double number; double *number_out; char *text; const char *src; unsigned size; } pm_ctrl_arg;
static void pm_ctrl_do(void *p)
{
    pm_ctrl_arg *a = p;
#ifdef GUICONTROLS
    if (a->number_out) a->status = pm_gui_ctrl_number(a->ref, a->number_out);
    else if (a->src) a->status = pm_gui_ctrl_set_text(a->ref, a->src);
    else if (a->text) a->status = pm_gui_ctrl_text(a->ref, a->text, a->size);
    else a->status = pm_gui_ctrl_set_number(a->ref, a->number);
#else
    a->status = -1;
#endif
}
int pm_ctrlval_get_number(int ref, double *out)
{
    pm_ctrl_arg a = {0};
    if (!out) return -1;
    a.ref = ref; a.status = -1; a.number_out = out;
    pm_core_call(pm_ctrl_do, &a);
    return a.status;
}
int pm_ctrlval_set_number(int ref, double value)
{
    pm_ctrl_arg a = {0};
    a.ref = ref; a.status = -1; a.number = value;
    pm_core_call(pm_ctrl_do, &a);
    return a.status;
}
int pm_ctrlval_get_text(int ref, char *buf, unsigned size)
{
    pm_ctrl_arg a = {0};
    if (!buf || !size) return -1;
    a.ref = ref; a.status = -1; a.text = buf; a.size = size;
    buf[0] = 0;
    pm_core_call(pm_ctrl_do, &a);
    return a.status;
}
int pm_ctrlval_set_text(int ref, const char *value)
{
    pm_ctrl_arg a = {0};
    if (!value) return -1;
    a.ref = ref; a.status = -1; a.src = value;
    pm_core_call(pm_ctrl_do, &a);
    return a.status;
}

/* The GUI subscription is optional. Keep only its pointer in static RAM;
   the queue and all counters live in the RUN allocator and are released
   on unsubscribe or when the RUN scope ends. */
#ifdef GUICONTROLS
#define PM_GUI_EVENTS 16
struct pm_gui_event { int kind, x, y, ref; };
struct pm_gui_state {
    struct pm_gui_event events[PM_GUI_EVENTS];
    unsigned head, count;
    pm_gui_callback callback;
    int down_x, down_y;
    bool dispatching, down_pending;
};
static struct pm_gui_state *pm_gui_active;

static void pm_gui_push(int kind, int x, int y, int ref)
{
    struct pm_gui_state *s = pm_gui_active;
    unsigned slot;
    if (!s || !pm_runtime_is_running() || !s->callback) return;
    if (s->count == PM_GUI_EVENTS) {
        s->head = (s->head + 1) % PM_GUI_EVENTS;
        --s->count;
    }
    slot = (s->head + s->count) % PM_GUI_EVENTS;
    s->events[slot] = (struct pm_gui_event){kind, x, y, ref};
    ++s->count;
}
void pm_gui_event_down_mark(int x, int y)
{
    struct pm_gui_state *s = pm_gui_active;
    if (!s) return;
    s->down_pending = pm_runtime_is_running() && s->callback;
    s->down_x = x;
    s->down_y = y;
}
void pm_gui_event_down_finish(int ref)
{
    struct pm_gui_state *s = pm_gui_active;
    if (!s) return;
    if (s->down_pending)
        pm_gui_push(PM_GUI_DOWN, s->down_x, s->down_y, ref);
    s->down_pending = false;
}
void pm_gui_event_up_mark(int x, int y, int ref)
{
    pm_gui_push(PM_GUI_UP, x, y, ref);
}
#endif
void pm_gui_runtime_begin(void)
{
#ifdef GUICONTROLS
    pm_gui_active = NULL;
#endif
}
void pm_gui_runtime_end(void)
{
#ifdef GUICONTROLS
    struct pm_gui_state *s = pm_gui_active;
    pm_gui_active = NULL;
    if (s) pm_runtime_free(s);
#endif
}
static void pm_gui_subscribe_do(void *p)
{
#ifdef GUICONTROLS
    pm_gui_callback cb = *(pm_gui_callback *)p;
    struct pm_gui_state *s;
    if (!pm_runtime_is_running()) error("GUI callbacks require RUN .c/.elf");
    s = pm_gui_active;
    if (!cb) {
        if (!s) return;
        s->callback = NULL;
        s->head = s->count = 0;
        s->down_pending = false;
        /* Unsubscribe inside a callback: defer free until polling ends. */
        if (!s->dispatching) {
            pm_gui_active = NULL;
            pm_runtime_free(s);
        }
        return;
    }
    if (!s) {
        s = pm_runtime_malloc(sizeof(*s));
        if (!s) error("Not enough memory for GUI event queue");
        memset(s, 0, sizeof(*s));
        pm_gui_active = s;
    }
    s->callback = cb;
    s->head = s->count = 0;
    s->down_pending = false;
#else
    (void)p;
    error("GUI controls are not available");
#endif
}
void pm_gui_on_event(pm_gui_callback callback)
{
    pm_core_call(pm_gui_subscribe_do, &callback);
}
void pm_gui_runtime_poll(void)
{
#ifdef GUICONTROLS
    struct pm_gui_state *s = pm_gui_active;
    unsigned quota = PM_GUI_EVENTS;
    if (!pm_runtime_is_running()) return;
    /* The BASIC executor calls ProcessTouch() from check_interrupt().
       Native C RUN/pm_pause() never enters that dispatcher, so GUI CLICK
       (and mouse/touch input) needs the same main-thread service here.
       Do not run the GUI engine while a popup drawing is deferred. */
    if (Ctrl) {
        if (!(DelayedDrawKeyboard || DelayedDrawFmtBox || DelayedDrawListbox || calibrate))
            ProcessTouch();
        if (CheckGuiFlag)
            CheckGui();
    }
#if defined(USBKEYBOARD) && defined(PICOMITEVGA)
    else if (OSK_IsActive() && !calibrate)
        ProcessTouch();
#endif
#ifdef USBKEYBOARD
    else if (usb_touch_present && !calibrate)
        ProcessTouch();
#endif
    if (!s || s->dispatching) return;
    s->dispatching = true;
    while (s->count && s->callback && quota--) {
        struct pm_gui_event ev = s->events[s->head];
        pm_gui_callback cb = s->callback;
        s->head = (s->head + 1) % PM_GUI_EVENTS;
        --s->count;
        cb(ev.kind, ev.x, ev.y, ev.ref);
    }
    s->dispatching = false;
    if (!s->callback) {
        pm_gui_active = NULL;
        pm_runtime_free(s);
    }
#endif
}
void pm_gui_poll(void) { pm_core_call(NULL, NULL); }

void pm_print(const char *s)
{
    if (s) MMPrintString((char *)s);
}

static void pm_core_yield_do(void *p)
{
    (void)p;
#ifdef PICOMITEWEB
    if (WIFIconnected || WebScanActive) ProcessWeb(1);
#endif
    ProcessBT();
    routinechecks();
}
void pm_yield(void) { pm_core_call(pm_core_yield_do, NULL); }

void pm_delay_us(unsigned int us)
{
    uint64_t end = time_us_64() + (uint64_t)us;
    while (time_us_64() < end)
        pm_core_call(NULL, NULL);
}
void pm_pause(unsigned int ms)
{
    uint64_t end = time_us_64() + (uint64_t)ms * 1000u;
    do pm_yield(); while (time_us_64() < end);
}

static void pm_core_keydown_do(void *p)
{
    int *a = p;
    pm_core_range(a[0], 0, 8);
    a[1] = KeyDownValue(a[0]);
}
int pm_keydown(int n)
{
    int a[2] = {n, 0};
    pm_core_call(pm_core_keydown_do, a);
    return a[1];
}

/* DEVICE(MOUSE channel, what): X/Y/L/R/M/W/B/D/T, matching BASIC. */
static void pm_core_mouse_do(void *p)
{
    int *a = p;
    volatile struct s_nunstruct *m;
    pm_core_range(a[0], 1, 4);
    m = &nunstruct[a[0]];
    switch (toupper((unsigned char)a[1])) {
    case 'X': a[2] = m->ax; break;
    case 'Y': a[2] = m->ay; break;
    case 'L': a[2] = m->L; break;
    case 'R': a[2] = m->R; break;
    case 'M': a[2] = m->C; break;
    case 'W': a[2] = m->az; break;
    case 'B': a[2] = m->x0; break;
    case 'D': a[2] = m->Z; m->Z = 0; break;
    case 'T': a[2] = m->classic[0]; break;
    default: SyntaxError();
    }
}
int pm_mouse(int channel, int what)
{
    int a[3] = {channel, what, 0};
    pm_core_call(pm_core_mouse_do, a);
    return a[2];
}

static void pm_core_usb_do(void *p)
{
    int *a = p;
#ifdef USBKEYBOARD
    pm_core_range(a[0], 0, 4);
    a[1] = a[0] ? HID[a[0] - 1].Device_type : Current_USB_devices;
#else
    (void)a;
    SyntaxError();
#endif
}
int pm_usb_device(int port)
{
    int a[2] = {port, 0};
    pm_core_call(pm_core_usb_do, a);
    return a[1];
}

int pm_errno(void) { return MMerrno; }
const char *pm_errmsg(void) { return MMErrMsg; }
void pm_error_clear(void) { MMerrno = 0; MMErrMsg[0] = 0; }

static int pm_core_fb_bpp(void)
{
#ifdef PICOMITEVGA
    if (DISPLAY_TYPE == SCREENMODE1) return 1;
#ifdef HDMI
    if (DISPLAY_TYPE == SCREENMODE4) return 16;
    if (DISPLAY_TYPE == SCREENMODE5) return 8;
#endif
#endif
    return 4;
}

static int pm_core_fill(void *dst, const void *src, unsigned have)
{
    unsigned want;
    if (!dst) return -1;
    want = *(const unsigned *)dst;
    if (want < sizeof(unsigned)) return -1;
    memcpy((char *)dst + sizeof(unsigned), (const char *)src + sizeof(unsigned),
           (want < have ? want : have) - sizeof(unsigned));
    return 0;
}

int pm_get_framebuffer(void *out)
{
    struct { unsigned size; void *pixels; int width, height, stride, format; } f;
    unsigned char *buf = WriteBuf;
#ifndef PICOMITEVGA
#if PICOMITERP2350
    if (buf == NULL && Option.DISPLAY_TYPE >= VIRTUAL && Option.DISPLAY_TYPE < VGA222)
        buf = FRAMEBUFFER;
#else
    if (buf == NULL && Option.DISPLAY_TYPE >= VIRTUAL)
        buf = FRAMEBUFFER;
#endif
#endif
    if (!buf || HRes <= 0 || VRes <= 0) return -1;
    memset(&f, 0, sizeof(f));
    f.pixels = buf;
    f.width = HRes;
    f.height = VRes;
    f.format = pm_core_fb_bpp();
    f.stride = f.format == 1 ? (HRes + 7) >> 3 : f.format == 4 ? (HRes + 1) >> 1 : HRes * (f.format >> 3);
    return pm_core_fill(out, &f, sizeof(f));
}
unsigned pm_fb_colour(int rgb)
{
    switch (pm_core_fb_bpp()) {
    case 1: return (rgb & 0xffffff) != 0;
    case 8: return RGB332((uint32_t)rgb);
    case 16: return RGB555((uint32_t)rgb);
    default: return RGB121((uint32_t)rgb);
    }
}

static int pm_core_font_code(int font, int scale)
{
    if (font == PM_DEFAULT_ARG) font = (gui_font >> 4) + 1;
    if (scale == PM_DEFAULT_ARG) scale = gui_font & 15;
    pm_core_range(font, 1, FONT_TABLE_SIZE);
    if (!FontTable[font - 1]) error("Invalid font #%", font);
    pm_core_range(scale, 1, 15);
    return ((font - 1) << 4) | scale;
}
static void pm_core_font_width_do(void *p)
{
    int *a = p; a[2] = GetFontWidth(pm_core_font_code(a[0], a[1]));
}
int pm_font_width(int font, int scale)
{
    int a[3] = {font, scale, 0};
    pm_core_call(pm_core_font_width_do, a);
    return a[2];
}
static void pm_core_font_height_do(void *p)
{
    int *a = p; a[2] = GetFontHeight(pm_core_font_code(a[0], a[1]));
}
int pm_font_height(int font, int scale)
{
    int a[3] = {font, scale, 0};
    pm_core_call(pm_core_font_height_do, a);
    return a[2];
}

static int pm_core_colour_arg(int c, int lo, int dflt)
{
    if (c == PM_DEFAULT_ARG) return dflt;
    pm_core_range(c, lo, WHITE);
    return c;
}

typedef struct {
    int x, y, font, scale, fg, bg;
    const char *s, *align;
} pm_text_args;
static void pm_core_text_do(void *p)
{
    const pm_text_args *a = p;
    int jh = 0, jv = 0, jo = 0;
    CheckDisplay();
    if (a->align && *a->align &&
        !GetJustification((char *)a->align, &jh, &jv, &jo)) error("Justification");
    GUIPrintString(a->x, a->y, pm_core_font_code(a->font, a->scale),
                   jh, jv, jo,
                   pm_core_colour_arg(a->fg, 0, gui_fcolour),
                   pm_core_colour_arg(a->bg, -1, gui_bcolour),
                   (char *)a->s);
    if (Option.Refresh) Display_Refresh();
}
void pm_text(int x, int y, const char *s, const char *align, int font, int scale, int fg, int bg)
{
    pm_text_args a = {x, y, font, scale, fg, bg, s, align};
    pm_core_call(pm_core_text_do, &a);
}

static void pm_core_draw_box_do(void *p)
{
    int *a = p;
    int x = a[0], y = a[1], w = a[2], h = a[3], lw = a[4], c = a[5], fill = a[6];
    CheckDisplay();
    if (lw == PM_DEFAULT_ARG) lw = 1;
    else pm_core_range(lw, 0, 100);
    c = pm_core_colour_arg(c, 0, gui_fcolour);
    fill = pm_core_colour_arg(fill, -1, -1);
    if (w && h) DrawBox(x, y, x + w + (w > 0 ? -1 : 1), y + h + (h > 0 ? -1 : 1), lw, c, fill);
    if (Option.Refresh) Display_Refresh();
}
void pm_draw_box(int x, int y, int w, int h, int lw, int c, int fill)
{
    int a[7] = {x, y, w, h, lw, c, fill};
    pm_core_call(pm_core_draw_box_do, a);
}

static void pm_core_draw_rbox_do(void *p)
{
    int *a = p;
    int x = a[0], y = a[1], w = a[2], h = a[3], r = a[4], c = a[5], fill = a[6];
    CheckDisplay();
    if (r == PM_DEFAULT_ARG) r = 10;
    else pm_core_range(r, 0, 100);
    c = pm_core_colour_arg(c, 0, gui_fcolour);
    fill = pm_core_colour_arg(fill, -1, -1);
    if (w && h) DrawRBox(x, y, x + w + (w > 0 ? -1 : 1), y + h + (h > 0 ? -1 : 1), r, c, fill);
    if (Option.Refresh) Display_Refresh();
}
void pm_draw_rbox(int x, int y, int w, int h, int r, int c, int fill)
{
    int a[7] = {x, y, w, h, r, c, fill};
    pm_core_call(pm_core_draw_rbox_do, a);
}

static int pm_core_fb_letter(int b)
{
    b = toupper((unsigned char)b);
    if (b != 'N' && b != 'F' && b != 'L' && b != 'T' && b != '2')
        error("Invalid argument");
    return b;
}
static void pm_core_blit_fb_do(void *p)
{
    int *a = p;
    a[0] = pm_core_fb_letter(a[0]);
    a[1] = pm_core_fb_letter(a[1]);
    BlitFramebufferC(a[0], a[1], a[2], a[3], a[4], a[5],
                     a[6], a[7], a[8] == PM_DEFAULT_ARG ? -1 : a[8]);
}
void pm_blit_framebuffer(int from, int to, int x1, int y1, int x2, int y2,
                         int w, int h, int colour)
{
    int a[9] = {toupper((unsigned char)from), toupper((unsigned char)to),
                x1, y1, x2, y2, w, h, colour};
    pm_core_call(pm_core_blit_fb_do, a);
}

/* Direct services also used by the BASIC graphics/audio engine. */
static void pm_core_sprite_show_do(void *p)
{
    int *a = p;
    SpriteShowC(a[0], a[1], a[2], a[3], a[4] == PM_DEFAULT_ARG ? -1 : a[4]);
}
void pm_sprite_show(int n, int x, int y, int layer, int rotation)
{
    int a[5] = {n, x, y, layer, rotation};
    pm_core_call(pm_core_sprite_show_do, a);
}
static void pm_core_sprite_hide_do(void *p) { SpriteHideC(*(int *)p); }
void pm_sprite_hide(int n) { pm_core_call(pm_core_sprite_hide_do, &n); }
static void pm_core_sprite_write_do(void *p)
{
    int *a = p;
    SpriteWriteC(a[0], a[1], a[2], a[3] == PM_DEFAULT_ARG ? -1 : a[3]);
}
void pm_sprite_write(int n, int x, int y, int orientation)
{
    int a[4] = {n, x, y, orientation};
    pm_core_call(pm_core_sprite_write_do, a);
}
static void pm_core_sprite_close_all_do(void *p) { (void)p; SpriteCloseAllC(); }
void pm_sprite_close_all(void) { pm_core_call(pm_core_sprite_close_all_do, NULL); }

static void pm_core_fb_copy_do(void *p)
{
    int *a = p;
    FrameBufferCopyC(pm_core_fb_letter(a[0]), pm_core_fb_letter(a[1]), a[2] != 0);
}
void pm_framebuffer_copy(int from, int to, int at_blank)
{
    int a[3] = {from, to, at_blank};
    pm_core_call(pm_core_fb_copy_do, a);
}

typedef struct { const int16_t *samples; unsigned frames, rate, channels; } pm_pcm_args;
static void pm_core_pcm_play_do(void *p)
{
    pm_pcm_args *a = p;
    PCMPlay(a->samples, a->frames, a->rate, a->channels);
}
void pm_pcm_play(const int16_t *samples, unsigned frames, unsigned rate, unsigned channels)
{
    pm_pcm_args a;
    a.samples = samples; a.frames = frames; a.rate = rate; a.channels = channels;
    pm_core_call(pm_core_pcm_play_do, &a);
}
static void pm_core_pcm_stop_do(void *p) { (void)p; PCMStop(); }
void pm_pcm_stop(void) { pm_core_call(pm_core_pcm_stop_do, NULL); }
int pm_pcm_playing(void) { return PCMPlaying(); }
static void pm_core_pcm_poll_do(void *p) { (void)p; PCMPoll(); }
void pm_pcm_poll(void) { pm_core_call(pm_core_pcm_poll_do, NULL); }

static int pm_core_colour_value(int c)
{
    extern const int CMM1map[16];
    if (CMM1) {
        pm_core_range(c, 0, 15);
        return CMM1map[c];
    }
    pm_core_range(c, 0, 0xFFFFFFF);
    return c;
}
static void pm_core_colour_do(void *p)
{
    int *a = p;
    gui_fcolour = pm_core_colour_value(a[0]);
    if (a[1] != PM_DEFAULT_ARG) gui_bcolour = pm_core_colour_value(a[1]);
    last_fcolour = gui_fcolour;
    last_bcolour = gui_bcolour;
    if (!CurrentLinePtr) { PromptFC = gui_fcolour; PromptBC = gui_bcolour; }
}
void pm_colour(int fc, int bc)
{
    int a[2] = {fc, bc};
    pm_core_call(pm_core_colour_do, a);
}

static void pm_core_mode_do(void *p)
{
#ifdef PICOMITEVGA
    int mode = *(int *)p;
    pm_core_range(mode, 1, MAXMODES);
    setmode(mode, true);
#if defined(USBKEYBOARD) && defined(GUICONTROLS) && defined(PICOMITEVGA)
    OSK_Invalidate();
#endif
#else
    (void)p;
    error("MODE is not available on this firmware");
#endif
}
void pm_mode(int mode) { pm_core_call(pm_core_mode_do, &mode); }
