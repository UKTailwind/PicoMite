/* CMM2 (apps/scummvm/overlay): frank-quest's file at 949301b, but no welcome when RUN named a game. */
/*
 * FRANK Quest
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Demoscene-style cold-boot intro.
 *
 * Layers (back to front):
 *   1. Vertical sky gradient (8 palette slots, top-to-horizon).
 *   2. Three parallax mountain silhouettes, sine-wave horizons,
 *      different speeds and amplitudes, drawn back-to-front.
 *   3. Centered "FRANK QUEST" title in the 6x8 font from frank-msx,
 *      4x-scaled with a per-row vertical color gradient and a hard
 *      drop shadow.
 *   4. Horizontal sine-wobbled ticker at the bottom.
 *
 * Any keypress or mouse-button press dismisses the welcome
 * immediately. Stale events from boot are drained once at entry so a
 * buffered press doesn't skip the screen on frame 1. The intro
 * fall-in, ticker, and outro are all skippable.
 *
 * Font glyphs are traced from frank-msx's ui_font_6x8 (which itself
 * was traced from murmapple's disk_ui), so the title visually matches
 * the FRANK MSX boot screen as the user requested.
 */

#include "frank_quest_selector.h"

#include "backends/platform/rp2350/rp2350-system.h"
#include "backends/platform/rp2350/rp2350-minimal.h"

#include "common/events.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "graphics/palette.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cmm2.h"                                // CMM2: cmm2_start

extern OSystem *g_system;

namespace {

// ---- 6x8 font (ported from frank-msx ui_font_6x8) -----------------
//
// Each glyph is 8 rows; only the top 6 bits of each byte are used. ASCII
// 32..126 inclusive (95 glyphs). Index = ch - 32.
constexpr int kFontGlyphs    = 95;
constexpr int kFontCellW     = 6;
constexpr int kFontCellH     = 8;

const uint8_t kFont6x8[kFontGlyphs][kFontCellH] = {
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*   */
	{0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x00}, /* ! */
	{0x50,0x50,0x50,0x00,0x00,0x00,0x00,0x00}, /* " */
	{0x50,0x50,0xF8,0x50,0xF8,0x50,0x50,0x00}, /* # */
	{0x20,0x78,0xA0,0x70,0x28,0xF0,0x20,0x00}, /* $ */
	{0xC0,0xC8,0x10,0x20,0x40,0x98,0x18,0x00}, /* % */
	{0x40,0xA0,0xA0,0x40,0xA8,0x90,0x68,0x00}, /* & */
	{0x20,0x20,0x40,0x00,0x00,0x00,0x00,0x00}, /* ' */
	{0x10,0x20,0x40,0x40,0x40,0x20,0x10,0x00}, /* ( */
	{0x40,0x20,0x10,0x10,0x10,0x20,0x40,0x00}, /* ) */
	{0x00,0x20,0xA8,0x70,0xA8,0x20,0x00,0x00}, /* * */
	{0x00,0x20,0x20,0xF8,0x20,0x20,0x00,0x00}, /* + */
	{0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x40}, /* , */
	{0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00}, /* - */
	{0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00}, /* . */
	{0x00,0x08,0x10,0x20,0x40,0x80,0x00,0x00}, /* / */
	{0x70,0x88,0x98,0xA8,0xC8,0x88,0x70,0x00}, /* 0 */
	{0x20,0x60,0x20,0x20,0x20,0x20,0x70,0x00}, /* 1 */
	{0x70,0x88,0x08,0x30,0x40,0x80,0xF8,0x00}, /* 2 */
	{0x70,0x88,0x08,0x30,0x08,0x88,0x70,0x00}, /* 3 */
	{0x10,0x30,0x50,0x90,0xF8,0x10,0x10,0x00}, /* 4 */
	{0xF8,0x80,0xF0,0x08,0x08,0x88,0x70,0x00}, /* 5 */
	{0x30,0x40,0x80,0xF0,0x88,0x88,0x70,0x00}, /* 6 */
	{0xF8,0x08,0x10,0x20,0x40,0x40,0x40,0x00}, /* 7 */
	{0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00}, /* 8 */
	{0x70,0x88,0x88,0x78,0x08,0x10,0x60,0x00}, /* 9 */
	{0x00,0x00,0x20,0x00,0x00,0x20,0x00,0x00}, /* : */
	{0x00,0x00,0x20,0x00,0x00,0x20,0x20,0x40}, /* ; */
	{0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x00}, /* < */
	{0x00,0x00,0xF8,0x00,0xF8,0x00,0x00,0x00}, /* = */
	{0x40,0x20,0x10,0x08,0x10,0x20,0x40,0x00}, /* > */
	{0x70,0x88,0x10,0x20,0x20,0x00,0x20,0x00}, /* ? */
	{0x70,0x88,0xB8,0xA8,0xB8,0x80,0x70,0x00}, /* @ */
	{0x70,0x88,0x88,0xF8,0x88,0x88,0x88,0x00}, /* A */
	{0xF0,0x88,0x88,0xF0,0x88,0x88,0xF0,0x00}, /* B */
	{0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00}, /* C */
	{0xE0,0x90,0x88,0x88,0x88,0x90,0xE0,0x00}, /* D */
	{0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8,0x00}, /* E */
	{0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0x00}, /* F */
	{0x70,0x88,0x80,0xB8,0x88,0x88,0x70,0x00}, /* G */
	{0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x00}, /* H */
	{0x70,0x20,0x20,0x20,0x20,0x20,0x70,0x00}, /* I */
	{0x38,0x10,0x10,0x10,0x90,0x90,0x60,0x00}, /* J */
	{0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88,0x00}, /* K */
	{0x80,0x80,0x80,0x80,0x80,0x80,0xF8,0x00}, /* L */
	{0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88,0x00}, /* M */
	{0x88,0xC8,0xA8,0x98,0x88,0x88,0x88,0x00}, /* N */
	{0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00}, /* O */
	{0xF0,0x88,0x88,0xF0,0x80,0x80,0x80,0x00}, /* P */
	{0x70,0x88,0x88,0x88,0xA8,0x90,0x68,0x00}, /* Q */
	{0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88,0x00}, /* R */
	{0x70,0x88,0x80,0x70,0x08,0x88,0x70,0x00}, /* S */
	{0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0x00}, /* T */
	{0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00}, /* U */
	{0x88,0x88,0x88,0x88,0x50,0x50,0x20,0x00}, /* V */
	{0x88,0x88,0x88,0xA8,0xA8,0xD8,0x88,0x00}, /* W */
	{0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00}, /* X */
	{0x88,0x88,0x50,0x20,0x20,0x20,0x20,0x00}, /* Y */
	{0xF8,0x08,0x10,0x20,0x40,0x80,0xF8,0x00}, /* Z */
	{0x70,0x40,0x40,0x40,0x40,0x40,0x70,0x00}, /* [ */
	{0x00,0x80,0x40,0x20,0x10,0x08,0x00,0x00}, /* \ */
	{0x70,0x10,0x10,0x10,0x10,0x10,0x70,0x00}, /* ] */
	{0x20,0x50,0x88,0x00,0x00,0x00,0x00,0x00}, /* ^ */
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF8}, /* _ */
	{0x40,0x20,0x10,0x00,0x00,0x00,0x00,0x00}, /* ` */
	{0x00,0x00,0x70,0x08,0x78,0x88,0x78,0x00}, /* a */
	{0x80,0x80,0xB0,0xC8,0x88,0xC8,0xB0,0x00}, /* b */
	{0x00,0x00,0x70,0x80,0x80,0x88,0x70,0x00}, /* c */
	{0x08,0x08,0x68,0x98,0x88,0x98,0x68,0x00}, /* d */
	{0x00,0x00,0x70,0x88,0xF8,0x80,0x70,0x00}, /* e */
	{0x30,0x48,0x40,0xE0,0x40,0x40,0x40,0x00}, /* f */
	{0x00,0x00,0x68,0x98,0x98,0x68,0x08,0x70}, /* g */
	{0x80,0x80,0xB0,0xC8,0x88,0x88,0x88,0x00}, /* h */
	{0x20,0x00,0x60,0x20,0x20,0x20,0x70,0x00}, /* i */
	{0x10,0x00,0x30,0x10,0x10,0x90,0x60,0x00}, /* j */
	{0x80,0x80,0x90,0xA0,0xC0,0xA0,0x90,0x00}, /* k */
	{0x60,0x20,0x20,0x20,0x20,0x20,0x70,0x00}, /* l */
	{0x00,0x00,0xD0,0xA8,0xA8,0xA8,0xA8,0x00}, /* m */
	{0x00,0x00,0xB0,0xC8,0x88,0x88,0x88,0x00}, /* n */
	{0x00,0x00,0x70,0x88,0x88,0x88,0x70,0x00}, /* o */
	{0x00,0x00,0xB0,0xC8,0xC8,0xB0,0x80,0x80}, /* p */
	{0x00,0x00,0x68,0x98,0x98,0x68,0x08,0x08}, /* q */
	{0x00,0x00,0xB0,0xC8,0x80,0x80,0x80,0x00}, /* r */
	{0x00,0x00,0x78,0x80,0x70,0x08,0xF0,0x00}, /* s */
	{0x40,0x40,0xE0,0x40,0x40,0x48,0x30,0x00}, /* t */
	{0x00,0x00,0x88,0x88,0x88,0x98,0x68,0x00}, /* u */
	{0x00,0x00,0x88,0x88,0x88,0x50,0x20,0x00}, /* v */
	{0x00,0x00,0x88,0xA8,0xA8,0xA8,0x50,0x00}, /* w */
	{0x00,0x00,0x88,0x50,0x20,0x50,0x88,0x00}, /* x */
	{0x00,0x00,0x88,0x88,0x98,0x68,0x08,0x70}, /* y */
	{0x00,0x00,0xF8,0x10,0x20,0x40,0xF8,0x00}, /* z */
	{0x10,0x20,0x20,0x40,0x20,0x20,0x10,0x00}, /* { */
	{0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00}, /* | */
	{0x40,0x20,0x20,0x10,0x20,0x20,0x40,0x00}, /* } */
	{0x00,0x00,0x40,0xA8,0x10,0x00,0x00,0x00}, /* ~ */
};

