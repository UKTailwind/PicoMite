/*
 * FRANK Quest
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RP2350 OSystem implementation.
 *
 * CMM2: the I2S driver's calls are the loader's sound stream (overlay/src/main.c), and the mixer runs at its rate; the
 * mutex holds off the stream's refill (as masking the I2S DMA interrupt did); the date and time are the CMM2's clock.
 */

#include "backends/platform/rp2350/rp2350-system.h"
#include "backends/platform/rp2350/rp2350-minimal.h"
#include "backends/fs/rp2350/rp2350-fs-factory.h"
#include "backends/events/default/default-events.h"
#include "backends/timer/default/default-timer.h"
#include "backends/audiocd/rp2350/rp2350-audiocd.h"
#include "backends/saves/default/default-saves.h"
#include "common/config-manager.h"
#include "audio/mixer_intern.h"

#ifdef USE_I2S_AUDIO
extern "C" {
#include "audio.h"
}
#endif

#include "cmm2_port.h"

#include <stdio.h>
#include <string.h>

// Global mixer pointer for audio callback
static Audio::MixerImpl *g_mixer = nullptr;

// Audio callback function called from C audio driver
#ifdef USE_I2S_AUDIO
extern "C" void cabal_audio_set_mixer_callback(void (*callback)(uint8_t *stream, int len));

static void mixer_callback_wrapper(uint8_t *stream, int len) {
	if (g_mixer) {
		g_mixer->mixCallback(stream, len);
	}
}
#endif

// Graphics mode
static const OSystem::GraphicsMode s_supportedGraphicsModes[] = {
	{"1x", "Normal", 1},
	{0, 0, 0}
};

OSystem_RP2350::OSystem_RP2350()
	: _screenWidth(320), _screenHeight(200), _shakeOffset(0),
	  _overlayVisible(false), _paletteDirty(false),
	  _mouseX(160), _mouseY(100), _mouseVisible(false),
	  _mouseButtons(0), _prevMouseButtons(0),
	  _mixer(nullptr), _startTime(0), _quitRequested(false) {

	memset(&_cursor, 0, sizeof(_cursor));
	memset(_palette, 0, sizeof(_palette));
}

OSystem_RP2350::~OSystem_RP2350() {
	// Order is load-bearing.
	//
	// 1. Silence the audio path first. The I2S DMA IRQ calls
	//    mixer_callback_wrapper which dereferences g_mixer; if we
	//    delete _mixer while the IRQ is still armed the next DMA
	//    completion dives through a freed vtable and hard-faults.
#ifdef USE_I2S_AUDIO
	cabal_audio_set_mixer_callback(NULL);
	cabal_audio_shutdown();
#endif
	g_mixer = nullptr;

	if (_cursor.data) {
		delete[] _cursor.data;
	}
	if (_mixer) {
		delete _mixer;
	}
	_screen.reset();
	_overlay.reset();

	// 2. Destroy OSystem subsystems that need our virtual mutex
	//    methods BEFORE the base-class destructor runs. Once ~OSystem
	//    starts, the vtable reverts to OSystem's and the pure-virtual
	//    lockMutex/unlockMutex stubs are reached through g_system,
	//    which crashes at PC=0.
	//
	//    DefaultTimerManager::~DefaultTimerManager takes a StackLock
	//    on its own mutex → calls g_system->lockMutex(). Similarly
	//    DefaultEventManager's timer callback path. Tear them down
	//    here while our vtable is still active, then null them so
	//    ~OSystem's own `delete _timerManager` is a no-op.
	delete _timerManager;
	_timerManager = nullptr;

	delete _eventManager;
	_eventManager = nullptr;

	delete _savefileManager;
	_savefileManager = nullptr;

	delete _audiocdManager;
	_audiocdManager = nullptr;

	// 3. _fsFactory points at the RP2350FilesystemFactory singleton
	//    (&RP2350FilesystemFactory::instance()), not an owned
	//    heap object. Letting ~OSystem run `delete _fsFactory` would
	//    free the singleton storage and leave Singleton<>::_singleton
	//    pointing at freed memory; the next call to instance() then
	//    dereferences garbage and hardfaults. Null it here so the
	//    base destructor's delete is a no-op on nullptr.
	_fsFactory = nullptr;
}

