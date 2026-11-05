/* PicoMite API v33: resident EN/RU keyboard remap, Windows-1251 output.
 * RP2350: LIBRARY LOAD uses TinyCC to compile this source in place.
 * RP2040: compile to ARMv6-M soft-float ET_REL and change the quoted
 * filename in key_rus.bas to key_rus.elf.
 *
 * CSUB command: KeyRus 0 = install, 1 = status, 2 = uninstall.
 * Ctrl+Shift (either side) switches EN <-> RU once per chord.
 * The remapper is IRQ-safe: no BASIC, I/O, allocation or waiting inside it.
 */
#include <stdint.h>
#include <picomite.h>
#include <picomite/input.h>
#include <picomite/resident.h>

#define KR_MAGIC 0x4B525553u /* 'KRUS' */
enum {
    KR_MODE = 0,       /* 0: original input; 1: Russian Windows-1251 */
    KR_CHORD,          /* Ctrl+Shift was already recognised */
    KR_PREVIOUS,       /* previous pm_remap_fn, stored as an ARM pointer */
    KR_SIGNATURE,
    KR_SWITCHES,
    KR_CAPS            /* physical Caps Lock state for punctuation keys */
};

#define KR_CTRL  (PM_KMOD_LCTRL  | PM_KMOD_RCTRL)
#define KR_SHIFT (PM_KMOD_LSHIFT | PM_KMOD_RSHIFT)
#define KR_ALT   (PM_KMOD_LALT   | PM_KMOD_RALT)
#define KR_GUI   (PM_KMOD_LGUI   | PM_KMOD_RGUI)

/* PS/2 set-2 physical scan -> HID keyboard usage. Unrecognised keys are
 * left alone, including extended E0 keys and numeric-keypad keys. */
static uint8_t ps2_to_hid(uint16_t key)
{
    switch (key) {
    case 0x1c: return 0x04; /* A */
    case 0x32: return 0x05; /* B */
    case 0x21: return 0x06; /* C */
    case 0x23: return 0x07; /* D */
    case 0x24: return 0x08; /* E */
    case 0x2b: return 0x09; /* F */
    case 0x34: return 0x0a; /* G */
    case 0x33: return 0x0b; /* H */
    case 0x43: return 0x0c; /* I */
    case 0x3b: return 0x0d; /* J */
    case 0x42: return 0x0e; /* K */
    case 0x4b: return 0x0f; /* L */
    case 0x3a: return 0x10; /* M */
    case 0x31: return 0x11; /* N */
    case 0x44: return 0x12; /* O */
    case 0x4d: return 0x13; /* P */
    case 0x15: return 0x14; /* Q */
    case 0x2d: return 0x15; /* R */
    case 0x1b: return 0x16; /* S */
    case 0x2c: return 0x17; /* T */
    case 0x3c: return 0x18; /* U */
    case 0x2a: return 0x19; /* V */
    case 0x1d: return 0x1a; /* W */
    case 0x22: return 0x1b; /* X */
    case 0x35: return 0x1c; /* Y */
    case 0x1a: return 0x1d; /* Z */
    case 0x16: return 0x1e; /* 1 */
    case 0x1e: return 0x1f; /* 2 */
    case 0x26: return 0x20; /* 3 */
    case 0x25: return 0x21; /* 4 */
    case 0x2e: return 0x22; /* 5 */
    case 0x36: return 0x23; /* 6 */
    case 0x3d: return 0x24; /* 7 */
    case 0x3e: return 0x25; /* 8 */
    case 0x46: return 0x26; /* 9 */
    case 0x45: return 0x27; /* 0 */
    case 0x54: return 0x2f; /* [ */
    case 0x5b: return 0x30; /* ] */
    case 0x4c: return 0x33; /* ; */
    case 0x52: return 0x34; /* ' */
    case 0x0e: return 0x35; /* ` */
    case 0x41: return 0x36; /* , */
    case 0x49: return 0x37; /* . */
    case 0x4a: return 0x38; /* / */
    default:   return 0;
    }
}