const uint8_t *glyph6x8(char ch) {
	if ((unsigned char)ch < 32 || (unsigned char)ch > 126)
		return kFont6x8[0];
	return kFont6x8[(unsigned char)ch - 32];
}

// ---- Sine table ---------------------------------------------------
//
// 256 entries covering one full period. Values are signed 8-bit so they
// fit in (-127..127). Used for: parallax horizon waves, ticker bobbing.
constexpr int kSinSize = 256;
int8_t kSinTab[kSinSize];

void initSinTable() {
	// Approx sine via 4-term polynomial Bhaskara — keeps everything int.
	// Good enough for visual wobble; we only use it for screen-space y
	// offsets in the 0..3 px range.
	//
	// sin(x) ≈ 16x(π - x) / (5π² - 4x(π - x)) in [0,π].
	// We compute it via integer math by mapping i ∈ [0,256) to angle.
	for (int i = 0; i < kSinSize; ++i) {
		// Use float here — runs once at boot, doesn't hurt.
		double t   = (double)i / kSinSize * 6.28318530718;
		double s   = 0.0;
		// Tiny Taylor for sin around 0; reduce range first.
		double tw  = t;
		while (tw >  3.14159265) tw -= 6.28318530718;
		while (tw < -3.14159265) tw += 6.28318530718;
		double tw2 = tw * tw;
		s = tw - tw*tw2/6.0 + tw*tw2*tw2/120.0
		    - tw*tw2*tw2*tw2/5040.0;
		int   v = (int)(s * 127.0);
		if (v >  127) v =  127;
		if (v < -127) v = -127;
		kSinTab[i] = (int8_t)v;
	}
}

inline int isin(int phase) {
	return kSinTab[((unsigned)phase) & (kSinSize - 1)];
}

// ---- Palette ------------------------------------------------------
//
// Layout (35 slots in use; selector reinstalls 0..5 when it takes over).
//
//   0        : pure black (drop shadow / void)
//   1..16    : sky gradient, top → horizon (16 bands, dithered)
//   17       : far mountain (mid violet)
//   18       : mid mountain (deep violet)
//   19       : near mountain (near-black with blue tint)
//   20..27   : title vertical gradient (white → yellow → orange →
//              red → magenta) — 8 bands across 32 px scaled glyph
//   28       : ticker text (white)
//   29       : ticker glow (warm)
constexpr int kSkyBands = 16;
enum : uint8_t {
	kColShadow      = 0,
	kColSky0        = 1,
	kColSkyLast     = kColSky0 + kSkyBands - 1,   // 16
	kColMtnFar      = 17,
	kColMtnMid      = 18,
	kColMtnNear     = 19,
	kColTitle0      = 20,
	kColTitleLast   = 27,
	kColTicker      = 28,
	kColTickerGlow  = 29,
};