void OSystem_RP2350::initBackend() {
	printf("OSystem_RP2350: Initializing backend...\n");

	// Initialize the minimal system
	cabal_system_init();

	// Set filesystem factory
	_fsFactory = &RP2350FilesystemFactory::instance();

	// Create event manager (OSystem_RP2350 is the EventSource)
	_eventManager = new DefaultEventManager(this);

	// Create timer manager
	_timerManager = new DefaultTimerManager();

	// Record start time
	_startTime = cabal_get_millis();

	// Create mixer at 44100 Hz (standard CD quality)
	_mixer = new Audio::MixerImpl(this, cabal_audio_rate());   // CMM2: the stream's rate (44.1 kHz as its timer makes it)
	_mixer->setReady(true);
	g_mixer = _mixer;

#ifdef USE_I2S_AUDIO
	// Initialize I2S audio driver
	printf("OSystem_RP2350: Initializing I2S audio...\n");
	cabal_audio_set_mixer_callback(mixer_callback_wrapper);
	cabal_audio_init();
#endif

	// Create AudioCD manager (stub - no CD audio on embedded)
	_audiocdManager = new RP2350AudioCDManager();

	// Create save file manager
	_savefileManager = new DefaultSaveFileManager("/quest/saves");

	printf("OSystem_RP2350: Backend initialized.\n");
	OSystem::initBackend();
}

bool OSystem_RP2350::hasFeature(Feature f) {
	switch (f) {
	case kFeatureCursorPalette:
		return true;
	default:
		return false;
	}
}

void OSystem_RP2350::setFeatureState(Feature f, bool enable) {
	// Not implemented
}

bool OSystem_RP2350::getFeatureState(Feature f) {
	return false;
}

// Graphics

const OSystem::GraphicsMode *OSystem_RP2350::getSupportedGraphicsModes() const {
	return s_supportedGraphicsModes;
}

int OSystem_RP2350::getDefaultGraphicsMode() const {
	return 1;
}

bool OSystem_RP2350::setGraphicsMode(int mode) {
	return true;
}

int OSystem_RP2350::getGraphicsMode() const {
	return 1;
}

void OSystem_RP2350::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	printf("OSystem_RP2350: initSize(%u, %u)\n", width, height);

	_screenWidth = width;
	_screenHeight = height;

	// Initialize the minimal backend graphics
	cabal_init_graphics(width, height);

	// Create screen surface
	_screen.create(width, height, Graphics::PixelFormat::createFormatCLUT8());

	// Create overlay surface (same size, 8bpp for simplicity)
	_overlay.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
}

int16 OSystem_RP2350::getHeight() {
	return _screenHeight;
}

int16 OSystem_RP2350::getWidth() {
	return _screenWidth;
}

PaletteManager *OSystem_RP2350::getPaletteManager() {
	return this;
}

void OSystem_RP2350::setPalette(const byte *colors, uint start, uint num) {
	memcpy(_palette + start * 3, colors, num * 3);
	_paletteDirty = true;
}

void OSystem_RP2350::grabPalette(byte *colors, uint start, uint num) {
	memcpy(colors, _palette + start * 3, num * 3);
}

void OSystem_RP2350::updatePalette() {
	if (_paletteDirty) {
		cabal_set_palette(_palette, 0, 256);
		_paletteDirty = false;
	}
}

void OSystem_RP2350::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	if (!_screen.getPixels()) return;

	const byte *src = (const byte *)buf;
	byte *dst = (byte *)_screen.getBasePtr(x, y);
	int dstPitch = _screen.getPitch();

	for (int row = 0; row < h; row++) {
		memcpy(dst, src, w);
		src += pitch;
		dst += dstPitch;
	}
}