/* Lowercase CP1251 corresponding to physical HID A..Z (QWERTY -> JCUKEN).
 * Uppercase А..Я is always lowercase byte - 0x20. */
static const uint8_t rus_letters[26] = {
    0xf4, /* A: ф */ 0xe8, /* B: и */ 0xf1, /* C: с */
    0xe2, /* D: в */ 0xf3, /* E: у */ 0xe0, /* F: а */
    0xef, /* G: п */ 0xf0, /* H: р */ 0xf8, /* I: ш */
    0xee, /* J: о */ 0xeb, /* K: л */ 0xe4, /* L: д */
    0xfc, /* M: ь */ 0xf2, /* N: т */ 0xf9, /* O: щ */
    0xe7, /* P: з */ 0xe9, /* Q: й */ 0xea, /* R: к */
    0xfb, /* S: ы */ 0xe5, /* T: е */ 0xe3, /* U: г */
    0xec, /* V: м */ 0xf6, /* W: ц */ 0xf7, /* X: ч */
    0xed, /* Y: н */ 0xff  /* Z: я */
};

/* Returns the original byte for keys outside the Russian layout. */
static uint8_t rus_code(uint8_t code, uint8_t hid, uint8_t mods,
                        uint32_t caps)
{
    int shifted = (mods & KR_SHIFT) != 0;
    if (hid >= 0x04 && hid <= 0x1d) {
        uint8_t result = rus_letters[hid - 0x04];
        /* The existing layout has already applied Shift and Caps Lock to
         * alphabetic keys. Don't guess the active native layout here. */
        if (code >= 'A' && code <= 'Z')
            result -= 0x20;
        return result;
    }
    if (hid >= 0x1e && hid <= 0x27) {
        /* Standard Russian shifted numeral row; normal digits unchanged. */
        if (!shifted) return code;
        switch (hid) {
        case 0x1f: return '"';  /* Shift+2 */
        case 0x20: return 0xb9; /* Shift+3: № */
        case 0x21: return ';';  /* Shift+4 */
        case 0x23: return ':';  /* Shift+6 */
        case 0x24: return '?';  /* Shift+7 */
        default: return code;
        }
    }
    /* Caps Lock also capitalises Russian letters on punctuation keys. */
    int upper = (shifted != 0) ^ (caps != 0);
    uint8_t result;
    switch (hid) {
    case 0x2f: result = 0xf5; break; /* [ : х */
    case 0x30: result = 0xfa; break; /* ] : ъ */
    case 0x33: result = 0xe6; break; /* ; : ж */
    case 0x34: result = 0xfd; break; /* ' : э */
    case 0x35: return upper ? 0xa8 : 0xb8; /* ` : Ё / ё */
    case 0x36: result = 0xe1; break; /* , : б */
    case 0x37: result = 0xfe; break; /* . : ю */
    case 0x38: return shifted ? ',' : '.'; /* / : punctuation */
    default: return code;
    }
    return upper ? (uint8_t)(result - 0x20) : result;
}

/* This is called directly from physical keyboard processing (PS/2 in IRQ).
 * Only read/update the resident's own fixed words. */
static uint8_t keyrus_remap(uint8_t code, uint16_t key,
                            uint8_t modifiers, uint8_t flags)
{
    uint32_t *s = pm_resident_state();
    if (!s || s[KR_SIGNATURE] != KR_MAGIC)
        return code;

    /* Modifier notifications arrive even without an output character.
     * A latch ensures only one switch per held Ctrl+Shift chord. */
    int chord = (modifiers & KR_CTRL) && (modifiers & KR_SHIFT);
    if (!chord) {
        s[KR_CHORD] = 0;
    } else if (!s[KR_CHORD]) {
        s[KR_CHORD] = 1;
        s[KR_MODE] ^= 1u;
        ++s[KR_SWITCHES];
    }

    /* Maintain Caps Lock for the Russian punctuation positions. */
    if ((flags & PM_REMAP_DOWN) && !(flags & PM_REMAP_REPEAT)) {
        if ((flags & PM_REMAP_PS2) ? (key == 0x58) : (key == 0x39))
            s[KR_CAPS] ^= 1u;
    }

    if (!(flags & PM_REMAP_EMIT) || !s[KR_MODE] ||
        (flags & PM_REMAP_MEDIA) ||
        (modifiers & (KR_CTRL | KR_ALT | KR_GUI)))
        return code;

    uint8_t hid;
    if (flags & PM_REMAP_PS2) {
        hid = ps2_to_hid(key);
    } else if (key <= 0xffu) {
        hid = (uint8_t)key;
    } else {
        return code;
    }
    return rus_code(code, hid, modifiers, s[KR_CAPS]);
}

