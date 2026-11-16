/* hdmi.h: frank-nes's soft framebuffer display (drivers/hdmi_pio), which nes_cmm2.c makes the CMM2's MODE 7,8 */
#ifndef CMM2_HDMI_H
#define CMM2_HDMI_H
#include "pico/stdlib.h"

extern bool SELECT_VGA;
extern int graphics_buffer_width, graphics_buffer_height;
void graphics_init(void);
void graphics_set_buffer(uint8_t *buffer);
void graphics_set_offset(int x, int y);
void graphics_set_palette(uint8_t i, uint32_t rgb);
void graphics_set_palette_hdmi(uint8_t i, uint32_t rgb);
void graphics_restore_sync_colors(void);        /* (each palette's last call: nes_cmm2.c shows it then) */
void graphics_set_mode(int mode);
uint8_t testPins(uint32_t a, uint32_t b);
#endif