constexpr int kPaletteCount = 30;

// 8-stop hand-tuned dusk gradient. installPalette() interpolates this
// to kSkyBands palette slots so the sky reads smoothly across the
// upper screen instead of as visible stripes.
const uint8_t kSkyKeyR[8] = { 0x10, 0x28, 0x4a, 0x76, 0xa2, 0xcc, 0xee, 0xff };
const uint8_t kSkyKeyG[8] = { 0x14, 0x18, 0x20, 0x2a, 0x44, 0x6e, 0xa4, 0xd8 };
const uint8_t kSkyKeyB[8] = { 0x40, 0x58, 0x66, 0x70, 0x70, 0x66, 0x60, 0x88 };

void installPalette() {
	byte pal[kPaletteCount * 3];
	memset(pal, 0, sizeof(pal));

	// 0: black (drop shadow + outline).
	pal[0] = 0; pal[1] = 0; pal[2] = 0;

	// Sky: linear interpolation between the 8 key stops, expanded to
	// 16 palette slots. This alone halves the visible banding;
	// drawSky() then dithers between adjacent slots to dissolve what's
	// left.
	for (int i = 0; i < kSkyBands; ++i) {
		const int t        = i * (8 - 1);            // 0..15 → 0..15 over 7 segments
		const int seg      = t / (kSkyBands - 1);    // 0..6
		const int rem      = t - seg * (kSkyBands - 1);
		const int span     = kSkyBands - 1;
		const int r = kSkyKeyR[seg] + (kSkyKeyR[seg + 1] - kSkyKeyR[seg]) * rem / span;
		const int g = kSkyKeyG[seg] + (kSkyKeyG[seg + 1] - kSkyKeyG[seg]) * rem / span;
		const int b = kSkyKeyB[seg] + (kSkyKeyB[seg + 1] - kSkyKeyB[seg]) * rem / span;
		const int s = (kColSky0 + i) * 3;
		pal[s + 0] = (uint8_t)r;
		pal[s + 1] = (uint8_t)g;
		pal[s + 2] = (uint8_t)b;
	}

	// Mountain layers. Each darker than the last so the parallax reads
	// as depth.
	pal[kColMtnFar  * 3 + 0] = 0x40; pal[kColMtnFar  * 3 + 1] = 0x20; pal[kColMtnFar  * 3 + 2] = 0x50;
	pal[kColMtnMid  * 3 + 0] = 0x20; pal[kColMtnMid  * 3 + 1] = 0x10; pal[kColMtnMid  * 3 + 2] = 0x30;
	pal[kColMtnNear * 3 + 0] = 0x08; pal[kColMtnNear * 3 + 1] = 0x06; pal[kColMtnNear * 3 + 2] = 0x18;

	// Title gradient (top to bottom): classic demoscene fire.
	const uint8_t titR[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xa8, 0x60 };
	const uint8_t titG[8] = { 0xff, 0xf4, 0xd0, 0x90, 0x40, 0x18, 0x00, 0x00 };
	const uint8_t titB[8] = { 0xff, 0x80, 0x20, 0x00, 0x00, 0x00, 0x40, 0x80 };
	for (int i = 0; i < 8; ++i) {
		const int s = (kColTitle0 + i) * 3;
		pal[s + 0] = titR[i];
		pal[s + 1] = titG[i];
		pal[s + 2] = titB[i];
	}

	// Ticker text + warm glow underline.
	pal[kColTicker     * 3 + 0] = 0xff;
	pal[kColTicker     * 3 + 1] = 0xff;
	pal[kColTicker     * 3 + 2] = 0xff;
	pal[kColTickerGlow * 3 + 0] = 0xff;
	pal[kColTickerGlow * 3 + 1] = 0xa0;
	pal[kColTickerGlow * 3 + 2] = 0x10;

	g_system->getPaletteManager()->setPalette(pal, 0, kPaletteCount);
}

// Mountain colors at full opacity (matches installPalette()). Kept in
// one place so the outro fade has a known starting point.
constexpr uint8_t kMtnFarRGB[3]  = { 0x40, 0x20, 0x50 };
constexpr uint8_t kMtnMidRGB[3]  = { 0x20, 0x10, 0x30 };
constexpr uint8_t kMtnNearRGB[3] = { 0x08, 0x06, 0x18 };

// Fade the three mountain palette slots toward the horizon sky color
// (the warm band where mountains meet the sky), parameterized by
// `t` = 0..256 where 0 is fully visible and 256 is fully merged.
//
// We bias the target toward the highest sky band (the warm
// orange/pink near the horizon) so the silhouettes dissolve into the
// sunset rather than just dimming to black — that's what reads as the
// mountains "fading away" into the sky.
void fadeMountains(int t) {
	if (t < 0)   t = 0;
	if (t > 256) t = 256;
	const uint8_t targetR = kSkyKeyR[7];
	const uint8_t targetG = kSkyKeyG[7];
	const uint8_t targetB = kSkyKeyB[7];

	auto mix = [&](const uint8_t src[3], uint8_t out[3]) {
		out[0] = (uint8_t)(src[0] + ((targetR - src[0]) * t) / 256);
		out[1] = (uint8_t)(src[1] + ((targetG - src[1]) * t) / 256);
		out[2] = (uint8_t)(src[2] + ((targetB - src[2]) * t) / 256);
	};

	byte rgb[9];
	mix(kMtnFarRGB,  rgb + 0);
	mix(kMtnMidRGB,  rgb + 3);
	mix(kMtnNearRGB, rgb + 6);
	g_system->getPaletteManager()->setPalette(rgb, kColMtnFar, 3);
}

// ---- Low-level draw helpers ---------------------------------------

inline void putPixel(Graphics::Surface *surf, int x, int y, uint8_t color) {
	if ((unsigned)x >= (unsigned)surf->getWidth())  return;
	if ((unsigned)y >= (unsigned)surf->getHeight()) return;
	*(uint8_t *)surf->getBasePtr(x, y) = color;
}

void hline(Graphics::Surface *surf, int x, int y, int w, uint8_t color) {
	const int sw = surf->getWidth();
	if (y < 0 || y >= surf->getHeight()) return;
	if (x < 0) { w += x; x = 0; }
	if (x + w > sw) w = sw - x;
	if (w <= 0) return;
	memset(surf->getBasePtr(x, y), color, w);
}

void vlineFromY(Graphics::Surface *surf, int x, int y0, int y1, uint8_t color) {
	if ((unsigned)x >= (unsigned)surf->getWidth()) return;
	const int sh = surf->getHeight();
	if (y0 < 0)  y0 = 0;
	if (y1 > sh) y1 = sh;
	for (int y = y0; y < y1; ++y) {
		*(uint8_t *)surf->getBasePtr(x, y) = color;
	}
}