Graphics::Surface *OSystem_RP2350::lockScreen() {
	return &_screen;
}

void OSystem_RP2350::unlockScreen() {
	// Nothing to do
}

void OSystem_RP2350::fillScreen(uint32 col) {
	if (_screen.getPixels()) {
		memset(_screen.getPixels(), col, _screen.getWidth() * _screen.getHeight());
	}
}

void OSystem_RP2350::updateScreen() {
	// Update palette if needed
	updatePalette();

	// Copy to minimal backend
	CabalSurface *cabalScreen = cabal_lock_screen();
	if (cabalScreen && cabalScreen->pixels && _screen.getPixels()) {
		const byte *src;
		if (_overlayVisible && _overlay.getPixels()) {
			src = (const byte *)_overlay.getPixels();
		} else {
			src = (const byte *)_screen.getPixels();
		}
		memcpy(cabalScreen->pixels, src, _screenWidth * _screenHeight);
	}
	cabal_unlock_screen();

	// Update cursor position
	cabal_set_mouse_pos(_mouseX, _mouseY);

	// Push to display
	cabal_update_screen();

	// Drive the DefaultTimerManager — SCI's music/sound engine and
	// several other subsystems register ~60 Hz callbacks here, and
	// without the pulse cutscenes and fades stall mid-sequence.
	if (_timerManager)
		static_cast<DefaultTimerManager *>(_timerManager)->handler();

#ifdef USE_I2S_AUDIO
	// Process audio - mix and send to I2S
	cabal_audio_process_frame();
#endif
}

void OSystem_RP2350::setShakePos(int shakeOffset) {
	_shakeOffset = shakeOffset;
}

// Overlay

void OSystem_RP2350::showOverlay() {
	_overlayVisible = true;
	cabal_show_overlay();
}

void OSystem_RP2350::hideOverlay() {
	_overlayVisible = false;
	cabal_hide_overlay();
}

Graphics::PixelFormat OSystem_RP2350::getOverlayFormat() const {
	return Graphics::PixelFormat::createFormatCLUT8();
}

void OSystem_RP2350::clearOverlay() {
	if (_overlay.getPixels()) {
		// Copy screen to overlay
		memcpy(_overlay.getPixels(), _screen.getPixels(), _screenWidth * _screenHeight);
	}
}

void OSystem_RP2350::grabOverlay(void *buf, int pitch) {
	if (!_overlay.getPixels()) return;

	byte *dst = (byte *)buf;
	const byte *src = (const byte *)_overlay.getPixels();

	for (int y = 0; y < _screenHeight; y++) {
		memcpy(dst, src, _screenWidth);
		dst += pitch;
		src += _overlay.getPitch();
	}
}

void OSystem_RP2350::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	if (!_overlay.getPixels()) return;

	const byte *src = (const byte *)buf;
	byte *dst = (byte *)_overlay.getBasePtr(x, y);
	int dstPitch = _overlay.getPitch();

	for (int row = 0; row < h; row++) {
		memcpy(dst, src, w);
		src += pitch;
		dst += dstPitch;
	}
}

int16 OSystem_RP2350::getOverlayHeight() {
	return _screenHeight;
}

int16 OSystem_RP2350::getOverlayWidth() {
	return _screenWidth;
}

// Mouse

bool OSystem_RP2350::showMouse(bool visible) {
	bool prev = _mouseVisible;
	if (visible != prev) {
		printf("showMouse(%d)\n", visible);
	}
	_mouseVisible = visible;
	cabal_show_mouse(visible);
	return prev;
}

void OSystem_RP2350::warpMouse(int x, int y) {
	_mouseX = x;
	_mouseY = y;
	cabal_set_mouse_pos(x, y);
}

