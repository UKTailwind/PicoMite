/*
 * CMM2: a menu of save slots for SCUMM games, and a message box, drawn over the game (frank_quest_savemenu.h).
 * They draw on the backend's screen in the game's own palette, with the selector's font, and put the
 * picture back afterwards. Slots 1 to 9 are the ones Alt-1 to 9 and Ctrl-1 to 9 use.
 */

#include "frank_quest_savemenu.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common/events.h"
#include "common/str.h"
#include "common/system.h"
#include "graphics/palette.h"
#include "graphics/surface.h"
#include "scumm/scumm.h"

void frankQuestDrawText(Graphics::Surface *surf, int x, int y, const char *s, uint8_t color);
void frankQuestFillRect(Graphics::Surface *surf, int x, int y, int w, int h, uint8_t color);

namespace {

const int kCharW = 6;       // the selector's 5 x 7 font
const int kLineH = 10;
const int kSlots = 9;

/* the picture under the box, put back when this goes */
class ScreenCover {
public:
	ScreenCover() : _copy(nullptr), _size(0) {
		Graphics::Surface *s = g_system->lockScreen();
		if (s && s->getPixels() && s->getWidth() > 0) {
			_size = (size_t)s->getPitch() * s->getHeight();
			_copy = (byte *)malloc(_size);
			if (_copy)
				memcpy(_copy, s->getPixels(), _size);
		}
		g_system->unlockScreen();
	}
	~ScreenCover() {
		if (_copy) {
			Graphics::Surface *s = g_system->lockScreen();
			memcpy(s->getPixels(), _copy, _size);
			g_system->unlockScreen();
			g_system->updateScreen();
			free(_copy);
		}
	}
	bool ok() const { return _copy != nullptr; }

private:
	byte *_copy;
	size_t _size;
};

/* the darkest and the brightest colours of the game's palette as it is now */
void pickColours(uint8_t &dark, uint8_t &light) {
	byte pal[256 * 3];
	int lo = 1 << 30, hi = -1;
	dark = 0;
	light = 255;
	g_system->getPaletteManager()->grabPalette(pal, 0, 256);
	for (int i = 0; i < 256; i++) {
		int l = pal[i * 3] * 3 + pal[i * 3 + 1] * 6 + pal[i * 3 + 2];
		if (l < lo) {
			lo = l;
			dark = (uint8_t)i;
		}
		if (l > hi) {
			hi = l;
			light = (uint8_t)i;
		}
	}
}

void panel(Graphics::Surface *s, int x, int y, int w, int h, uint8_t dark, uint8_t light) {
	frankQuestFillRect(s, x, y, w, h, light);
	frankQuestFillRect(s, x + 1, y + 1, w - 2, h - 2, dark);
}

} // namespace

void frankQuestMessage(const char *text, uint32 ms) {
	ScreenCover cover;
	if (!cover.ok())
		return;
	char lines[6][52];
	int n = 0, widest = 0;
	for (const char *p = text; *p && n < 6;) {
		const char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		if (len > 0) {
			int c = len < 51 ? len : 51;
			memcpy(lines[n], p, c);
			lines[n][c] = 0;
			if (c > widest)
				widest = c;
			n++;
		}
		p += len + (e ? 1 : 0);
	}
	if (n == 0)
		return;
	uint8_t dark, light;
	pickColours(dark, light);
	Graphics::Surface *s = g_system->lockScreen();
	int w = widest * kCharW + 12, h = n * kLineH + 8;
	int x = (s->getWidth() - w) / 2, y = (s->getHeight() - h) / 2;
	panel(s, x, y, w, h, dark, light);
	for (int i = 0; i < n; i++)
		frankQuestDrawText(s, x + 6, y + 5 + i * kLineH, lines[i], light);
	g_system->unlockScreen();
	uint32 end = g_system->getMillis() + ms;
	while (g_system->getMillis() < end) {
		Common::Event ev;
		bool done = false;
		while (g_system->getEventManager()->pollEvent(ev))
			if (ev.type == Common::EVENT_KEYDOWN || ev.type == Common::EVENT_LBUTTONDOWN || ev.type == Common::EVENT_QUIT)
				done = true;
		if (done)
			break;
		g_system->updateScreen();
		g_system->delayMillis(16);
	}
}

