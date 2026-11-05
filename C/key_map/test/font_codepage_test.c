/* font_codepage_test.c -- cp1251 font #10 smoke test for pico-mm/tcc.
 *
 * Install the corresponding DEFINEFONT #10 file in PicoMite's LIBRARY first:
 *   LIBRARY LOAD "picomite-cp1251-8x16.bas"
 *   RUN "font_codepage_test.c", "cp1251"
 *
 * This tests firmware pm_text() -> GUIPrintString(), not a second C renderer.
 * Extended characters are explicit 8-bit codepage bytes, NOT UTF-8.
 */
#include <picomite.h>
#include <picomite/graphics.h>
#include <string.h>

#define TEST_FONT 10
#define COLS      16
#define ROWS       8

/* Byte sequences for "Privet, mir!" in Cyrillic ("Привет, мир!"). */
static const char hello_cp1251[] = "Привет, мир!";

int main(int argc, char **argv)
{
    const char *hello;
    const char *label;
    int fw, fh, row, col;
    int first_y = 56;
    char glyphs[COLS + 1];
    char range[5];
    const char hex[] = "0123456789ABCDEF";

    hello = hello_cp1251;
    label = "cp1251";

    /* The font must be registered by BasicPrepareLibraryOnly(), called
       before RUN .c in core/Commands.c.  A missing font raises the normal
       PicoMite error "Invalid font #10" here instead of silently skipping. */
    pm_print("Checking LIBRARY DEFINEFONT #10...\r\n");
    fw = pm_font_width(TEST_FONT, 1);
    fh = pm_font_height(TEST_FONT, 1);
    if (fw != 8 || fh != 16) {
        pm_print("Font #10 is not 8x16\r\n");
        return 2;
    }
    pm_print("Font #10 is 8x16; rendering via pm_text()\r\n");

    pm_cls(PM_BLACK);
    pm_text(4, 4, label, "LT", 1, 1, PM_YELLOW, PM_BLACK);
    pm_text(4, 24, hello, "LT", TEST_FONT, 1, PM_WHITE, PM_BLACK);

    /* Display all 0x80..0xFF glyphs as 8 rows of 16 raw codepage bytes. */
    for (row = 0; row < ROWS; ++row) {
        int base = 0x80 + row * COLS;
        int y = first_y + row * fh;
        if (y + fh > pm_vres()) break;
        range[0] = hex[(base >> 4) & 15];
        range[1] = hex[base & 15];
        range[2] = ':';
        range[3] = '\0';
        for (col = 0; col < COLS; ++col)
            glyphs[col] = (char)(base + col);
        glyphs[COLS] = '\0';
        pm_text(4, y + 2, range, "LT", 1, 1, PM_CYAN, PM_BLACK);
        pm_text(32, y, glyphs, "LT", TEST_FONT, 1, PM_WHITE, PM_BLACK);
    }
    pm_refresh();
    pm_print("Rendered raw bytes 0x80..0xFF using pm_text()\r\n");
    return 0;
}