// Draw a glyph at the given scale. `colorRows[]` is indexed by the
// destination row offset within the scaled glyph (0..rows*scale - 1) so
// we can apply a vertical color gradient. Pass a single-color array
// (broadcasted) for non-gradient draws.
void drawGlyphScaled(Graphics::Surface *surf, int x, int y, char ch,
                     int scale, const uint8_t *colorRows) {
	const uint8_t *g = glyph6x8(ch);
	const int sw = surf->getWidth();
	const int sh = surf->getHeight();
	for (int row = 0; row < kFontCellH; ++row) {
		const uint8_t bits = g[row];
		for (int col = 0; col < kFontCellW; ++col) {
			if (!(bits & (0x80 >> col))) continue;
			for (int dy = 0; dy < scale; ++dy) {
				const int py = y + row * scale + dy;
				if (py < 0 || py >= sh) continue;
				const uint8_t color =
				    colorRows[row * scale + dy];
				const int pxBase = x + col * scale;
				for (int dx = 0; dx < scale; ++dx) {
					const int px = pxBase + dx;
					if (px < 0 || px >= sw) continue;
					*(uint8_t *)surf->getBasePtr(px, py) = color;
				}
			}
		}
	}
}

// 1x draw of the 6x8 font in a single color.
void drawText(Graphics::Surface *surf, int x, int y, const char *s,
              uint8_t color) {
	for (; *s; ++s) {
		const uint8_t *gr = glyph6x8(*s);
		for (int row = 0; row < kFontCellH; ++row) {
			const uint8_t bits = gr[row];
			const int py = y + row;
			if ((unsigned)py >= (unsigned)surf->getHeight()) continue;
			for (int col = 0; col < kFontCellW; ++col) {
				if (!(bits & (0x80 >> col))) continue;
				putPixel(surf, x + col, py, color);
			}
		}
		x += kFontCellW;
	}
}

// ---- Sky gradient -------------------------------------------------

constexpr int kSkyTop    = 0;
constexpr int kSkyBottom = 130;   // below this, mountains take over

// 4x4 Bayer matrix (values 0..15). Used to dither between two adjacent
// palette bands so the sky transitions look continuous despite the
// 16-band palette.
constexpr uint8_t kBayer4[4][4] = {
	{  0,  8,  2, 10 },
	{ 12,  4, 14,  6 },
	{  3, 11,  1,  9 },
	{ 15,  7, 13,  5 },
};

void drawSky(Graphics::Surface *surf) {
	const int SW = surf->getWidth();
	const int bandH = 16;   // fixed-point precision: 16 sub-bands per palette band
	for (int y = kSkyTop; y < kSkyBottom; ++y) {
		// Map y to a 16.4 fixed-point position within the 16-band sky.
		// `pos` ranges 0..(kSkyBands-1)*16 - 1; high 4 bits = band
		// index, low 4 bits = dither weight to the next band.
		const int pos = (y - kSkyTop) * (kSkyBands - 1) * bandH /
		                (kSkyBottom - kSkyTop);
		int bandLo  = pos / bandH;
		int weight  = pos - bandLo * bandH;        // 0..15 → use as Bayer threshold
		if (bandLo >= kSkyBands - 1) {
			bandLo = kSkyBands - 1;
			weight = 0;
		}
		const uint8_t cLo = (uint8_t)(kColSky0 + bandLo);
		const uint8_t cHi = (uint8_t)(kColSky0 + bandLo + (weight > 0 ? 1 : 0));

		uint8_t *row = (uint8_t *)surf->getBasePtr(0, y);
		const uint8_t *bayerRow = kBayer4[y & 3];
		for (int x = 0; x < SW; ++x) {
			// Pixel goes to the higher (next) band when its Bayer
			// threshold is below the weight. Yields a clean 16-step
			// dither pattern between bands.
			row[x] = (weight > bayerRow[x & 3]) ? cHi : cLo;
		}
	}
}

// ---- Parallax mountains -------------------------------------------
//
// Each layer's horizon is a sum of two sines. Phase advances per frame
// at the layer's speed. For each x, we compute the horizon y and fill
// from there to the bottom of the screen with the layer's color.
//
// Layers drawn back-to-front:
//   far : horizon ~110, big slow waves
//   mid : horizon ~140, faster + smaller
//   near: horizon ~170, fastest + small
//
// "Slow" / "fast" here is relative to frame count, giving the parallax
// illusion the user asked for.

// Peak heights (in pixels above each layer's baseY) for mountain ridges.
// Picked by hand to look like actual mountain ranges — uneven spacing,
// big peaks next to small ones, occasional valley plateaus. Each layer
// uses the same table but different peak spacing and amplitude scale,
// so the layers don't all have identical silhouettes.
//
// Width is intentionally non-power-of-two so the wraparound period is
// long enough that the user can't easily spot the loop.
constexpr int kPeakCount = 23;
const uint8_t kPeakHeights[kPeakCount] = {
	24, 10, 38,  6, 28, 48, 14, 34,  8, 22,
	44, 18, 30, 12, 40, 26,  4, 36, 20, 32,
	16, 46, 28,
};

struct MountainLayer {
	int     baseY;        // y-coord of valleys (peaks rise above this)
	int     spacing;      // average horizontal pixels between peaks
	int     ampScale;     // 0..255 — peak height multiplier (256 = 1.0)
	int     pxPerSec;     // horizontal scroll speed (positive = leftward)
	uint8_t color;
};

const MountainLayer kLayers[3] = {
	// Speeds tuned for a calm dusk feel:
	//   far  : 18 px/s ≈ 0.30 px/frame at 60 fps  — barely perceptible drift
	//   mid  : 52 px/s ≈ 0.85 px/frame             — gentle background motion
	//   near : 130 px/s ≈ 2.15 px/frame            — leading-edge parallax
	//
	// Ratio ≈ 1 : 2.9 : 7.2. Wider than a strict 1:2:4 because human
	// eyes read parallax more readily when speed gaps are exaggerated;
	// otherwise the three layers blur together. Absolute speeds kept
	// low so column-quantized rendering doesn't read as judder.
	{ 110, 38, 200,  18, kColMtnFar  },
	{ 138, 30, 230,  52, kColMtnMid  },
	{ 168, 22, 256, 130, kColMtnNear },
};