void OSystem_RP2350::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY,
                                    uint32 keycolor, bool dontScale,
                                    const Graphics::PixelFormat *format) {
	if (_cursor.data) {
		delete[] _cursor.data;
	}

	_cursor.w = w;
	_cursor.h = h;
	_cursor.hotX = hotspotX;
	_cursor.hotY = hotspotY;
	_cursor.keycolor = keycolor;
	_cursor.data = new byte[w * h];
	if (!buf || !_cursor.data) {
		return;
	}
	memcpy(_cursor.data, buf, w * h);
	printf("setMouseCursor(%ux%u hot=%d,%d key=%u)\n", w, h, hotspotX, hotspotY, (unsigned)keycolor);

	cabal_set_mouse_cursor(_cursor.data, w, h, hotspotX, hotspotY, (uint8)keycolor);
}

// Events

uint32 OSystem_RP2350::getMillis() {
	return cabal_get_millis();
}

void OSystem_RP2350::delayMillis(uint msecs) {
#ifdef USE_I2S_AUDIO
	// Process audio during delays to prevent underruns
	// Call audio every ~16ms to match buffer timing (735 samples @ 44100Hz = ~16.7ms)
	while (msecs > 0) {
		uint chunk = (msecs > 16) ? 16 : msecs;
		cabal_delay(chunk);
		msecs -= chunk;
		cabal_audio_process_frame();
		if (_timerManager)
			static_cast<DefaultTimerManager *>(_timerManager)->handler();
	}
#else
	cabal_delay(msecs);
#endif
}

void OSystem_RP2350::getTimeAndDate(TimeDate &t) const {
	// CMM2: the board's clock (MMBasic's DATE$ and TIME$)
	int d[6];
	cmm2->datetime(d);
	t.tm_sec = d[5];
	t.tm_min = d[4];
	t.tm_hour = d[3];
	t.tm_mday = d[2];
	t.tm_mon = d[1] - 1;
	t.tm_year = d[0] - 1900;
	t.tm_wday = 0;
}

extern "C" int frank_quest_cad_consume(void);

bool OSystem_RP2350::pollEvent(Common::Event &event) {
	// Ctrl+Alt+Del — translate into a Common::EVENT_QUIT so ScummVM
	// engines exit run() cleanly and cabal_main() returns to the
	// selector. Consumed here before any real input so we can't lose
	// the signal to a busy input queue.
	if (frank_quest_cad_consume()) {
		event.type = Common::EVENT_QUIT;
		return true;
	}

	CabalEvent cabalEvent;
	if (!cabal_poll_event(&cabalEvent)) {
		return false;
	}

	switch (cabalEvent.type) {
	case CABAL_EVENT_KEYDOWN:
		event.type = Common::EVENT_KEYDOWN;
		event.kbd.keycode = (Common::KeyCode)cabalEvent.kbd.keycode;
		event.kbd.ascii = cabalEvent.kbd.ascii;
		event.kbd.flags = 0;
		if (cabalEvent.kbd.flags & CABAL_MOD_SHIFT) event.kbd.flags |= Common::KBD_SHIFT;
		if (cabalEvent.kbd.flags & CABAL_MOD_CTRL) event.kbd.flags |= Common::KBD_CTRL;
		if (cabalEvent.kbd.flags & CABAL_MOD_ALT) event.kbd.flags |= Common::KBD_ALT;
		return true;

	case CABAL_EVENT_KEYUP:
		event.type = Common::EVENT_KEYUP;
		event.kbd.keycode = (Common::KeyCode)cabalEvent.kbd.keycode;
		event.kbd.ascii = cabalEvent.kbd.ascii;
		event.kbd.flags = 0;
		if (cabalEvent.kbd.flags & CABAL_MOD_SHIFT) event.kbd.flags |= Common::KBD_SHIFT;
		if (cabalEvent.kbd.flags & CABAL_MOD_CTRL) event.kbd.flags |= Common::KBD_CTRL;
		if (cabalEvent.kbd.flags & CABAL_MOD_ALT) event.kbd.flags |= Common::KBD_ALT;
		return true;

	case CABAL_EVENT_MOUSEMOVE:
		event.type = Common::EVENT_MOUSEMOVE;
		event.mouse.x = _mouseX = cabalEvent.mouse.x;
		event.mouse.y = _mouseY = cabalEvent.mouse.y;
		return true;

	case CABAL_EVENT_LBUTTONDOWN:
		event.type = Common::EVENT_LBUTTONDOWN;
		event.mouse.x = _mouseX = cabalEvent.mouse.x;
		event.mouse.y = _mouseY = cabalEvent.mouse.y;
		return true;

	case CABAL_EVENT_LBUTTONUP:
		event.type = Common::EVENT_LBUTTONUP;
		event.mouse.x = _mouseX = cabalEvent.mouse.x;
		event.mouse.y = _mouseY = cabalEvent.mouse.y;
		return true;

	case CABAL_EVENT_RBUTTONDOWN:
		event.type = Common::EVENT_RBUTTONDOWN;
		event.mouse.x = _mouseX = cabalEvent.mouse.x;
		event.mouse.y = _mouseY = cabalEvent.mouse.y;
		return true;

	case CABAL_EVENT_RBUTTONUP:
		event.type = Common::EVENT_RBUTTONUP;
		event.mouse.x = _mouseX = cabalEvent.mouse.x;
		event.mouse.y = _mouseY = cabalEvent.mouse.y;
		return true;

	case CABAL_EVENT_QUIT:
		event.type = Common::EVENT_QUIT;
		_quitRequested = true;
		return true;

	default:
		return false;
	}
}