/* pm_resident_start() keeps a Flash LIBRARY CSUB protected against rewrite.
 * The keyboard callback itself is independent of this cooperative timer. */
static void keyrus_service(void *state)
{
    (void)state;
}

/* Flash LIBRARY CSUB entry: command is MMBasic INTEGER (64-bit pointer). */
void KeyRus(long long *command)
{
    uint32_t *s;
    if (!command) return;
    switch (*command) {
    case 0: /* install */
        if (pm_resident_active()) {
            s = pm_resident_state();
            pm_print(s && s[KR_SIGNATURE] == KR_MAGIC
                     ? "KEY RUS: ALREADY INSTALLED\r\n"
                     : "KEY RUS: ANOTHER RESIDENT IS ACTIVE\r\n");
            return;
        }
        if (pm_resident_start(keyrus_service, 60000u) != 0) {
            pm_print("KEY RUS: INSTALL REFUSED (FLASH LIBRARY REQUIRED)\r\n");
            return;
        }
        s = pm_resident_state();
        if (!s) {
            (void)pm_resident_stop();
            pm_print("KEY RUS: STATE UNAVAILABLE\r\n");
            return;
        }
        s[KR_MODE] = 0;
        s[KR_CHORD] = 0;
        s[KR_SWITCHES] = 0;
        s[KR_CAPS] = (uint32_t)(pm_keydown(8) & 1);
        s[KR_SIGNATURE] = KR_MAGIC;
        s[KR_PREVIOUS] = (uint32_t)(uintptr_t)pm_set_remap_fn(keyrus_remap);
        pm_print("KEY RUS: INSTALLED, EN (CTRL+SHIFT TO SWITCH)\r\n");
        return;
    case 1: /* status */
        s = pm_resident_state();
        if (!pm_resident_active() || !s || s[KR_SIGNATURE] != KR_MAGIC) {
            pm_print("KEY RUS: NOT INSTALLED\r\n");
            return;
        }
        pm_print(s[KR_MODE] ? "KEY RUS: RU (CP1251)\r\n" : "KEY RUS: EN\r\n");
        return;
    case 2: /* uninstall: the resident restores its own resource */
        s = pm_resident_state();
        if (!pm_resident_active() || !s || s[KR_SIGNATURE] != KR_MAGIC) {
            pm_print("KEY RUS: NOT INSTALLED\r\n");
            return;
        }
        {
            pm_remap_fn previous = (pm_remap_fn)(uintptr_t)s[KR_PREVIOUS];
            pm_remap_fn installed = pm_set_remap_fn(previous);
            if (installed != keyrus_remap) {
                /* Someone else replaced our callback; do not remove it. */
                (void)pm_set_remap_fn(installed);
                pm_print("KEY RUS: REMAP OWNERSHIP CHANGED, STOP REFUSED\r\n");
                return;
            }
        }
        s[KR_SIGNATURE] = 0;
        if (pm_resident_stop() != 0) {
            s[KR_SIGNATURE] = KR_MAGIC;
            (void)pm_set_remap_fn(keyrus_remap);
            pm_print("KEY RUS: RESIDENT STOP REFUSED\r\n");
            return;
        }
        pm_print("KEY RUS: STOPPED; PREVIOUS REMAP RESTORED\r\n");
        return;
    default:
        pm_print("KEY RUS: USE 0=INSTALL, 1=STATUS, 2=STOP\r\n");
        return;
    }
}