// Returns the silhouette height (pixels above baseY) at virtual world
// coordinate `vx` for a given layer, in 8.8 fixed-point. Sub-pixel
// precision is what lets sub-pixel scroll offsets actually shift the
// silhouette every frame instead of jumping a full pixel every few
// frames — the dither in drawMountains turns those fractional values
// into smooth visual motion.
//
// Peaks are placed at virtual x = idx * spacing + spacing/2; valleys
// sit at cell boundaries, so adjacent peaks of different heights are
// separated by a clean V instead of forming sinusoidal hills.
inline int peakHeightAtFixed(const MountainLayer &L, int vxFixed) {
	const int period = L.spacing * kPeakCount;
	if (period <= 0) return 0;

	const int periodFixed = period << 8;
	int wx = vxFixed % periodFixed;
	if (wx < 0) wx += periodFixed;

	const int idx       = (wx >> 8) / L.spacing;
	const int local     = wx - idx * (L.spacing << 8);   // 8.8 within cell
	const int halfFixed = (L.spacing << 8) / 2;
	const int peak      = kPeakHeights[idx];             // integer px

	int hFixed;
	if (local < halfFixed) {
		// Rising slope: 0 at left valley → peak at cell center.
		hFixed = ((peak << 8) * local) /
		         (halfFixed > 0 ? halfFixed : 1);
	} else {
		// Falling slope: peak → 0 at right valley.
		const int t = local - halfFixed;
		hFixed = (peak << 8) -
		         ((peak << 8) * t) /
		         (halfFixed > 0 ? halfFixed : 1);
	}
	// Apply per-layer amplitude scale. Result stays in 8.8.
	return (hFixed * L.ampScale) >> 8;
}

// Reuses kBayer4 (defined for the sky dither) for sub-pixel ridge
// dithering. Same matrix → same spatial frequency, so the two layers
// don't beat against each other.
void drawMountains(Graphics::Surface *surf, uint32_t elapsedMs) {
	const int SW = surf->getWidth();
	const int SH = surf->getHeight();
	for (int li = 0; li < 3; ++li) {
		const MountainLayer &L = kLayers[li];
		// Scroll offset in 8.8 fixed pixels: pxPerSec * ms / 1000,
		// shifted left by 8 to keep fractional bits.
		const int scrollFixed =
		    (int)(((int64_t)elapsedMs * L.pxPerSec << 8) / 1000);

		// Different layers tile the dither with a different phase so
		// the three silhouettes don't beat against one another.
		const int phaseY = li & 3;

		for (int x = 0; x < SW; ++x) {
			const int hFixed =
			    peakHeightAtFixed(L, (x << 8) + scrollFixed);
			// Integer height + Bayer-dithered ±1 px on the fractional
			// part. As `hFixed` advances by less than one pixel, an
			// evenly distributed minority of columns rounds *up* by
			// one, shifting the apparent edge by sub-pixel amounts —
			// which the eye reads as smooth motion instead of
			// once-every-N-frames jumps.
			const int hInt = hFixed >> 8;
			const int frac = hFixed & 0xFF;     // 0..255
			// Bayer threshold expanded to 0..255 range (×17 ≈ ×16+1).
			const int thr  = kBayer4[x & 3][phaseY] * 17;
			const int bump = (frac > thr) ? 1 : 0;
			int top = L.baseY - (hInt + bump);
			if (top < 0)  top = 0;
			if (top > SH) top = SH;
			vlineFromY(surf, x, top, SH, L.color);
		}
	}
}

// ---- Title --------------------------------------------------------

constexpr int kTitleScale = 4;
constexpr int kTitleH     = kFontCellH * kTitleScale;   // 32 px

const char *kTitleText = "FRANK QUEST";

void buildTitleGradient(uint8_t out[kTitleH]) {
	// 32 destination rows mapped to 8 palette slots → 4 rows per band.
	for (int y = 0; y < kTitleH; ++y) {
		const int band = y * 8 / kTitleH;
		out[y] = (uint8_t)(kColTitle0 + (band > 7 ? 7 : band));
	}
}

constexpr int kTitleY0 = 36;

int titleX0(int screenWidth) {
	const int titleLen = (int)strlen(kTitleText);
	const int titleW   = titleLen * kFontCellW * kTitleScale;
	return (screenWidth - titleW) / 2;
}

// Static (no-physics) title draw — used during the main intro phase.
void drawTitle(Graphics::Surface *surf) {
	uint8_t gradient[kTitleH];
	buildTitleGradient(gradient);

	uint8_t shadow[kTitleH];
	memset(shadow, kColShadow, sizeof(shadow));

	const int titleLen = (int)strlen(kTitleText);
	const int x0       = titleX0(surf->getWidth());
	const int y0       = kTitleY0;

	// Drop shadow, offset (scale, scale) so it reads at a glance.
	for (int i = 0; i < titleLen; ++i) {
		drawGlyphScaled(surf, x0 + kTitleScale + i * kFontCellW * kTitleScale,
		                y0 + kTitleScale, kTitleText[i], kTitleScale, shadow);
	}
	// Front face with vertical gradient.
	for (int i = 0; i < titleLen; ++i) {
		drawGlyphScaled(surf, x0 + i * kFontCellW * kTitleScale,
		                y0, kTitleText[i], kTitleScale, gradient);
	}
}

// Falling-title draw. Each character has its own y-offset (positive
// or negative); spaces are skipped. Glyphs fully off-screen on either
// the top or bottom edge are simply omitted, so the same renderer
// works for both the intro (negative offsets falling to 0) and the
// outro (positive offsets falling past the bottom).
void drawTitleFalling(Graphics::Surface *surf, const int *yOffset) {
	uint8_t gradient[kTitleH];
	buildTitleGradient(gradient);

	uint8_t shadow[kTitleH];
	memset(shadow, kColShadow, sizeof(shadow));

	const int titleLen = (int)strlen(kTitleText);
	const int x0       = titleX0(surf->getWidth());
	const int y0       = kTitleY0;
	const int sh       = surf->getHeight();

	for (int i = 0; i < titleLen; ++i) {
		if (kTitleText[i] == ' ') continue;
		const int gy = y0 + yOffset[i];
		// Skip glyphs that haven't entered the screen yet (intro) or
		// have already exited the bottom (outro).
		if (gy + kTitleH <= 0) continue;
		if (gy >= sh)          continue;

		const int gx = x0 + i * kFontCellW * kTitleScale;
		drawGlyphScaled(surf, gx + kTitleScale, gy + kTitleScale,
		                kTitleText[i], kTitleScale, shadow);
		drawGlyphScaled(surf, gx, gy, kTitleText[i], kTitleScale, gradient);
	}
}