// Mutex — audio DMA IRQ guard.
//
// We have a single CPU core running engine code, but the I2S audio
// DMA completes asynchronously and re-enters the mixer callback in
// IRQ context. Engines (e.g. Gob's AdLib) read and write OPL /
// mixer state from both contexts, guarded by StackLock on OSystem's
// mutex. An earlier "no-op" implementation left every such critical
// section wide open — the mixer IRQ could preempt a register-write
// half-way and see a partially-updated Channel struct, which caused
// PC=0 / wild-pointer hardfaults deep inside dbopl (the synthHandler
// member reads as garbage because the write hasn't finished).
//
// The RP2350 has no real mutexes available at this layer (only
// hardware spinlocks, which don't help against a same-core IRQ).
// Masking just DMA_IRQ_1 (our audio IRQ line) is enough: nothing
// else touches the shared state. HDMI runs on DMA_IRQ_0 and stays
// enabled.
//
// The MutexRef holds a nesting counter bit-packed with the previous
// IRQ-enabled state so recursive locks nest cleanly.

struct CabalMutex {
	int nesting;
	bool wasEnabled;
};

OSystem::MutexRef OSystem_RP2350::createMutex() {
	return reinterpret_cast<MutexRef>(new CabalMutex{0, false});
}

void OSystem_RP2350::lockMutex(MutexRef mutex) {
	auto *m = reinterpret_cast<CabalMutex *>(mutex);
	if (!m) return;
	cmm2_sound_lock();   // CMM2: the loader's refill held off (it counts, so locks nest)
	++m->nesting;
}

void OSystem_RP2350::unlockMutex(MutexRef mutex) {
	auto *m = reinterpret_cast<CabalMutex *>(mutex);
	if (!m || m->nesting <= 0) return;
	--m->nesting;
	cmm2_sound_unlock();
}

void OSystem_RP2350::deleteMutex(MutexRef mutex) {
	delete reinterpret_cast<CabalMutex *>(mutex);
}

// Audio

Audio::Mixer *OSystem_RP2350::getMixer() {
	return _mixer;
}

// Misc

void OSystem_RP2350::quit() {
	_quitRequested = true;
}

void OSystem_RP2350::displayMessageOnOSD(const char *msg) {
	printf("OSD: %s\n", msg);
}

void OSystem_RP2350::logMessage(LogMessageType::Type type, const char *message) {
	printf("%s\n", message);
}