int FrankQuestSaveMenu::runModal() {
	ScreenCover cover;
	if (!cover.ok())
		return 0;
	Common::String names[kSlots + 1];
	bool used[kSlots + 1];
	for (int i = 1; i <= kSlots; i++)
		used[i] = _vm->getSavegameName(i, names[i]);
	const bool canSave = _vm->canSaveGameStateCurrently();
	const bool canLoad = _vm->canLoadGameStateCurrently();
	uint8_t dark, light;
	pickColours(dark, light);

	Graphics::Surface *s = g_system->lockScreen();
	const int w = 42 * kCharW + 12, h = (kSlots + 6) * kLineH + 8;
	const int x = (s->getWidth() - w) / 2, y = (s->getHeight() - h) / 2;
	const int firstRow = y + 5 + 2 * kLineH;
	g_system->unlockScreen();

	int sel = 1;
	bool confirm = false, redraw = true, done = false;
	bool f5held = true;                                 // the F5 that opened this: a held key repeats after 400 ms
	const char *status = canSave ? "" : "SAVING IS NOT POSSIBLE JUST NOW";

	while (!done && !_vm->shouldQuit()) {
		if (redraw) {
			char b[64];
			s = g_system->lockScreen();
			panel(s, x, y, w, h, dark, light);
			frankQuestDrawText(s, x + 6, y + 5, "SAVE OR RESTORE", light);
			for (int i = 1; i <= kSlots; i++) {
				int ry = firstRow + (i - 1) * kLineH;
				snprintf(b, sizeof(b), "%d  %.36s", i, used[i] ? names[i].c_str() : "(EMPTY)");
				if (i == sel)
					frankQuestFillRect(s, x + 3, ry - 1, w - 6, kLineH - 1, light);
				frankQuestDrawText(s, x + 6, ry, b, i == sel ? dark : light);
			}
			frankQuestDrawText(s, x + 6, firstRow + (kSlots + 1) * kLineH - 6, status, light);
			frankQuestDrawText(s, x + 6, firstRow + (kSlots + 2) * kLineH - 6, "UP/DOWN OR 1-9: CHOOSE A SLOT", light);
			frankQuestDrawText(s, x + 6, firstRow + (kSlots + 3) * kLineH - 6, "S: SAVE    R: RESTORE    ESC: BACK", light);
			g_system->unlockScreen();
			redraw = false;
		}
		Common::Event ev;
		while (g_system->getEventManager()->pollEvent(ev)) {
			if (ev.type == Common::EVENT_QUIT) {
				done = true;
			} else if (ev.type == Common::EVENT_KEYUP) {
				if (ev.kbd.keycode == Common::KEYCODE_F5)
					f5held = false;
			} else if (ev.type == Common::EVENT_LBUTTONDOWN) {
				int r = (ev.mouse.y - firstRow) / kLineH + 1;
				if (ev.mouse.y >= firstRow && r >= 1 && r <= kSlots) {
					sel = r;
					confirm = false;
					status = "";
					redraw = true;
				}
			} else if (ev.type == Common::EVENT_KEYDOWN) {
				Common::KeyCode k = ev.kbd.keycode;
				if (k == Common::KEYCODE_F5 && f5held)
					continue;
				if (confirm && k == Common::KEYCODE_s)
					continue;                           // (the S that asked, repeating)
				if (confirm) {
					confirm = false;
					status = "";
					redraw = true;
					if (k == Common::KEYCODE_y) {
						k = Common::KEYCODE_INVALID;
						done = true;
						TimeDate t;
						char d[32];
						g_system->getTimeAndDate(t);
						snprintf(d, sizeof(d), "%02d-%02d-%04d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min);
						_vm->requestSave(sel, d);
					}
					continue;
				}
				if (k == Common::KEYCODE_ESCAPE || k == Common::KEYCODE_F5) {
					done = true;
				} else if (k == Common::KEYCODE_UP || k == Common::KEYCODE_DOWN) {
					sel = (sel + (k == Common::KEYCODE_UP ? kSlots - 2 : 0)) % kSlots + 1;
					status = "";
					redraw = true;
				} else if (k >= Common::KEYCODE_1 && k <= Common::KEYCODE_9) {
					sel = k - Common::KEYCODE_0;
					status = "";
					redraw = true;
				} else if (k == Common::KEYCODE_s) {
					if (!canSave) {
						status = "SAVING IS NOT POSSIBLE JUST NOW";
						redraw = true;
					} else if (used[sel]) {
						status = "REPLACE THIS SAVE? Y: YES, ANY OTHER KEY: NO";
						confirm = true;
						redraw = true;
					} else {
						done = true;
						TimeDate t;
						char d[32];
						g_system->getTimeAndDate(t);
						snprintf(d, sizeof(d), "%02d-%02d-%04d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min);
						_vm->requestSave(sel, d);
					}
				} else if (k == Common::KEYCODE_r) {
					if (!used[sel]) {
						status = "NOTHING IS SAVED IN THIS SLOT";
						redraw = true;
					} else if (!canLoad) {
						status = "RESTORING IS NOT POSSIBLE JUST NOW";
						redraw = true;
					} else {
						done = true;
						_vm->requestLoad(sel);
					}
				}
			}
		}
		g_system->updateScreen();
		g_system->delayMillis(16);
	}
	return 0;
}