// ---- Ticker -------------------------------------------------------
//
// Scrolls a long greet message right-to-left at the bottom of the
// screen, with a per-character vertical sine bob. Returns true once
// the whole message has scrolled off the left edge.

// No leading or trailing whitespace padding here — the renderer
// already places the first character at scrollX=SW (just off-screen
// right) on frame 0, so leading spaces only add empty scroll time
// before the user sees anything. Same goes for trailing: the outro
// already handles the visual exit, so trailing padding just inserts
// dead air between the last glyph and tickerDoneMs.
const char *kTickerMsg =
    "WELCOME TO FRANK QUEST FIRMWARE * "
    "THIS IS A PORT OF CABAL PROJECT TO THE RP2350 MICROCONTROLLER * "
    "RUNS SCUMM, SCI, AGI AND OTHER ENGINES * "
    "MADE WITH SOME MAGIC AND WHISTLES BY MIKHAIL MATVEEV IN 2026 * "
    "COPY ALL GAMES TO \"QUEST\" DIR ON MICROSD CARD, "
    "INSERT INTO YOUR BOARD AND REBOOT * "
    "GREETINGS TO MURMULATOR COMMUNITY * "
    "DNCRAPTOR, WE PROBABLY NEED VGA AND PWM FOR THIS PORT :)";

constexpr int kTickerScale     = 2;
constexpr int kTickerCharW     = kFontCellW * kTickerScale;   // 12
constexpr int kTickerCharH     = kFontCellH * kTickerScale;   // 16
constexpr int kTickerY         = 178;                         // baseline-ish
constexpr int kTickerWobbleAmp = 2;                           // peak ≈ ±2 px
// 60 px/s lands on exactly 1 px/frame at the 60 Hz frame target — the
// smoothest motion achievable with binary glyphs (no anti-aliasing).
constexpr int kTickerPxPerSec  = 60;
// Phase step per screen column. 256-entry sin table, step 3 → 85-px
// wavelength. Long enough that the slope is gentle (≈ amp*2π/85
// ≈ 0.37 px per screen px), which spaces the unavoidable 1-px
// stair-steps ~3 columns apart so the eye reads them as a continuous
// wave instead of as ladders.
constexpr int kSineScrollPxStep = 3;

// Underline glow band, drawn between [x0, x1) on the screen. Used for
// three phases:
//   intro draw-in  : [0, lineRight) where lineRight grows 0→SW
//   steady state   : [0, SW)
//   outro wipe-out : [wipeX, SW) where wipeX grows 0→SW
void drawGlowLine(Graphics::Surface *surf, int x0, int x1) {
	const int SW = surf->getWidth();
	if (x0 < 0)  x0 = 0;
	if (x1 > SW) x1 = SW;
	if (x1 <= x0) return;
	const int y = kTickerY + kTickerCharH + 1;
	hline(surf, x0, y, x1 - x0, kColTickerGlow);
}

// Sinescroll renderer. Per-pixel-column vertical offset means the
// wobble is spatially continuous across the entire ticker — no per-
// glyph quantization steps. The wave moves over time because the
// phase advances with elapsedMs, so a given character undulates
// vertically as it scrolls through the screen-fixed wave.
void drawTicker(Graphics::Surface *surf, uint32_t elapsedMs) {
	const int SW     = surf->getWidth();
	const int msgLen = (int)strlen(kTickerMsg);

	// At t=0 the first char sits at x=SW and advances left.
	const int scroll  = (int)((int64_t)elapsedMs * kTickerPxPerSec / 1000);
	const int scrollX = SW - scroll;

	// Wobble phase is wall-clock driven so the wave rate stays
	// constant regardless of frame jitter. 480 = 2× the original 240
	// rate — gives the wave a more energetic bob without retuning
	// amplitude or wavelength.
	const int wobblePhase = (int)((int64_t)elapsedMs * 480 / 1000);

	for (int i = 0; i < msgLen; ++i) {
		const int cx = scrollX + i * kTickerCharW;
		if (cx + kTickerCharW < 0)   continue;
		if (cx >= SW)                continue;

		const uint8_t *gly = glyph6x8(kTickerMsg[i]);
		for (int row = 0; row < kFontCellH; ++row) {
			const uint8_t bits = gly[row];
			for (int col = 0; col < kFontCellW; ++col) {
				if (!(bits & (0x80 >> col))) continue;
				const int px = cx + col * kTickerScale;

				// Sample the sine wave at this screen column. With
				// kTickerScale=2 we shift both columns of a glyph
				// pixel by the same y so the 2x scale stays clean —
				// otherwise a single glyph pixel could split across
				// two y values and look torn.
				const int sxKey = px & ~1;
				const int wob   = (isin(wobblePhase + sxKey * kSineScrollPxStep) *
				                   kTickerWobbleAmp) / 128;
				const int py    = kTickerY + wob + row * kTickerScale;

				putPixel(surf, px,     py,     kColTicker);
				putPixel(surf, px + 1, py,     kColTicker);
				putPixel(surf, px,     py + 1, kColTicker);
				putPixel(surf, px + 1, py + 1, kColTicker);
			}
		}
	}
}

// Total time required for the ticker to finish scrolling — first char
// has to travel SW + msgWidth pixels at kTickerPxPerSec.
uint32_t tickerDurationMs(int screenWidth) {
	const int msgLen   = (int)strlen(kTickerMsg);
	const int msgWidth = msgLen * kTickerCharW;
	const int totalPx  = screenWidth + msgWidth;
	return (uint32_t)((int64_t)totalPx * 1000 / kTickerPxPerSec);
}

// ---- Footer (info block under the title) --------------------------

void drawFooter(Graphics::Surface *surf) {
#ifndef CABAL_VERSION
#define CABAL_VERSION "?.??"
#endif
	char vline[40];
	snprintf(vline, sizeof(vline), "VERSION %s", CABAL_VERSION);

	const char *url   = "github.com/rh1tech/frank-quest";
	const char *byline = "(c) 2026 Mikhail Matveev";

	// Drop-shadow centered text. Footer sits above the mountain horizon
	// but the silhouettes can rise into it on tall peaks, so the shadow
	// is what keeps it legible.
	auto centered = [&](const char *s, int y, uint8_t color) {
		const int w = (int)strlen(s) * kFontCellW;
		const int x = (surf->getWidth() - w) / 2;
		drawText(surf, x + 1, y + 1, s, kColShadow);
		drawText(surf, x,     y,     s, color);
	};

	centered(vline,  88, kColTitle0);          // bright white-yellow
	centered(byline, 100, kColTitle0 + 2);     // pale yellow
	centered(url,    112, kColTitle0 + 2);
}

