/* CMM2 (apps/scummvm/overlay): frank-quest's file at 949301b with one detector row added (dgc64), and a game named
   after RUN's comma started at once (RUN "scummvm.app", dott: frank_quest_run_selector). */
/*
 * FRANK Quest
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Game selector implementation. Renders via the OSystem backend so we
 * get palette + event routing for free; input uses the low-level event
 * pump (pollEvent) so arrow keys and Enter work during the selector
 * phase even though no engine is active.
 */

#include "frank_quest_selector.h"
#include "frank_quest_fs.h"

#include "backends/platform/rp2350/rp2350-system.h"
#include "backends/platform/rp2350/rp2350-minimal.h"

#include "common/events.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "graphics/palette.h"

#include <stdio.h>
#include <string.h>
#include "cmm2.h"                                // CMM2: cmm2_start, for the game named on RUN's line
#include <stdlib.h>

extern OSystem *g_system;

namespace {

// ---- detector table -----------------------------------------------
//
// Each entry is matched against a scanned /quest/<dir> entry. The
// directory name must equal `dirName`, and `probeFile` must exist
// relative to the directory (or be NULL for "directory alone is
// enough"). First match wins, so order specific variants before
// generics (e.g. "mi1ega" before "mi1").
struct Detector {
	const char *dirName;
	const char *probeFile;       // NULL = no file probe
	const char *displayName;
	QuestEngine engine;
	uint8_t     engineSubtype;   // kyra gameId; unused for other engines
	const char *engineHint;      // scumm dirName / sci gameid
};

// Detector entries cover the same games that the old hardcoded
// cabal_main() list supported. Ordering: SCUMM first (most specific
// directory names), then SCI, then Kyra/Gob/AGI.
const Detector kDetectors[] = {
	// --- SCUMM v4 ---
	{"mi1",      "000.LFL",       "Monkey Island 1 (VGA floppy)",     QuestEngine::Scumm, 0, "mi1"},
	{"mi1ega",   "000.LFL",       "Monkey Island 1 (EGA floppy)",     QuestEngine::Scumm, 0, "mi1ega"},
	{"loom",     "000.LFL",       "Loom (VGA)",                       QuestEngine::Scumm, 0, "loom"},
	// --- SCUMM v5 ---
	{"mi1cd",    "monkey.000",    "Monkey Island 1 (CD talkie)",      QuestEngine::Scumm, 0, "mi1cd"},
	{"monkey",   "monkey.000",    "Monkey Island 1 (CD talkie)",      QuestEngine::Scumm, 0, "monkey"},
	{"mi2",      "monkey2.000",   "Monkey Island 2: LeChuck's Revenge",QuestEngine::Scumm, 0, "mi2"},
	{"monkey2",  "monkey2.000",   "Monkey Island 2: LeChuck's Revenge",QuestEngine::Scumm, 0, "monkey2"},
	{"atlantis", "atlantis.000",  "Indiana Jones and the Fate of Atlantis", QuestEngine::Scumm, 0, "atlantis"},
	{"indy4",    "atlantis.000",  "Indiana Jones and the Fate of Atlantis", QuestEngine::Scumm, 0, "indy4"},
	// --- SCUMM v6 ---
	{"dott",     "tentacle.000",  "Day of the Tentacle",              QuestEngine::Scumm, 0, "dott"},
	{"tentacle", "tentacle.000",  "Day of the Tentacle",              QuestEngine::Scumm, 0, "tentacle"},
	{"samnmax",  NULL,            "Sam & Max Hit the Road",           QuestEngine::Scumm, 0, "samnmax"},
	{"sam",      NULL,            "Sam & Max Hit the Road",           QuestEngine::Scumm, 0, "sam"},
	// --- SCUMM v7 ---
	{"ft",       "ft.la0",        "Full Throttle",                    QuestEngine::Scumm, 0, "ft"},
	{"fulltp",   "ft.la0",        "Full Throttle",                    QuestEngine::Scumm, 0, "fulltp"},

	// --- SCI (Sierra) ---
	{"kq1",      NULL, "King's Quest I",                 QuestEngine::Sci, 0, "kq1"},
	{"kq4",      NULL, "King's Quest IV",                QuestEngine::Sci, 0, "kq4"},
	{"kq5",      NULL, "King's Quest V",                 QuestEngine::Sci, 0, "kq5"},
	{"kq6",      NULL, "King's Quest VI",                QuestEngine::Sci, 0, "kq6"},
	{"lsl1",     NULL, "Leisure Suit Larry 1",           QuestEngine::Sci, 0, "lsl1"},
	{"lsl2",     NULL, "Leisure Suit Larry 2",           QuestEngine::Sci, 0, "lsl2"},
	{"lsl3",     NULL, "Leisure Suit Larry 3",           QuestEngine::Sci, 0, "lsl3"},
	{"lsl5",     NULL, "Leisure Suit Larry 5",           QuestEngine::Sci, 0, "lsl5"},
	{"lsl6",     NULL, "Leisure Suit Larry 6",           QuestEngine::Sci, 0, "lsl6"},
	{"sq1",      NULL, "Space Quest I",                  QuestEngine::Sci, 0, "sq1"},
	{"sq3",      NULL, "Space Quest III",                QuestEngine::Sci, 0, "sq3"},
	{"sq4",      NULL, "Space Quest IV",                 QuestEngine::Sci, 0, "sq4"},
	{"sq5",      NULL, "Space Quest V",                  QuestEngine::Sci, 0, "sq5"},
	{"pq1",      NULL, "Police Quest I",                 QuestEngine::Sci, 0, "pq1"},
	{"pq2",      NULL, "Police Quest II",                QuestEngine::Sci, 0, "pq2"},
	{"pq3",      NULL, "Police Quest III",               QuestEngine::Sci, 0, "pq3"},
	{"qfg1vga",  NULL, "Quest for Glory I (VGA)",        QuestEngine::Sci, 0, "qfg1vga"},
	{"qfg1",     NULL, "Quest for Glory I",              QuestEngine::Sci, 0, "qfg1"},
	{"qfg2",     NULL, "Quest for Glory II",             QuestEngine::Sci, 0, "qfg2"},
	{"qfg3",     NULL, "Quest for Glory III",            QuestEngine::Sci, 0, "qfg3"},
	{"iceman",     NULL, "Codename: ICEMAN",             QuestEngine::Sci, 0, "iceman"},
	{"laurabow2",  NULL, "Laura Bow 2: Dagger of Amon Ra",QuestEngine::Sci, 0, "laurabow2"},
	{"laurabow",   NULL, "Laura Bow: Colonel's Bequest", QuestEngine::Sci, 0, "laurabow"},
	{"longbow",    NULL, "Conquests of the Longbow",     QuestEngine::Sci, 0, "longbow"},
	{"ecoquest2",  NULL, "EcoQuest 2",                   QuestEngine::Sci, 0, "ecoquest2"},
	{"ecoquest",   NULL, "EcoQuest",                     QuestEngine::Sci, 0, "ecoquest"},
	{"freddy",     NULL, "Freddy Pharkas",               QuestEngine::Sci, 0, "freddy"},
	{"jones",      NULL, "Jones in the Fast Lane",       QuestEngine::Sci, 0, "jones"},
	{"pepper",     NULL, "Pepper's Adventures in Time",  QuestEngine::Sci, 0, "pepper"},
	{"slater",     NULL, "Slater & Charlie Go Camping",  QuestEngine::Sci, 0, "slater"},
	{"castlebrain",NULL, "Castle of Dr. Brain",          QuestEngine::Sci, 0, "castlebrain"},
	{"islandbrain",NULL, "Island of Dr. Brain",          QuestEngine::Sci, 0, "islandbrain"},
	{"mothergoose",NULL, "Mixed-Up Mother Goose",        QuestEngine::Sci, 0, "mothergoose"},
	{"camelot",    NULL, "Conquests of Camelot",         QuestEngine::Sci, 0, "camelot"},
	// CMM2: Robin Gravel's public-domain SCI0 fan game, shipped with the firmware as an example. A name no Sierra
	// game starts with, so launchSciGame takes it as a fan game (GID_FANMADE)
	{"dgc64",      NULL, "Another DG Game: I Want My C64 Back", QuestEngine::Sci, 0, "dgc64"},

	// --- Kyra ---
	{"kyra2",       NULL, "Legend of Kyrandia 2: Hand of Fate",   QuestEngine::Kyra, 1, ""},
	{"kyr2",        NULL, "Legend of Kyrandia 2: Hand of Fate",   QuestEngine::Kyra, 1, ""},
	{"kyrandia2",   NULL, "Legend of Kyrandia 2: Hand of Fate",   QuestEngine::Kyra, 1, ""},
	{"handoffate",  NULL, "Legend of Kyrandia 2: Hand of Fate",   QuestEngine::Kyra, 1, ""},
	{"kyra1",       NULL, "Legend of Kyrandia 1",                 QuestEngine::Kyra, 0, ""},
	{"kyr1",        NULL, "Legend of Kyrandia 1",                 QuestEngine::Kyra, 0, ""},
	{"kyrandia",    NULL, "Legend of Kyrandia 1",                 QuestEngine::Kyra, 0, ""},

	// --- Gobliiins ---
	{"gob1",  NULL, "Gobliiins",     QuestEngine::Gob, 1, ""},
	{"gob2",  NULL, "Gobliins 2",    QuestEngine::Gob, 2, ""},
	{"gob3",  NULL, "Goblins 3",     QuestEngine::Gob, 3, ""},
	{"gob",   NULL, "Gobliiins (auto-detect)", QuestEngine::Gob, 0, ""},

	// --- AGI (Sierra pre-SCI) ---
	{"agi",   NULL, "AGI fan-made / generic",  QuestEngine::Agi, 0, ""},
};

constexpr int kNumDetectors = sizeof(kDetectors) / sizeof(kDetectors[0]);

int strcasecmp_ascii(const char *a, const char *b) {
	while (*a && *b) {
		char ca = *a, cb = *b;
		if (ca >= 'A' && ca <= 'Z') ca = ca - 'A' + 'a';
		if (cb >= 'A' && cb <= 'Z') cb = cb - 'A' + 'a';
		if (ca != cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
		++a; ++b;
	}
	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

// ---- 5x7 glyphs ---------------------------------------------------
//
// Small inline font so the selector doesn't need a real renderer. Rows
// stored MSB-left across 5 columns. Uppercase-only plus digits and a
// handful of punctuation that we actually use in display names.

struct Glyph5x7 {
	char    ch;
	uint8_t rows[7];
};

const Glyph5x7 kGlyphs[] = {
	{' ',  {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{'!',  {0x04,0x04,0x04,0x04,0x04,0x00,0x04}},
	{'"',  {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}},
	{'&',  {0x08,0x14,0x14,0x08,0x15,0x12,0x0D}},
	{'\'', {0x04,0x04,0x00,0x00,0x00,0x00,0x00}},
	{'(',  {0x02,0x04,0x08,0x08,0x08,0x04,0x02}},
	{')',  {0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
	{'*',  {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}},
	{'+',  {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}},
	{',',  {0x00,0x00,0x00,0x00,0x00,0x04,0x08}},
	{'-',  {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
	{'.',  {0x00,0x00,0x00,0x00,0x00,0x00,0x04}},
	{'/',  {0x01,0x02,0x04,0x08,0x10,0x00,0x00}},
	{'0',  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
	{'1',  {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
	{'2',  {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
	{'3',  {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}},
	{'4',  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
	{'5',  {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}},
	{'6',  {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}},
	{'7',  {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
	{'8',  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
	{'9',  {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}},
	{':',  {0x00,0x04,0x00,0x00,0x00,0x04,0x00}},
	{'?',  {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}},
	{'A',  {0x0E,0x11,0x11,0x11,0x1F,0x11,0x11}},
	{'B',  {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
	{'C',  {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
	{'D',  {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}},
	{'E',  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
	{'F',  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
	{'G',  {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}},
	{'H',  {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
	{'I',  {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
	{'J',  {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
	{'K',  {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
	{'L',  {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
	{'M',  {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
	{'N',  {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
	{'O',  {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
	{'P',  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
	{'Q',  {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
	{'R',  {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
	{'S',  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
	{'T',  {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
	{'U',  {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
	{'V',  {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
	{'W',  {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}},
	{'X',  {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
	{'Y',  {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
	{'Z',  {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
	{'[',  {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}},
	{'\\', {0x10,0x08,0x08,0x04,0x02,0x02,0x01}},
	{']',  {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}},
	{'^',  {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}},
	{'_',  {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}},
	{'`',  {0x08,0x04,0x02,0x00,0x00,0x00,0x00}},
	{'|',  {0x04,0x04,0x04,0x04,0x04,0x04,0x04}},
};
constexpr int kNumGlyphs = sizeof(kGlyphs) / sizeof(kGlyphs[0]);

const Glyph5x7 *findGlyph(char ch) {
	if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
	for (int i = 0; i < kNumGlyphs; ++i) {
		if (kGlyphs[i].ch == ch) return &kGlyphs[i];
	}
	return &kGlyphs[0]; // space fallback
}

void drawGlyph(Graphics::Surface *surf, int x, int y, char ch,
               uint8_t color) {
	const Glyph5x7 *g = findGlyph(ch);
	const int w = surf->getWidth();
	const int h = surf->getHeight();
	for (int row = 0; row < 7; ++row) {
		const int py = y + row;
		if (py < 0 || py >= h) continue;
		const uint8_t bits = g->rows[row];
		for (int col = 0; col < 5; ++col) {
			if (!(bits & (1 << (4 - col)))) continue;
			const int px = x + col;
			if (px < 0 || px >= w) continue;
			*(uint8_t *)surf->getBasePtr(px, py) = color;
		}
	}
}

void drawText(Graphics::Surface *surf, int x, int y, const char *s,
              uint8_t color) {
	for (; *s; ++s) {
		drawGlyph(surf, x, y, *s, color);
		x += 6;  // 5 px glyph + 1 px gap
	}
}

void fillRect(Graphics::Surface *surf, int x, int y, int w, int h,
              uint8_t color) {
	const int sw = surf->getWidth();
	const int sh = surf->getHeight();
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > sw) w = sw - x;
	if (y + h > sh) h = sh - y;
	if (w <= 0 || h <= 0) return;
	for (int row = 0; row < h; ++row) {
		uint8_t *p = (uint8_t *)surf->getBasePtr(x, y + row);
		memset(p, color, w);
	}
}

// Scan /quest/ for directories and match against the detector table.
int scanGames(QuestGame *out, int maxOut) {
	const char *root = "/quest";
	if (!cabal_path_exists(root)) {
		printf("Selector: /quest does not exist on SD card\n");
		return 0;
	}

	CabalDir *dir = cabal_dir_open(root);
	if (!dir) {
		printf("Selector: cannot open /quest\n");
		return 0;
	}

	int count = 0;
	CabalDirEntry entry;
	while (cabal_dir_read(dir, &entry) && count < maxOut) {
		if (!entry.isDirectory) continue;

		// Match against detector table.
		const Detector *d = NULL;
		for (int i = 0; i < kNumDetectors; ++i) {
			if (strcasecmp_ascii(entry.name, kDetectors[i].dirName) != 0)
				continue;
#ifdef PICOMITE
			if (kDetectors[i].engine != QuestEngine::Scumm)   // PicoMite: only the SCUMM engine is built (make.py)
				continue;
#endif
			if (kDetectors[i].probeFile) {
				char probe[128];
				snprintf(probe, sizeof(probe), "%s/%s/%s",
				         root, entry.name, kDetectors[i].probeFile);
				if (!cabal_path_exists(probe)) continue;
			}
			d = &kDetectors[i];
			break;
		}
		if (!d) {
			printf("Selector: /quest/%s — no detector match, skipping\n",
			       entry.name);
			continue;
		}

		QuestGame *g = &out[count++];
		snprintf(g->dirPath, sizeof(g->dirPath), "%s/%s", root, entry.name);
		strncpy(g->displayName, d->displayName, sizeof(g->displayName) - 1);
		g->displayName[sizeof(g->displayName) - 1] = 0;
		g->engine = d->engine;
		g->engineSubtype = d->engineSubtype;
		strncpy(g->engineHint, d->engineHint, sizeof(g->engineHint) - 1);
		g->engineHint[sizeof(g->engineHint) - 1] = 0;

		printf("Selector: found %s -> %s\n", g->dirPath, g->displayName);
	}
	cabal_dir_close(dir);

	// Sort alphabetically by displayName. N is small so insertion sort
	// is fine and avoids pulling in qsort.
	for (int i = 1; i < count; ++i) {
		QuestGame tmp = out[i];
		int j = i;
		while (j > 0 && strcasecmp_ascii(out[j - 1].displayName, tmp.displayName) > 0) {
			out[j] = out[j - 1];
			--j;
		}
		out[j] = tmp;
	}

	return count;
}

// ---- palette ------------------------------------------------------
//
// The selector owns palette slots 0..5 during its lifetime. Engines
// rewrite the palette on init() so there's no need to save/restore.
enum : uint8_t {
	kColBg       = 0,  // black
	kColText     = 1,  // white
	kColDim      = 2,  // grey (status lines)
	kColAccent   = 3,  // accent (title / scrollbar thumb)
	kColTrack    = 4,  // scrollbar track
	kColHighlight= 5,  // highlight fill
};

void installPalette() {
	byte pal[6 * 3];
	// bg     — black
	pal[0]=0x00; pal[1]=0x00; pal[2]=0x00;
	// text   — white
	pal[3]=0xF0; pal[4]=0xF0; pal[5]=0xF0;
	// dim    — mid-grey
	pal[6]=0x70; pal[7]=0x70; pal[8]=0x78;
	// accent — warm amber
	pal[9]=0xE0; pal[10]=0xB8; pal[11]=0x30;
	// track  — dim blue
	pal[12]=0x28; pal[13]=0x30; pal[14]=0x48;
	// highlight — bright blue
	pal[15]=0x50; pal[16]=0x70; pal[17]=0xE0;
	g_system->getPaletteManager()->setPalette(pal, 0, 6);
}

// ---- layout -------------------------------------------------------
//
// 320x200 screen. Single windowed frame with a title bar at the top,
// the scrolling list beneath it, and a two-line legend below the
// window. All coordinates are derived from these constants so
// rendering and event math stay in sync.
struct Layout {
	int winX, winY, winW, winH;
	int titleH;
	int listX, listY;          // content origin
	int listW;                 // content width (excludes scrollbar when present)
	int lineH;
	int visibleLines;
	int legendY;
	bool scrollbar;
	int sbX, sbY, sbH, sbW;
};

constexpr int kLineH    = 10;
constexpr int kTitleH   = 12;
constexpr int kGlyphW   = 6;       // 5 px glyph + 1 px gap
constexpr int kSbWidth  = 3;
constexpr int kSbGap    = 3;

Layout computeLayout(int screenW, int screenH, int itemCount) {
	Layout L{};
	L.winX   = 4;
	L.winY   = 4;
	L.winW   = screenW - 8;
	L.titleH = kTitleH;
	L.lineH  = kLineH;

	// Reserve bottom 22 px for the two-line legend + gap.
	const int legendBlock = 22;
	L.winH   = screenH - L.winY - legendBlock;

	// Content geometry inside the window.
	const int contentTop    = L.winY + L.titleH + 2;
	const int contentBottom = L.winY + L.winH - 3;
	const int contentH      = contentBottom - contentTop;
	L.visibleLines          = contentH / L.lineH;
	if (L.visibleLines < 1) L.visibleLines = 1;

	L.scrollbar = (itemCount > L.visibleLines);

	// Content padding: matching 6 px gutter on both sides between the
	// window border and the text so a full-width highlight row sits
	// symmetrically inside the frame. When the scrollbar is visible it
	// eats the right gutter.
	L.listY = contentTop + 1;
	L.listX = L.winX + 6;
	const int rightReserve = L.scrollbar ? (kSbWidth + kSbGap + 3) : 6;
	L.listW = (L.winX + L.winW) - L.listX - rightReserve;

	L.sbW = kSbWidth;
	L.sbX = L.winX + L.winW - L.sbW - 3;
	L.sbY = L.listY;
	L.sbH = L.visibleLines * L.lineH;

	L.legendY = L.winY + L.winH + 4;
	return L;
}

void drawWindow(Graphics::Surface *surf, const Layout &L, int count) {
	// 1-px accent border.
	fillRect(surf, L.winX, L.winY, L.winW, 1, kColAccent);
	fillRect(surf, L.winX, L.winY + L.winH - 1, L.winW, 1, kColAccent);
	fillRect(surf, L.winX, L.winY, 1, L.winH, kColAccent);
	fillRect(surf, L.winX + L.winW - 1, L.winY, 1, L.winH, kColAccent);

	// Title bar: solid accent fill, dark text.
	fillRect(surf, L.winX + 1, L.winY + 1, L.winW - 2, L.titleH, kColAccent);
	drawText(surf, L.winX + 6, L.winY + 3, "FRANK Quest", kColBg);

	// Right-aligned game count inside the title bar.
	char rightLabel[32];
	snprintf(rightLabel, sizeof(rightLabel), "%d game%s",
	         count, count == 1 ? "" : "s");
	const int labelW = (int)strlen(rightLabel) * kGlyphW;
	drawText(surf, L.winX + L.winW - 6 - labelW, L.winY + 3,
	         rightLabel, kColBg);

	// Thin separator between title bar and content.
	fillRect(surf, L.winX + 1, L.winY + L.titleH + 1, L.winW - 2, 1,
	         kColTrack);
}

void renderFrame(const QuestGame *games, int count,
                 int selected, int scroll, const Layout &L) {
	Graphics::Surface *surf = g_system->lockScreen();
	if (!surf || !surf->getPixels()) {
		if (surf) g_system->unlockScreen();
		return;
	}

	const int SW = surf->getWidth();
	const int SH = surf->getHeight();
	fillRect(surf, 0, 0, SW, SH, kColBg);

	drawWindow(surf, L, count);

	// List content.
	if (count <= 0) {
		drawText(surf, L.listX, L.listY + 4,
		         "No games found in /quest.", kColText);
		drawText(surf, L.listX, L.listY + 4 + L.lineH,
		         "Copy games to /quest/<dir>/ on the SD card.", kColDim);
	} else {
		int end = scroll + L.visibleLines;
		if (end > count) end = count;
		for (int i = scroll; i < end; ++i) {
			const int y = L.listY + (i - scroll) * L.lineH;
			if (i == selected) {
				fillRect(surf, L.listX - 2, y - 1, L.listW + 4, L.lineH,
				         kColHighlight);
				drawText(surf, L.listX, y, games[i].displayName,
				         kColText);
			} else {
				drawText(surf, L.listX, y, games[i].displayName,
				         kColText);
			}
		}
	}

	// Scrollbar — only when the list can actually scroll.
	if (L.scrollbar) {
		fillRect(surf, L.sbX, L.sbY, L.sbW, L.sbH, kColTrack);

		int thumbH = (L.sbH * L.visibleLines) / count;
		if (thumbH < 6) thumbH = 6;
		if (thumbH > L.sbH) thumbH = L.sbH;
		const int maxScroll = count - L.visibleLines;
		const int maxTravel = L.sbH - thumbH;
		const int thumbY = L.sbY +
		                   (maxTravel * scroll) /
		                   (maxScroll > 0 ? maxScroll : 1);
		fillRect(surf, L.sbX, thumbY, L.sbW, thumbH, kColAccent);
	}

	// Legend below the window. Two columns; each prints as a single
	// "KEY: ACTION" string with a single space, and column B is a
	// fixed x-anchor so both lines line up vertically:
	//
	//   UP/DOWN: NAVIGATE           PGUP/PGDN: PAGE
	//   ENTER: LAUNCH               CTRL+ALT+DEL: RESET
	//
	// Column A's longest line is "UP/DOWN: NAVIGATE" (17 chars = 102
	// px). We place column B 20 chars past winX so the gap is at
	// least 3 chars = 18 px.
	const int colA = L.winX;
	const int colB = L.winX + 20 * kGlyphW;
	drawText(surf, colA, L.legendY,     "UP/DOWN: Navigate",    kColDim);
	drawText(surf, colB, L.legendY,     "PGUP/PGDN: Page",      kColDim);
	drawText(surf, colA, L.legendY + 9, "ENTER: Launch",        kColDim);
	drawText(surf, colB, L.legendY + 9, "CTRL+ALT+DEL: Reset",  kColDim);

	g_system->unlockScreen();
	g_system->updateScreen();
}

} // namespace

// CMM2: for the save menu (frank_quest_savemenu.cpp)
void frankQuestDrawText(Graphics::Surface *surf, int x, int y, const char *s, uint8_t color) {
	drawText(surf, x, y, s, color);
}

void frankQuestFillRect(Graphics::Surface *surf, int x, int y, int w, int h, uint8_t color) {
	fillRect(surf, x, y, w, h, color);
}

int frank_quest_scan_games(QuestGame *out, int maxOut) {
	return scanGames(out, maxOut);
}

void frank_quest_show_loading(const QuestGame &game) {
	// Paints a centered "Loading <game>..." window on top of whatever
	// the selector left on screen. Uses the palette slots the selector
	// already installed so the colors stay consistent. The engine will
	// repaint the full screen during startup and cover this window —
	// we just want something to look at during plugin load / resource
	// parsing so the user doesn't think the device froze.

	Graphics::Surface *surf = g_system->lockScreen();
	if (!surf || !surf->getPixels()) {
		if (surf) g_system->unlockScreen();
		return;
	}

	const int SW = surf->getWidth();
	const int SH = surf->getHeight();

	// Dim the background — clear to black.
	fillRect(surf, 0, 0, SW, SH, kColBg);

	// Word-wrap the game name into up to three lines so long titles
	// ("Indiana Jones and the Fate of Atlantis", "Legend of Kyrandia 2:
	// Hand of Fate") fit inside the window instead of being clipped.
	// Line 1 is always the "Loading" header; lines 2..4 hold wrapped
	// words from the display name, and a trailing "..." is appended to
	// whichever line gets the last word.
	const int pad_x = 10;
	const int pad_y = 8;
	const int maxWinW = SW - 40;
	const int maxLineChars =
	    (maxWinW - 2 * pad_x) / kGlyphW;  // glyph count per line

	constexpr int kMaxLines = 4;
	char lines[kMaxLines][64];
	int  lineCount = 0;
	int  widestChars = 0;

	auto pushLine = [&](const char *src) {
		if (lineCount >= kMaxLines) return;
		strncpy(lines[lineCount], src, sizeof(lines[0]) - 1);
		lines[lineCount][sizeof(lines[0]) - 1] = 0;
		const int n = (int)strlen(lines[lineCount]);
		if (n > widestChars) widestChars = n;
		++lineCount;
	};

	pushLine("Loading");

	// Greedy word-wrap over display name, then append "..." to the
	// final line.
	const char *p = game.displayName;
	char cur[64] = {0};
	int  curLen = 0;
	while (*p && lineCount < kMaxLines) {
		// Skip leading spaces.
		while (*p == ' ') ++p;
		if (!*p) break;

		// Find next word end.
		const char *wEnd = p;
		while (*wEnd && *wEnd != ' ') ++wEnd;
		const int wLen = (int)(wEnd - p);

		// Does it fit on the current line? (with separating space).
		const int sepLen = curLen > 0 ? 1 : 0;
		if (curLen + sepLen + wLen <= maxLineChars) {
			if (sepLen) cur[curLen++] = ' ';
			memcpy(cur + curLen, p, wLen);
			curLen += wLen;
			cur[curLen] = 0;
			p = wEnd;
			continue;
		}

		// Doesn't fit — flush current line and start a new one. If the
		// single word is longer than a line (shouldn't happen for game
		// titles in practice), hard-break it.
		if (curLen > 0) {
			pushLine(cur);
			curLen = 0;
			cur[0] = 0;
			continue;
		}
		const int take = wLen > maxLineChars ? maxLineChars : wLen;
		memcpy(cur, p, take);
		curLen = take;
		cur[curLen] = 0;
		p += take;
	}

	// Append "..." to the last buffered line (or push it alone).
	if (lineCount < kMaxLines) {
		const int dotsLen = 3;
		if (curLen + dotsLen <= maxLineChars) {
			memcpy(cur + curLen, "...", dotsLen);
			curLen += dotsLen;
			cur[curLen] = 0;
			pushLine(cur);
		} else {
			if (curLen > 0) pushLine(cur);
			if (lineCount < kMaxLines) pushLine("...");
		}
	}

	const int lineH = 9;  // 7-px glyph + 2-px leading
	const int textBlockH = lineCount * lineH - 2;

	int winW = widestChars * kGlyphW + 2 * pad_x;
	if (winW > maxWinW) winW = maxWinW;
	const int winH = textBlockH + 2 * pad_y;
	const int winX = (SW - winW) / 2;
	const int winY = (SH - winH) / 2;

	// Fill + 1-px accent border.
	fillRect(surf, winX, winY, winW, winH, kColBg);
	fillRect(surf, winX,              winY,              winW, 1, kColAccent);
	fillRect(surf, winX,              winY + winH - 1,   winW, 1, kColAccent);
	fillRect(surf, winX,              winY,              1,    winH, kColAccent);
	fillRect(surf, winX + winW - 1,   winY,              1,    winH, kColAccent);

	// Each line is horizontally centered inside the window.
	for (int i = 0; i < lineCount; ++i) {
		const int lineChars = (int)strlen(lines[i]);
		const int lineW     = lineChars * kGlyphW;
		const int x         = winX + (winW - lineW) / 2;
		const int y         = winY + pad_y + i * lineH;
		drawText(surf, x, y, lines[i], kColText);
	}

	g_system->unlockScreen();
	g_system->updateScreen();
}

int frank_quest_run_selector(const QuestGame *games, int count,
                             int initialIndex) {
	// CMM2: RUN "scummvm.app", folder starts the game in /quest/folder at once; the list comes after it, as usual
	static bool named_done = false;
	if (!named_done) {
		named_done = true;
		if (cmm2_start && cmm2_start->argc > 1) {
			for (int i = 0; i < count; ++i) {
				const char *d = strrchr(games[i].dirPath, '/');
				if (strcasecmp_ascii(d ? d + 1 : games[i].dirPath, cmm2_start->argv[1]) == 0)
					return i;
			}
			printf("FRANK Quest: no game in /quest/%s\n", cmm2_start->argv[1]);
		}
	}

	installPalette();

	const int SW = g_system->getWidth();
	const int SH = g_system->getHeight();
	Layout L = computeLayout(SW, SH, count);

	int selected = initialIndex;
	if (selected < 0) selected = 0;
	if (selected >= count) selected = count > 0 ? count - 1 : 0;
	int scroll = 0;
	if (selected >= L.visibleLines) scroll = selected - L.visibleLines + 1;

	renderFrame(games, count, selected, scroll, L);

	// Poll events directly from the rp2350 OSystem — EventManager
	// isn't set up this early and we only need raw key navigation.
	OSystem_RP2350 *sys = static_cast<OSystem_RP2350 *>(g_system);

	while (true) {
		Common::Event ev;
		bool dirty = false;
		while (sys->pollEvent(ev)) {
			if (ev.type != Common::EVENT_KEYDOWN) continue;
			if (count <= 0) continue;

			int prevSel = selected;
			int prevScroll = scroll;

			switch (ev.kbd.keycode) {
			case Common::KEYCODE_UP:
				// Wrap: from item 0, Up goes to the last item.
				selected = (selected - 1 + count) % count;
				break;
			case Common::KEYCODE_DOWN:
				// Wrap: from last item, Down returns to item 0.
				selected = (selected + 1) % count;
				break;
			case Common::KEYCODE_PAGEUP:
				selected -= L.visibleLines;
				if (selected < 0) selected = 0;
				break;
			case Common::KEYCODE_PAGEDOWN:
				selected += L.visibleLines;
				if (selected >= count) selected = count - 1;
				break;
			case Common::KEYCODE_HOME:
				selected = 0;
				break;
			case Common::KEYCODE_END:
				selected = count - 1;
				break;
			case Common::KEYCODE_RETURN:
			case Common::KEYCODE_KP_ENTER:
				return selected;
			default:
				break;
			}

			// Keep selection visible.
			if (selected < scroll) scroll = selected;
			if (selected >= scroll + L.visibleLines)
				scroll = selected - L.visibleLines + 1;

			if (selected != prevSel || scroll != prevScroll) dirty = true;
		}

		if (dirty) {
			renderFrame(games, count, selected, scroll, L);
		}
		g_system->delayMillis(16);
	}
}
