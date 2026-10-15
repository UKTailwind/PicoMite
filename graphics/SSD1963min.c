#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

#if defined(PICOMITEMIN)
static void WriteColorDisabled(unsigned int c)
{
    (void)c;
}

void (*WriteColor)(unsigned int c) = WriteColorDisabled;

void InitSSD1963(void)
{
}

void InitDisplaySSD(void)
{
}

/* OPTION LCDPANEL offers the panel name to each ConfigDisplay* in turn and
   each returns, leaving DISPLAY_TYPE at 0, when the name is not its own.
   So refuse only the names the full driver (SSD1963.c) accepts: anything
   else, SSD1306I2C included, goes on to ConfigDisplayI2C. */
static const char *const SSDnames[] = {
    "SSD1963_4", "SSD1963_5", "SSD1963_5A", "SSD1963_7", "SSD1963_7A", "SSD1963_8",
    "ILI9341_8", "SSD1963_4_16", "SSD1963_5_16",
#if PICOMITERP2350
    "SSD1963_5_12BUFF", "SSD1963_5_16BUFF", "SSD1963_5_BUFF",
    "SSD1963_7_12BUFF", "SSD1963_7_16BUFF", "SSD1963_7_BUFF",
#endif
    "SSD1963_5A_16", "SSD1963_7_16", "SSD1963_7A_16", "SSD1963_8_16",
    "ILI9341_16", "ILI9486_16", "IPS_4_16"};

void ConfigDisplaySSD(unsigned char *p)
{
    getcsargs(&p, 13);
    if ((argc & 1) != 1 || argc < 3)
        StandardError(2);
    for (int i = 0; i < (int)(sizeof(SSDnames) / sizeof(SSDnames[0])); i++)
        if (checkstring(argv[0], (unsigned char *)SSDnames[i]))
            error("SSD1963 displays not enabled");
}

void SetBacklightSSD1963(int intensity)
{
    (void)intensity;
}

void SetTearingCfg(int state, int mode)
{
    (void)state;
    (void)mode;
}

void ScrollSSD1963(int lines)
{
    (void)lines;
}

void WriteComand(int cmd)
{
    (void)cmd;
}

void WriteData(int data)
{
    (void)data;
}

void WriteData16bit(int data)
{
    (void)data;
}

void Write16bitCommand(int cmd)
{
    (void)cmd;
}

void SetAreaSSD1963(int x1, int y1, int x2, int y2)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
}

void SetAreaILI9341(int xstart, int ystart, int xend, int yend, int rw)
{
    (void)xstart;
    (void)ystart;
    (void)xend;
    (void)yend;
    (void)rw;
}

void SetAreaIPS_4_16(int xstart, int ystart, int xend, int yend, int rw)
{
    (void)xstart;
    (void)ystart;
    (void)xend;
    (void)yend;
    (void)rw;
}

void WriteCmdDataIPS_4_16(int cmd, int n, int data)
{
    (void)cmd;
    (void)n;
    (void)data;
}

void DrawBitmapSSD1963(int x1, int y1, int width, int height, int scale, int fg, int bg, unsigned char *bitmap)
{
    (void)x1;
    (void)y1;
    (void)width;
    (void)height;
    (void)scale;
    (void)fg;
    (void)bg;
    (void)bitmap;
}

void DrawBufferSSD1963(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}

void DrawBLITBufferSSD1963(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}

void DrawRectangleSSD1963(int x1, int y1, int x2, int y2, int c)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)c;
}

void ReadBufferSSD1963(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}

void ReadBLITBufferSSD1963(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}

void DrawBitmap320(int x1, int y1, int width, int height, int scale, int fg, int bg, unsigned char *bitmap)
{
    (void)x1;
    (void)y1;
    (void)width;
    (void)height;
    (void)scale;
    (void)fg;
    (void)bg;
    (void)bitmap;
}

void DrawBuffer320(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}

void DrawBLITBuffer320(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}

void DrawRectangle320(int x1, int y1, int x2, int y2, int c)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)c;
}

void ReadBuffer320(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}

void ReadBLITBuffer320(int x1, int y1, int x2, int y2, unsigned char *p)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)p;
}
#endif