// ---- Input drain --------------------------------------------------
//
// Returns true if the user pressed a key or mouse button this frame,
// meaning the welcome should be dismissed.
bool drainAndCheckInput(OSystem_RP2350 *sys) {
	bool gotInput = false;
	Common::Event ev;
	while (sys->pollEvent(ev)) {
		switch (ev.type) {
		case Common::EVENT_KEYDOWN:
		case Common::EVENT_LBUTTONDOWN:
		case Common::EVENT_RBUTTONDOWN:
		case Common::EVENT_MBUTTONDOWN:
			gotInput = true;
			break;
		default:
			break;
		}
	}
	return gotInput;
}

// Swallow any pending events without checking them. Used at startup
// to clear stale presses queued before the welcome screen took over,
// so a key buffered during boot doesn't dismiss us instantly.
void flushPendingEvents(OSystem_RP2350 *sys) {
	Common::Event ev;
	while (sys->pollEvent(ev)) { /* discard */ }
}

} // namespace

void frank_quest_show_welcome(uint32_t timeoutMs) {
	// CMM2: no welcome when RUN named a game (RUN "scummvm.app", dott): it starts at once
	if (cmm2_start && cmm2_start->argc > 1) return;
	installPalette();
	initSinTable();

	OSystem_RP2350 *sys = static_cast<OSystem_RP2350 *>(g_system);
	const uint32_t startMs = g_system->getMillis();

	// Drop anything the input drivers buffered before we got here so
	// the welcome doesn't dismiss itself on a stale boot-time keypress.
	flushPendingEvents(sys);

	const int SW = g_system->getWidth();

	// Phase boundaries:
	//   intro            : runs until every title glyph reaches rest.
	//                      End time (`tickerStartMs`) is determined
	//                      dynamically by the physics simulation — not
	//                      a fixed timer — so the ticker always begins
	//                      the moment the last glyph settles, no
	//                      matter how the bounce parameters change.
	//   main             : tickerStartMs..tickerDoneMs (ticker scrolling)
	//   outro            : tickerDoneMs..tickerDoneMs + 5000
	//   input accepted   : t >= lockEndMs
	//
	// Hard upper bound on intro length so we can't ever stall here if
	// physics goes weird (e.g. low-FPS environment shifts integration).
	constexpr uint32_t kIntroMaxMs = 4000;
	uint32_t tickerStartMs = 0;          // 0 = not started yet
	uint32_t tickerDoneMs  = 0;
	uint32_t lockEndMs     = 0;
	uint32_t exitDeadlineMs = 0;

	// Intro timing (ms from startMs). Mirrors the outro structure but
	// reversed: line and mountains build up, title falls in from above.
	constexpr uint32_t kIntroLineStart    = 0;
	constexpr uint32_t kIntroLineDuration = 400;
	constexpr uint32_t kIntroFadeStart    = 100;
	constexpr uint32_t kIntroFadeDuration = 1100;
	constexpr uint32_t kIntroFallStart    = 150;

	// Outro timing (ms from tickerDoneMs).
	constexpr uint32_t kOutroLineStart    = 0;
	constexpr uint32_t kOutroLineDuration = 400;
	constexpr uint32_t kOutroFadeStart    = 300;
	constexpr uint32_t kOutroFadeDuration = 1800;
	constexpr uint32_t kOutroFallStart    = 700;

	// Title physics state — shared between intro fall-in and outro
	// fall-out. Intro starts each glyph far above its rest position
	// with downward velocity; gravity carries it to y=0 (rest) where
	// it bounces a couple of times then locks. Outro re-uses the same
	// arrays starting from rest, with gravity dropping it past the
	// bottom edge.
	const int titleLen      = (int)strlen(kTitleText);
	int titleYOffset[32]    = { 0 };   // vertical displacement (px)
	int titleVel[32]        = { 0 };   // velocity in 8.8 fixed (px/frame << 8)
	bool titleStarted[32]   = { false };
	bool titleSettled[32]   = { false };  // intro: true once landed at rest
	bool outroStarted[32]   = { false };

	const uint8_t kStaggerMs = 40;
	const uint8_t kSeedRng[16] = {
	    13, 5, 21, 9, 27, 17, 3, 31, 11, 25, 7, 19, 1, 29, 15, 23
	};

	// Initial intro state: every non-space glyph starts well above the
	// top of the screen with a fixed downward velocity. Each glyph
	// activates after its stagger delay; until then it just sits
	// off-screen and isn't drawn.
	for (int i = 0; i < titleLen; ++i) {
		if (kTitleText[i] == ' ') continue;
		titleYOffset[i] = -(kTitleY0 + kTitleH + 8);   // off-screen above
	}

	// Glow line bounds — recomputed every frame from the phase clock.
	int lineX0 = 0;       // left edge
	int lineX1 = 0;       // right edge

	// Currently-applied mountain fade level (0 = full mountains, 256 =
	// fully dissolved). Set initially to 256 so installPalette()'s
	// resting mountain colors get overwritten by the intro fade-in's
	// first frame.
	int lastFadeT = -1;
	fadeMountains(256);
	lastFadeT = 256;

	constexpr uint32_t kFrameMs = 16;
	uint32_t nextFrameMs = g_system->getMillis();

	while (true) {
		const uint32_t now     = g_system->getMillis();
		const uint32_t elapsed = now - startMs;

		// Once intro completes (all glyphs settled or upper bound
		// hit), latch the downstream phase boundaries.
		if (tickerStartMs == 0) {
			bool allSettled = true;
			for (int i = 0; i < titleLen; ++i) {
				if (kTitleText[i] == ' ')   continue;
				if (!titleSettled[i])       { allSettled = false; break; }
			}
			// Don't latch until the fall has actually started — at
			// t=0 every glyph is "not yet active" and would falsely
			// look settled.
			const bool fallActive = elapsed >= kIntroFallStart;
			if ((allSettled && fallActive) || elapsed >= kIntroMaxMs) {
				tickerStartMs  = elapsed;
				tickerDoneMs   = tickerStartMs + tickerDurationMs(SW);
				lockEndMs      = tickerDoneMs + 5000u;
				exitDeadlineMs =
				    (timeoutMs > lockEndMs) ? timeoutMs : lockEndMs;
			}
		}

		// Exit conditions only meaningful once tickerStartMs is set.
		if (tickerStartMs != 0 && elapsed >= exitDeadlineMs) break;

		// Any keypress or mouse-button press dismisses the welcome
		// immediately — including during the intro fall-in. Stale
		// boot-time events were drained above, so this can't fire on
		// frame 1 from buffered noise.
		if (drainAndCheckInput(sys)) break;

		const bool inIntro = (tickerStartMs == 0);
		const bool inOutro = (tickerStartMs != 0) && (elapsed >= tickerDoneMs);
		const uint32_t introT = elapsed;
		const uint32_t outroT = inOutro ? (elapsed - tickerDoneMs) : 0;

		// ---- Glow line --------------------------------------------
		// Intro: draws in from left, [0, lineRight) where lineRight
		// grows 0→SW.
		// Steady: full width.
		// Outro: wipes off to the right, [wipeX, SW).
		if (inIntro) {
			lineX0 = 0;
			if (introT < kIntroLineStart) {
				lineX1 = 0;
			} else {
				const uint32_t dt = introT - kIntroLineStart;
				lineX1 = dt >= kIntroLineDuration
				          ? SW
				          : (int)((int64_t)dt * SW / kIntroLineDuration);
			}
		} else if (inOutro) {
			lineX1 = SW;
			if (outroT < kOutroLineStart) {
				lineX0 = 0;
			} else {
				const uint32_t dt = outroT - kOutroLineStart;
				lineX0 = dt >= kOutroLineDuration
				          ? SW
				          : (int)((int64_t)dt * SW / kOutroLineDuration);
			}
		} else {
			lineX0 = 0;
			lineX1 = SW;
		}

		// ---- Mountain fade ----------------------------------------
		// Intro: 256 (sky color) → 0 (full mountains).
		// Steady: 0.
		// Outro: 0 → 256.
		int fadeT = 0;
		if (inIntro) {
			if (introT < kIntroFadeStart) {
				fadeT = 256;
			} else {
				const uint32_t dt = introT - kIntroFadeStart;
				fadeT = dt >= kIntroFadeDuration
				          ? 0
				          : 256 - (int)((int64_t)dt * 256 / kIntroFadeDuration);
			}
		} else if (inOutro && outroT >= kOutroFadeStart) {
			const uint32_t dt = outroT - kOutroFadeStart;
			fadeT = dt >= kOutroFadeDuration
			          ? 256
			          : (int)((int64_t)dt * 256 / kOutroFadeDuration);
		}
		if (fadeT != lastFadeT) {
			fadeMountains(fadeT);
			lastFadeT = fadeT;
		}

		// ---- Title physics ----------------------------------------
		// Intro fall-in: glyphs descend from off-screen-top, hit y=0,
		// bounce once with damped velocity, then settle.
		if (inIntro && introT >= kIntroFallStart) {
			const uint32_t dt = introT - kIntroFallStart;
			for (int i = 0; i < titleLen; ++i) {
				if (kTitleText[i] == ' ') continue;
				if (titleSettled[i])      continue;

				const uint32_t startDelay = (uint32_t)i * kStaggerMs;
				if (!titleStarted[i] && dt >= startDelay) {
					titleStarted[i] = true;
					// Per-character variation so the row doesn't drop
					// in lockstep — a small extra downward kick.
					titleVel[i] = (kSeedRng[i & 15] & 0x07) << 6;
				}
				if (!titleStarted[i]) continue;

				// Gravity 0.45 px/frame² in 8.8 fixed.
				titleVel[i] += 115;
				titleYOffset[i] += titleVel[i] >> 8;

				// Bounce on rest line. Reflect velocity with 65%
				// energy loss until the bounce becomes imperceptible
				// (< ~1 px), then settle cleanly. Higher loss + larger
				// snap threshold collapses the bounce tail to 2–3
				// rebounds (≈250 ms) instead of 5–6 (≈800 ms) —
				// preserves the bouncy feel while letting the ticker
				// start much sooner after the title lands.
				if (titleYOffset[i] >= 0) {
					if (titleVel[i] < 256) {
						titleYOffset[i] = 0;
						titleVel[i]     = 0;
						titleSettled[i] = true;
					} else {
						titleYOffset[i] = -titleYOffset[i];
						titleVel[i]     = -(titleVel[i] * 35 / 100);
					}
				}
			}
		}

		// Outro fall-out: same physics arrays, but reset to rest at
		// the moment we cross into the outro phase, then accelerate
		// downward indefinitely.
		if (inOutro && outroT >= kOutroFallStart) {
			const uint32_t dt = outroT - kOutroFallStart;
			for (int i = 0; i < titleLen; ++i) {
				if (kTitleText[i] == ' ') continue;
				const uint32_t startDelay = (uint32_t)i * kStaggerMs;
				if (!outroStarted[i] && dt >= startDelay) {
					outroStarted[i] = true;
					// Reset to rest in case intro physics left a
					// residual offset, then add an upward kick like
					// before so the column doesn't drop in lockstep.
					titleYOffset[i] = 0;
					titleVel[i]     = -((kSeedRng[i & 15] & 0x07) << 6);
				}
				if (!outroStarted[i]) continue;

				titleVel[i] += 115;
				titleYOffset[i] += titleVel[i] >> 8;
			}
		}

		Graphics::Surface *surf = g_system->lockScreen();
		if (!surf || !surf->getPixels()) {
			if (surf) g_system->unlockScreen();
			g_system->delayMillis(kFrameMs);
			nextFrameMs = g_system->getMillis() + kFrameMs;
			continue;
		}

		drawSky(surf);
		drawMountains(surf, elapsed);

		// Title: falling renderer during intro & outro phases (handles
		// off-screen culling either side); static otherwise.
		if (inIntro || (inOutro && outroT >= kOutroFallStart)) {
			drawTitleFalling(surf, titleYOffset);
		} else {
			drawTitle(surf);
		}

		// Footer is hidden during the intro so it doesn't pop in
		// before the title has settled. Visible everywhere else.
		if (!inIntro) {
			drawFooter(surf);
		}
		drawGlowLine(surf, lineX0, lineX1);

		// Ticker text paints only during the main phase, between the
		// intro and outro. Its scroll clock is reset to 0 at
		// tickerStartMs so the message starts fully off-screen.
		if (!inIntro && !inOutro) {
			drawTicker(surf, elapsed - tickerStartMs);
		}

		g_system->unlockScreen();
		g_system->updateScreen();

		nextFrameMs += kFrameMs;
		const uint32_t after = g_system->getMillis();
		if ((int32_t)(nextFrameMs - after) > 0) {
			g_system->delayMillis(nextFrameMs - after);
		} else {
			nextFrameMs = after;
		}
	}
}
