/*
 * FRANK Quest
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Main entry point for RP2350; initializes the ScummVM-compatible
 * OSystem and runs the game.
 */

#include "backends/platform/rp2350/rp2350-system.h"
#include "backends/platform/rp2350/rp2350-minimal.h"
#include "frank_quest_fs.h"
#include "frank_quest_selector.h"

extern "C" {
#include "psram_allocator.h"
}

#include "common/system.h"
#include "common/config-manager.h"
#include "graphics/surface.h"

#ifdef ENABLE_AGI  // PicoMite: the SCUMM engine only (make.py)
// AGI Engine includes
#include "agi/agi.h"
#endif
#include "engines/advancedDetector.h"
#include "base/plugins.h"
#include "common/fs.h"
#include "common/archive.h"

#ifdef ENABLE_GOB  // PicoMite: the SCUMM engine only (make.py)
// GOB Engine includes
#include "gob/gob.h"
#endif

#ifdef ENABLE_KYRA  // PicoMite: the SCUMM engine only (make.py)
// Kyrandia Engine includes
#include "kyra/kyra_lok.h"
#include "kyra/kyra_hof.h"
#endif

#ifdef ENABLE_SCI  // PicoMite: the SCUMM engine only (make.py)
// SCI Engine includes
#include "sci/sci.h"
#endif

// SCUMM Engine includes (v1-v6 only — v7/v8 disabled, Full Throttle/Dig/COMI not built)
#include "scumm/scumm.h"
#include "scumm/scumm_v4.h"
#include "scumm/scumm_v5.h"
#include "scumm/scumm_v6.h"
#include "scumm/scumm_v7.h"
#include "scumm/detection.h"

#ifdef ENABLE_AGI  // PicoMite: the SCUMM engine only (make.py)
// Forward declare AGIGameDescription since it's defined in detection.cpp
namespace Agi {
struct AGIGameDescription {
    ADGameDescription desc;
    int gameID;
    int gameType;
    uint32 features;
    uint16 version;
};
}
#endif

#ifdef ENABLE_GOB  // PicoMite: the SCUMM engine only (make.py)
// Forward declare GOBGameDescription
namespace Gob {
struct GOBGameDescription {
    ADGameDescription desc;
    GameType gameType;
    int32 features;
    const char *startStkBase;
    const char *startTotBase;
    uint32 demoIndex;
};
}
#endif

#ifdef ENABLE_KYRA  // PicoMite: the SCUMM engine only (make.py)
// Forward declare KYRAGameDescription
struct KYRAGameDescription {
    ADGameDescription desc;
    Kyra::GameFlags flags;
};
#endif

#include <stdio.h>
#include <string.h>

// Global OSystem instance (declared in common/system.h)
extern OSystem *g_system;

// List files in a directory using cabal_fs
static void listDirectory(const char *path) {
    printf("Contents of %s:\n", path);

    CabalDir *dir = cabal_dir_open(path);
    if (!dir) {
        printf("  (cannot open directory)\n");
        return;
    }

    CabalDirEntry entry;
    int count = 0;
    while (cabal_dir_read(dir, &entry)) {
        if (entry.isDirectory) {
            printf("  [DIR]  %s\n", entry.name);
        } else {
            printf("  %6lu %s\n", (unsigned long)entry.size, entry.name);
        }
        count++;
    }
    cabal_dir_close(dir);

    if (count == 0) {
        printf("  (empty)\n");
    }
    printf("\n");
}

// Test pattern using OSystem API
static void drawTestPatternOSystem(void) {
    printf("Drawing test pattern via OSystem...\n");

    // Set up a test palette
    byte palette[256 * 3];
    memset(palette, 0, sizeof(palette));

    // SMPTE color bars
    // 0 = Black
    palette[0] = 0; palette[1] = 0; palette[2] = 0;
    // 1 = Red
    palette[3] = 255; palette[4] = 0; palette[5] = 0;
    // 2 = Green
    palette[6] = 0; palette[7] = 255; palette[8] = 0;
    // 3 = Blue
    palette[9] = 0; palette[10] = 0; palette[11] = 255;
    // 4 = Yellow
    palette[12] = 255; palette[13] = 255; palette[14] = 0;
    // 5 = Cyan
    palette[15] = 0; palette[16] = 255; palette[17] = 255;
    // 6 = Magenta
    palette[18] = 255; palette[19] = 0; palette[20] = 255;
    // 7 = White
    palette[21] = 255; palette[22] = 255; palette[23] = 255;

    PaletteManager *pal = g_system->getPaletteManager();
    if (pal) {
        pal->setPalette(palette, 0, 256);
    }

    // Draw color bars
    Graphics::Surface *screen = g_system->lockScreen();
    if (screen && screen->getPixels()) {
        int width = screen->getWidth();
        int height = screen->getHeight();
        int barWidth = width / 8;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int bar = x / barWidth;
                byte color;
                switch (bar) {
                    case 0: color = 7; break;  // White
                    case 1: color = 4; break;  // Yellow
                    case 2: color = 5; break;  // Cyan
                    case 3: color = 2; break;  // Green
                    case 4: color = 6; break;  // Magenta
                    case 5: color = 1; break;  // Red
                    case 6: color = 3; break;  // Blue
                    default: color = 0; break; // Black
                }
                byte *pixel = (byte *)screen->getBasePtr(x, y);
                *pixel = color;
            }
        }
    }
    g_system->unlockScreen();
    g_system->updateScreen();

    printf("Test pattern displayed.\n");
}

#ifdef ENABLE_GOB  // PicoMite: the SCUMM engine only (make.py)
// Detect which Gobliiins game version is present
// Returns: 1 = Gob1, 2 = Gob2, 3 = Gob3, 0 = unknown
static int detectGOBVersion(const char *gamePath) {
    char pathBuf[256];

    printf("GOB Detection: Checking files in %s\n", gamePath);

    // Check for Gobliiins 3 specific files
    snprintf(pathBuf, sizeof(pathBuf), "%s/gob3.stk", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found gob3.stk -> Gobliiins 3\n");
        return 3;
    }
    snprintf(pathBuf, sizeof(pathBuf), "%s/gob3cd.stk", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found gob3cd.stk -> Gobliiins 3\n");
        return 3;
    }

    // Check for Gobliiins 2 specific files
    snprintf(pathBuf, sizeof(pathBuf), "%s/gob2.stk", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found gob2.stk -> Gobliiins 2\n");
        return 2;
    }
    snprintf(pathBuf, sizeof(pathBuf), "%s/gob2cd.stk", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found gob2cd.stk -> Gobliiins 2\n");
        return 2;
    }
    snprintf(pathBuf, sizeof(pathBuf), "%s/mus_gob2.stk", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found mus_gob2.stk -> Gobliiins 2\n");
        return 2;
    }
    // Gobliiins 2 often has "track1.mp3" for CD audio
    snprintf(pathBuf, sizeof(pathBuf), "%s/track1.mp3", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found track1.mp3 -> Gobliiins 2 CD\n");
        return 2;
    }

    // Check for Gobliiins 1 specific file (gob.lic is Gob1 license file)
    snprintf(pathBuf, sizeof(pathBuf), "%s/gob.lic", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found gob.lic -> Gobliiins 1\n");
        return 1;
    }

    // Default check - intro.stk exists in all versions
    snprintf(pathBuf, sizeof(pathBuf), "%s/intro.stk", gamePath);
    if (cabal_path_exists(pathBuf)) {
        printf("GOB Detection: Found intro.stk, defaulting to Gobliiins 1\n");
        return 1;
    }

    printf("GOB Detection: No known files found\n");
    return 0;  // Unknown
}

// Forward declaration
static bool launchGOBGameWithVersion(const char *gamePath, int gobVersion);

// GOB (Gobliins) game launcher with auto-detection
// Returns true if a game was found and launched
static bool launchGOBGame(const char *gamePath) {
    printf("GOB: Attempting to launch game from %s\n", gamePath);

    // Detect which GOB game version
    int gobVersion = detectGOBVersion(gamePath);
    printf("GOB: Detected game version: Gobliiins %d\n", gobVersion);

    if (gobVersion == 0) {
        printf("GOB: Could not detect game version, defaulting to Gobliiins 1\n");
        gobVersion = 1;
    }

    return launchGOBGameWithVersion(gamePath, gobVersion);
}

// GOB (Gobliins) game launcher with explicit version
// Returns true if a game was found and launched
static bool launchGOBGameWithVersion(const char *gamePath, int gobVersion) {
    printf("GOB: Launching game from %s as Gobliiins %d\n", gamePath, gobVersion);

    // Set up the game path in config manager
    ConfMan.set("path", gamePath);
    ConfMan.setActiveDomain("quest-gob");

    // Set default audio/config values that Engine::syncSoundSettings() expects
    ConfMan.setInt("music_volume", 192);
    ConfMan.setInt("sfx_volume", 192);
    ConfMan.setInt("speech_volume", 192);
    ConfMan.setBool("mute", false);
    ConfMan.setBool("speech_mute", false);
    ConfMan.setBool("sfx_mute", false);
    ConfMan.setBool("music_mute", false);
    ConfMan.setInt("autosave_period", 0);  // Disable autosave on embedded
    ConfMan.setBool("enable_unsupported_game_warning", false);

    // Set music driver to AdLib for OPL emulation
    ConfMan.set("music_driver", "adlib");
    ConfMan.set("gm_device", "null");
    ConfMan.set("mt32_device", "null");
    ConfMan.setBool("native_mt32", false);
    ConfMan.setBool("enable_gs", false);
    ConfMan.setBool("multi_midi", false);
    ConfMan.setInt("midi_gain", 100);
    ConfMan.setBool("subtitles", true);
    ConfMan.setInt("talkspeed", 60);

    // GOB-specific settings
    ConfMan.setBool("copy_protection", false);  // Skip copy protection
    ConfMan.set("language", "en");
    ConfMan.set("opl_driver", "auto");

    // Graphics settings
    ConfMan.set("gfx_mode", "normal");
    ConfMan.setBool("aspect_ratio", false);
    ConfMan.setBool("fullscreen", false);
    ConfMan.setBool("filtering", false);

    // Load all plugins (engine and music) before creating the engine
    printf("GOB: Loading plugins...\n");
    PluginManager::instance().loadAllPlugins();
    printf("GOB: Plugins loaded.\n");

    // Add game directory to SearchManager so engine can find files
    printf("GOB: Setting up search path...\n");
    Common::FSNode gameDir(gamePath);
    if (gameDir.exists() && gameDir.isDirectory()) {
        SearchMan.addDirectory(gamePath, gameDir, 0, 4);
        printf("GOB: Added %s to search path\n", gamePath);

        // List files in the directory for debugging
        Common::FSList files;
        if (gameDir.getChildren(files, Common::FSNode::kListFilesOnly)) {
            printf("GOB: Found %d files in game directory:\n", (int)files.size());
            for (Common::FSList::iterator it = files.begin(); it != files.end(); ++it) {
                printf("  - %s\n", it->getName().c_str());
            }
        }
    } else {
        printf("GOB: WARNING - Game directory not found or not accessible!\n");
    }

    // Create a minimal GOB game description
    static Gob::GOBGameDescription gameDesc;
    memset(&gameDesc, 0, sizeof(gameDesc));

    // Set up the ADGameDescription part based on detected version
    if (gobVersion == 3) {
        gameDesc.desc.gameid = "gob3";
        gameDesc.desc.extra = "VGA";
        gameDesc.gameType = Gob::kGameTypeGob3;
        gameDesc.features = Gob::kFeaturesAdLib;
        printf("GOB: Configuring for Gobliiins 3 (VGA)\n");
    } else if (gobVersion == 2) {
        gameDesc.desc.gameid = "gob2";
        gameDesc.desc.extra = "VGA";
        gameDesc.gameType = Gob::kGameTypeGob2;
        gameDesc.features = Gob::kFeaturesAdLib;
        printf("GOB: Configuring for Gobliiins 2 (VGA)\n");
    } else {
        gameDesc.desc.gameid = "gob1";
        gameDesc.desc.extra = "EGA";
        gameDesc.gameType = Gob::kGameTypeGob1;
        gameDesc.features = Gob::kFeaturesEGA | Gob::kFeaturesAdLib;
        printf("GOB: Configuring for Gobliiins 1 (EGA)\n");
    }

    gameDesc.desc.language = Common::EN_ANY;
    gameDesc.desc.platform = Common::kPlatformDOS;
    gameDesc.desc.flags = ADGF_NO_FLAGS;
    gameDesc.desc.guioptions = "";
    gameDesc.startStkBase = 0;
    gameDesc.startTotBase = 0;
    gameDesc.demoIndex = 0;

    printf("GOB: Creating engine instance...\n");

    // Create the engine - GOB uses a two-step init process
    Gob::GobEngine *gobEngine = new Gob::GobEngine(g_system);
    gobEngine->initGame(&gameDesc);

    // Cast to Engine* to access public run() method
    ::Engine *engine = gobEngine;

    printf("GOB: Running game...\n");

    // Run the engine (this calls init() + go() internally)
    Common::Error err = engine->run();

    printf("GOB: Game finished with code %d (%s)\n", err.getCode(), err.getDesc().c_str());
    delete engine;
    return (err.getCode() == Common::kNoError);
}
#endif

#ifdef ENABLE_AGI  // PicoMite: the SCUMM engine only (make.py)
// AGI game launcher
// Returns true if a game was found and launched
static bool launchAGIGame(const char *gamePath) {
    printf("AGI: Attempting to launch game from %s\n", gamePath);

    // Set up the game path in config manager
    ConfMan.set("path", gamePath);
    ConfMan.setActiveDomain("quest-agi");

    // Set default audio/config values that Engine::syncSoundSettings() expects
    ConfMan.setInt("music_volume", 192);
    ConfMan.setInt("sfx_volume", 192);
    ConfMan.setInt("speech_volume", 192);
    ConfMan.setBool("mute", false);
    ConfMan.setBool("speech_mute", false);
    ConfMan.setInt("autosave_period", 0);  // Disable autosave on embedded
    ConfMan.setBool("originalsaveload", true);      // CMM2: the game's own screens (see launchSciGame)
    ConfMan.setBool("enable_unsupported_game_warning", false);

    // Set music driver to AdLib for OPL emulation
    ConfMan.set("music_driver", "adlib");
    ConfMan.set("gm_device", "null");
    ConfMan.set("mt32_device", "null");
    ConfMan.setBool("native_mt32", false);
    ConfMan.setBool("enable_gs", false);
    ConfMan.setBool("multi_midi", false);
    ConfMan.setInt("midi_gain", 100);
    ConfMan.setBool("subtitles", true);
    ConfMan.setInt("talkspeed", 60);

    // Load all plugins (engine and music) before creating the engine
    printf("AGI: Loading plugins...\n");
    PluginManager::instance().loadAllPlugins();
    printf("AGI: Plugins loaded.\n");

    // Add game directory to SearchManager so engine can find files
    printf("AGI: Setting up search path...\n");
    Common::FSNode gameDir(gamePath);
    if (gameDir.exists() && gameDir.isDirectory()) {
        SearchMan.addDirectory(gamePath, gameDir, 0, 4);
        printf("AGI: Added %s to search path\n", gamePath);

        // List files in the directory for debugging
        Common::FSList files;
        if (gameDir.getChildren(files, Common::FSNode::kListFilesOnly)) {
            printf("AGI: Found %d files in game directory:\n", (int)files.size());
            for (Common::FSList::iterator it = files.begin(); it != files.end(); ++it) {
                printf("  - %s\n", it->getName().c_str());
            }
        }
    } else {
        printf("AGI: WARNING - Game directory not found or not accessible!\n");
    }

    // Create a minimal AGI game description for AGI v2 games
    // This is for King's Quest, Space Quest, etc.
    static Agi::AGIGameDescription gameDesc;
    memset(&gameDesc, 0, sizeof(gameDesc));

    // Set up the ADGameDescription part
    gameDesc.desc.gameid = "agi-fanmade";  // Generic AGI
    gameDesc.desc.extra = "";
    gameDesc.desc.language = Common::EN_ANY;
    gameDesc.desc.platform = Common::kPlatformDOS;
    gameDesc.desc.flags = ADGF_NO_FLAGS;
    gameDesc.desc.guioptions = "";

    // Set AGI-specific fields
    gameDesc.gameID = Agi::GID_FANMADE;
    gameDesc.gameType = Agi::GType_V2;  // Most common AGI games are v2
    gameDesc.features = 0;
    gameDesc.version = 0x2917;  // AGI 2.917

    printf("AGI: Creating engine instance...\n");

    // Create the engine (use Engine pointer to access public run())
    ::Engine *engine = new Agi::AgiEngine(g_system, &gameDesc);

    printf("AGI: Running game...\n");

    // Run the engine (this calls init() + go() internally)
    Common::Error err = engine->run();

    printf("AGI: Game finished with code %d\n", err.getCode());
    delete engine;
    return (err.getCode() == Common::kNoError);
}
#endif

// Event loop using OSystem API
static void eventLoopOSystem(void) {
    printf("Entering event loop. Move mouse, press keys to test input.\n");
    printf("Press ESC to exit.\n");

    bool running = true;
    int frameCount = 0;
    uint32 lastTick = g_system->getMillis();

    g_system->showMouse(true);

    Common::EventManager *eventMan = g_system->getEventManager();

    while (running) {
        Common::Event event;

        // Poll events directly from OSystem_RP2350 since EventManager may not be set up
        OSystem_RP2350 *rp2350sys = static_cast<OSystem_RP2350 *>(g_system);
        while (rp2350sys->pollEvent(event)) {
            switch (event.type) {
                case Common::EVENT_KEYDOWN:
                    printf("Key down: %d (0x%02x)\n", event.kbd.keycode, event.kbd.keycode);
                    if (event.kbd.keycode == Common::KEYCODE_ESCAPE) {
                        running = false;
                    }
                    break;

                case Common::EVENT_KEYUP:
                    printf("Key up: %d\n", event.kbd.keycode);
                    break;

                case Common::EVENT_MOUSEMOVE:
                    if (frameCount % 30 == 0) {
                        printf("Mouse: %d, %d\n", event.mouse.x, event.mouse.y);
                    }
                    break;

                case Common::EVENT_LBUTTONDOWN:
                    printf("Left button down at %d, %d\n", event.mouse.x, event.mouse.y);
                    break;

                case Common::EVENT_LBUTTONUP:
                    printf("Left button up\n");
                    break;

                case Common::EVENT_RBUTTONDOWN:
                    printf("Right button down at %d, %d\n", event.mouse.x, event.mouse.y);
                    break;

                case Common::EVENT_RBUTTONUP:
                    printf("Right button up\n");
                    break;

                case Common::EVENT_QUIT:
                    running = false;
                    break;

                default:
                    break;
            }
        }

        // Update screen
        g_system->updateScreen();

        // Frame timing
        frameCount++;
        uint32 now = g_system->getMillis();
        if (now - lastTick >= 1000) {
            printf("FPS: %d\n", frameCount);
            frameCount = 0;
            lastTick = now;
        }

        // Small delay to prevent busy loop (~60 FPS target)
        g_system->delayMillis(16);
    }
}

// Called from main.c after hardware init
extern "C" void cabal_init(void) {
    printf("Cabal: Initializing OSystem backend...\n");

    // Create and initialize the OSystem
    g_system = new OSystem_RP2350();
    g_system->initBackend();

    // Initialize graphics (320x200 for classic games)
    g_system->initSize(320, 200, nullptr);

    // Initialize filesystem (SD card via FatFS).
    printf("FRANK Quest: Initializing filesystem...\n");
    CabalFsResult fsResult = cabal_fs_init();
    if (fsResult == CABAL_FS_OK) {
        printf("FRANK Quest: Filesystem ready.\n");

        if (cabal_path_exists("/quest")) {
            listDirectory("/quest");
        } else {
            printf("Note: /quest directory not found on SD card.\n");
        }
    } else {
        printf("FRANK Quest: Filesystem init failed (error=%d)\n", fsResult);
    }

    // Create save directory (must be after filesystem init).
    cabal_mkdir("/quest/saves");

    printf("FRANK Quest: System ready.\n");
}

#ifdef ENABLE_KYRA  // PicoMite: the SCUMM engine only (make.py)
// Kyrandia game launcher
// gameId: 0 = Kyrandia 1 (Legend of Kyrandia), 1 = Kyrandia 2 (Hand of Fate).
static bool launchKyrandiaGame(const char *gamePath, int gameId) {
    printf("KYRA: Launching %s from %s\n",
           gameId == 1 ? "Kyrandia 2 (Hand of Fate)" : "Kyrandia 1 (Legend of Kyrandia)",
           gamePath);

    ConfMan.set("path", gamePath);
    ConfMan.setActiveDomain("quest-kyra");

    // Audio/config defaults
    ConfMan.setInt("music_volume", 192);
    ConfMan.setInt("sfx_volume", 192);
    ConfMan.setInt("speech_volume", 192);
    ConfMan.setBool("mute", false);
    ConfMan.setBool("speech_mute", false);
    ConfMan.setBool("sfx_mute", false);
    ConfMan.setBool("music_mute", false);
    ConfMan.setInt("autosave_period", 0);
    ConfMan.setBool("enable_unsupported_game_warning", false);
    ConfMan.set("music_driver", "adlib");
    ConfMan.set("gm_device", "null");
    ConfMan.set("mt32_device", "null");
    ConfMan.setBool("native_mt32", false);
    ConfMan.setBool("subtitles", true);
    ConfMan.setInt("talkspeed", 60);
    ConfMan.set("language", "en");
    ConfMan.set("gfx_mode", "normal");
    ConfMan.setBool("aspect_ratio", false);

    printf("KYRA: Loading plugins...\n");
    PluginManager::instance().loadAllPlugins();

    // Add game directory to search path
    Common::FSNode gameDir(gamePath);
    if (gameDir.exists() && gameDir.isDirectory()) {
        SearchMan.addDirectory(gamePath, gameDir, 0, 4);
        printf("KYRA: Added %s to search path\n", gamePath);
    }

    static KYRAGameDescription gameDesc;
    memset(&gameDesc, 0, sizeof(gameDesc));

    gameDesc.desc.gameid    = (gameId == 1) ? "kyra2" : "kyra1";
    gameDesc.desc.extra     = "Floppy";
    gameDesc.desc.language  = Common::EN_ANY;
    gameDesc.desc.platform  = Common::kPlatformDOS;
    gameDesc.desc.flags     = ADGF_NO_FLAGS;
    gameDesc.desc.guioptions = "";

    gameDesc.flags.gameID      = (gameId == 1) ? Kyra::GI_KYRA2 : Kyra::GI_KYRA1;
    gameDesc.flags.lang        = Common::EN_ANY;
    gameDesc.flags.platform    = Common::kPlatformDOS;
    gameDesc.flags.isTalkie    = false;
    gameDesc.flags.isDemo      = false;
    gameDesc.flags.useHiRes    = false;
    gameDesc.flags.useDigSound = false;

    printf("KYRA: Creating engine...\n");
    ::Engine *engine = (gameId == 1)
        ? (::Engine *)new Kyra::KyraEngine_HoF(g_system, gameDesc.flags)
        : (::Engine *)new Kyra::KyraEngine_LoK(g_system, gameDesc.flags);

    printf("KYRA: Running game...\n");
    Common::Error err = engine->run();

    printf("KYRA: Game finished with code %d\n", err.getCode());
    delete engine;
    return (err.getCode() == Common::kNoError);
}
#endif

// Full Throttle (SCUMM v7) launcher temporarily disabled.

#ifdef ENABLE_SCI  // PicoMite: the SCUMM engine only (make.py)
// Map a common SCI game id string (from the directory name or manual hint)
// to the engine's SciGameId enum. Only covers SCI0/SCI1/SCI1.1 floppy and
// low-res CD games — SCI32 titles intentionally not listed.
static Sci::SciGameId detectSciGameId(const char *gamePath) {
    const char *slash = strrchr(gamePath, '/');
    const char *name = slash ? slash + 1 : gamePath;

    struct Entry { const char *prefix; Sci::SciGameId id; };
    static const Entry table[] = {
        {"kq1",        Sci::GID_KQ1},
        {"kq4",        Sci::GID_KQ4},
        {"kq5",        Sci::GID_KQ5},
        {"kq6",        Sci::GID_KQ6},
        {"lsl1",       Sci::GID_LSL1},
        {"lsl2",       Sci::GID_LSL2},
        {"lsl3",       Sci::GID_LSL3},
        {"lsl5",       Sci::GID_LSL5},
        {"lsl6",       Sci::GID_LSL6},
        {"sq1",        Sci::GID_SQ1},
        {"sq3",        Sci::GID_SQ3},
        {"sq4",        Sci::GID_SQ4},
        {"sq5",        Sci::GID_SQ5},
        {"pq1",        Sci::GID_PQ1},
        {"pq2",        Sci::GID_PQ2},
        {"pq3",        Sci::GID_PQ3},
        {"qfg1vga",    Sci::GID_QFG1VGA},
        {"qfg1",       Sci::GID_QFG1},
        {"qfg2",       Sci::GID_QFG2},
        {"qfg3",       Sci::GID_QFG3},
        {"iceman",     Sci::GID_ICEMAN},
        {"laurabow2",  Sci::GID_LAURABOW2},
        {"laurabow",   Sci::GID_LAURABOW},
        {"longbow",    Sci::GID_LONGBOW},
        {"ecoquest2",  Sci::GID_ECOQUEST2},
        {"ecoquest",   Sci::GID_ECOQUEST},
        {"freddy",     Sci::GID_FREDDYPHARKAS},
        {"hoyle1",     Sci::GID_HOYLE1},
        {"hoyle2",     Sci::GID_HOYLE2},
        {"hoyle3",     Sci::GID_HOYLE3},
        {"hoyle4",     Sci::GID_HOYLE4},
        {"jones",      Sci::GID_JONES},
        {"pepper",     Sci::GID_PEPPER},
        {"slater",     Sci::GID_SLATER},
        {"castlebrain",Sci::GID_CASTLEBRAIN},
        {"islandbrain",Sci::GID_ISLANDBRAIN},
        {"mothergoose",Sci::GID_MOTHERGOOSE},
        {"camelot",    Sci::GID_CAMELOT},
    };

    for (const auto &e : table) {
        if (strncmp(name, e.prefix, strlen(e.prefix)) == 0) {
            printf("SCI Detection: %s -> game id %s\n", name, e.prefix);
            return e.id;
        }
    }
    printf("SCI Detection: %s not recognized, falling back to fanmade\n", name);
    return Sci::GID_FANMADE;
}

// Sierra SCI game launcher (SCI0 / SCI1 / SCI1.1)
static bool launchSciGame(const char *gamePath, const char *gameIdStr) {
    printf("SCI: Launching game from %s (id=%s)\n", gamePath,
           gameIdStr ? gameIdStr : "(auto)");

    ConfMan.set("path", gamePath);
    ConfMan.setActiveDomain("quest-sci");

    ConfMan.setInt("music_volume", 192);
    ConfMan.setInt("sfx_volume", 192);
    ConfMan.setInt("speech_volume", 192);
    ConfMan.setBool("mute", false);
    ConfMan.setBool("speech_mute", false);
    ConfMan.setBool("sfx_mute", false);
    ConfMan.setBool("music_mute", false);
    ConfMan.setInt("autosave_period", 0);
    ConfMan.setBool("enable_unsupported_game_warning", false);
    ConfMan.set("music_driver", "adlib");
    ConfMan.set("gm_device", "null");
    ConfMan.set("mt32_device", "null");
    ConfMan.setBool("native_mt32", false);
    ConfMan.setBool("enable_gs", false);
    ConfMan.setBool("multi_midi", false);
    ConfMan.setInt("midi_gain", 100);
    ConfMan.setBool("subtitles", true);
    ConfMan.setInt("talkspeed", 60);
    ConfMan.set("language", "en");
    ConfMan.set("gfx_mode", "normal");
    ConfMan.setBool("aspect_ratio", false);

    // SCI-specific prefs
    ConfMan.setBool("copy_protection", false);
    ConfMan.setBool("prefer_digitalsfx", false);
    ConfMan.setBool("originalsaveload", true);      // CMM2: the game's own screens: GUI::SaveLoadChooser is a stub, always slot 0
    ConfMan.setBool("enable_black_lined_video", false);
    ConfMan.setBool("windows_cursors", false);
    ConfMan.setBool("silver_cursors", false);
    ConfMan.setBool("enable_high_resolution_graphics", false);
    ConfMan.setBool("disable_dithering", false);
    ConfMan.setBool("native_fb01", false);
    ConfMan.setBool("use_cdaudio", false);
    ConfMan.setInt("save_slot", -1);
    ConfMan.set("render_mode", "");

    printf("SCI: Loading plugins...\n");
    PluginManager::instance().loadAllPlugins();

    Common::FSNode gameDir(gamePath);
    if (gameDir.exists() && gameDir.isDirectory()) {
        SearchMan.addDirectory(gamePath, gameDir, 0, 4);
        printf("SCI: Added %s to search path\n", gamePath);
    } else {
        printf("SCI: WARNING - %s not accessible\n", gamePath);
        return false;
    }

    Sci::SciGameId gameId = detectSciGameId(gamePath);

    static ADGameDescription gameDesc;
    memset(&gameDesc, 0, sizeof(gameDesc));
    gameDesc.gameid = gameIdStr ? gameIdStr : "sci";
    gameDesc.extra = "";
    gameDesc.language = Common::EN_ANY;
    gameDesc.platform = Common::kPlatformDOS;
    gameDesc.flags = ADGF_NO_FLAGS;
    gameDesc.guioptions = GUIO_NONE;

    ConfMan.set("gameid", gameDesc.gameid);

    printf("SCI: Creating engine instance...\n");
    ::Engine *engine = new Sci::SciEngine(g_system, &gameDesc, gameId);

    printf("SCI: Running game...\n");
    Common::Error err = engine->run();

    printf("SCI: Game finished with code %d (%s)\n",
           err.getCode(), err.getDesc().c_str());
    delete engine;
    return (err.getCode() == Common::kNoError);
}
#endif

// LucasArts SCUMM v1-v6 game launcher. Covers MI1/MI2/DOTT/Sam&Max/
// FOA/LOOM and other classic v1-v6 titles. SCUMM v7/v8 (Full Throttle /
// The Dig / COMI / MI3+MI4) isn't built in this configuration.
struct ScummGameInfo {
    const char *dirName;           // matched against the trailing path component
    const char *gameid;            // ScummVM internal game id
    const char *pattern;           // filename pattern for resource files
    Scumm::FilenameGenMethod genMethod;
    Scumm::ScummGameId id;
    uint8_t version;               // 4 or 5 or 6
    const char *extra;             // "", "CD", "VGA", "Floppy", etc.
    uint32_t midi;                 // MDT_* bitmask, 0 for defaults
    uint32_t features;             // GF_* bitmask (GF_USE_KEY etc.)
};

// GF_* values from engines/scumm/scumm.h; reproduced so this file doesn't
// need scumm.h for the table definition.
#define CABAL_GF_USE_KEY (1u << 4)

// MIDI bitmask shorthands mirror values from engines/scumm/detection_tables.h.
// MDT_* are defined in audio/mididrv.h; reproduced here so we don't need to
// include that header in this translation unit.
#define CABAL_MDT_PCSPK       (1 << 0)
#define CABAL_MDT_CMS         (1 << 1)
#define CABAL_MDT_PCJR        (1 << 2)
#define CABAL_MDT_ADLIB       (1 << 3)
#define CABAL_MDT_MIDI        (1 << 9)
#define CABAL_MDT_PREFER_MT32 (1 << 10)
#define CABAL_MDT_PREFER_GM   (1 << 11)

#define CABAL_MIDI_MI1_VGA  (CABAL_MDT_PCSPK | CABAL_MDT_PCJR | CABAL_MDT_CMS | CABAL_MDT_ADLIB | CABAL_MDT_MIDI | CABAL_MDT_PREFER_MT32)
#define CABAL_MIDI_MI1_EGA  (CABAL_MDT_PCSPK | CABAL_MDT_PCJR | CABAL_MDT_CMS | CABAL_MDT_ADLIB | CABAL_MDT_MIDI | CABAL_MDT_PREFER_MT32)
#define CABAL_MIDI_MI1_CD   (CABAL_MDT_ADLIB)
#define CABAL_MIDI_MI2      (CABAL_MDT_PCSPK | CABAL_MDT_ADLIB | CABAL_MDT_MIDI | CABAL_MDT_PREFER_MT32)
#define CABAL_MIDI_INDY4    (CABAL_MDT_PCSPK | CABAL_MDT_ADLIB | CABAL_MDT_MIDI | CABAL_MDT_PREFER_MT32)
#define CABAL_MIDI_LOOM     (CABAL_MDT_PCSPK | CABAL_MDT_PCJR | CABAL_MDT_CMS | CABAL_MDT_ADLIB | CABAL_MDT_MIDI | CABAL_MDT_PREFER_MT32)
#define CABAL_MIDI_DOTT     (CABAL_MDT_ADLIB | CABAL_MDT_MIDI | CABAL_MDT_PREFER_GM)
#define CABAL_MIDI_SAMNMAX  (CABAL_MDT_ADLIB | CABAL_MDT_MIDI | CABAL_MDT_PREFER_GM)
// Full Throttle is digital-only (iMUSE Digital); it doesn't use the classic
// MIDI synth chain and the detection table marks it MDT_NONE.
#define CABAL_MIDI_FT       (0)

static const ScummGameInfo kScummGames[] = {
    // --- SCUMM v4 (floppy, .LFL + DISK*.LEC) ---
    {"mi1",      "monkey",  "%03d.LFL",      Scumm::kGenRoomNum, Scumm::GID_MONKEY_VGA, 4, "VGA", CABAL_MIDI_MI1_VGA, 0},
    {"mi1ega",   "monkey",  "%03d.LFL",      Scumm::kGenRoomNum, Scumm::GID_MONKEY_EGA, 4, "EGA", CABAL_MIDI_MI1_EGA, 0},
    {"loom",     "loom",    "%03d.LFL",      Scumm::kGenRoomNum, Scumm::GID_LOOM,       4, "VGA", CABAL_MIDI_LOOM,    0},
    // --- SCUMM v5 (monkey.000 / monkey2.000 / atlantis.000) ---
    {"mi1cd",    "monkey",  "monkey.%03d",   Scumm::kGenDiskNum, Scumm::GID_MONKEY,     5, "CD",  CABAL_MIDI_MI1_CD,  0},
    {"monkey",   "monkey",  "monkey.%03d",   Scumm::kGenDiskNum, Scumm::GID_MONKEY,     5, "CD",  CABAL_MIDI_MI1_CD,  0},
    {"mi2",      "monkey2", "monkey2.%03d",  Scumm::kGenDiskNum, Scumm::GID_MONKEY2,    5, "",    CABAL_MIDI_MI2,     0},
    {"monkey2",  "monkey2", "monkey2.%03d",  Scumm::kGenDiskNum, Scumm::GID_MONKEY2,    5, "",    CABAL_MIDI_MI2,     0},
    {"atlantis", "atlantis","atlantis.%03d", Scumm::kGenDiskNum, Scumm::GID_INDY4,      5, "",    CABAL_MIDI_INDY4,   0},
    {"indy4",    "atlantis","atlantis.%03d", Scumm::kGenDiskNum, Scumm::GID_INDY4,      5, "",    CABAL_MIDI_INDY4,   0},
    // --- SCUMM v6 (tentacle.000 / samnmax.000) — need GF_USE_KEY (0x69 XOR) ---
    {"dott",     "tentacle","tentacle.%03d", Scumm::kGenDiskNum, Scumm::GID_TENTACLE,   6, "",    CABAL_MIDI_DOTT,    CABAL_GF_USE_KEY},
    {"tentacle", "tentacle","tentacle.%03d", Scumm::kGenDiskNum, Scumm::GID_TENTACLE,   6, "",    CABAL_MIDI_DOTT,    CABAL_GF_USE_KEY},
    {"samnmax",  "samnmax", "samnmax.%03d",  Scumm::kGenDiskNum, Scumm::GID_SAMNMAX,    6, "",    CABAL_MIDI_SAMNMAX, CABAL_GF_USE_KEY},
    {"sam",      "samnmax", "samnmax.%03d",  Scumm::kGenDiskNum, Scumm::GID_SAMNMAX,    6, "",    CABAL_MIDI_SAMNMAX, CABAL_GF_USE_KEY},
    // --- SCUMM v7 (ft.la%d / dig.la%d) — digital audio + SMUSH video ---
    {"ft",       "ft",      "ft.la%d",       Scumm::kGenDiskNum, Scumm::GID_FT,         7, "",    CABAL_MIDI_FT,      0},
    {"fulltp",   "ft",      "ft.la%d",       Scumm::kGenDiskNum, Scumm::GID_FT,         7, "",    CABAL_MIDI_FT,      0},
};

static const ScummGameInfo *findScummGame(const char *gamePath) {
    const char *slash = strrchr(gamePath, '/');
    const char *name = slash ? slash + 1 : gamePath;
    for (const auto &g : kScummGames) {
        if (strcmp(name, g.dirName) == 0) return &g;
    }
    return nullptr;
}

static bool launchScummGame(const char *gamePath) {
    const ScummGameInfo *info = findScummGame(gamePath);
    if (!info) {
        printf("SCUMM: %s not recognized as a SCUMM v1-v6 game directory\n", gamePath);
        return false;
    }

    printf("SCUMM: Launching %s (v%d, id=%s) from %s\n",
           info->gameid, info->version, info->gameid, gamePath);

    ConfMan.set("path", gamePath);
    ConfMan.setActiveDomain("quest-scumm");

    ConfMan.setInt("music_volume", 192);
    ConfMan.setInt("sfx_volume", 192);
    ConfMan.setInt("speech_volume", 192);
    ConfMan.setBool("mute", false);
    ConfMan.setBool("speech_mute", false);
    ConfMan.setBool("sfx_mute", false);
    ConfMan.setBool("music_mute", false);
    ConfMan.setInt("autosave_period", 0);
    ConfMan.setBool("enable_unsupported_game_warning", false);
    ConfMan.set("music_driver", "adlib");
    ConfMan.set("gm_device", "null");
    ConfMan.set("mt32_device", "null");
    ConfMan.setBool("native_mt32", false);
    ConfMan.setBool("enable_gs", false);
    ConfMan.setBool("multi_midi", false);
    ConfMan.setInt("midi_gain", 100);
    ConfMan.setBool("subtitles", true);
    ConfMan.setInt("talkspeed", 60);
    ConfMan.set("language", "en");
    ConfMan.set("gfx_mode", "normal");
    ConfMan.setBool("aspect_ratio", false);

    // SCUMM-specific knobs
    ConfMan.set("gameid", info->gameid);
    ConfMan.set("original_gui", "false");
    ConfMan.setBool("dump_scripts", false);
    ConfMan.setBool("copy_protection", false);
    ConfMan.setBool("demo_mode", false);
    ConfMan.setBool("nosubtitles", false);
    ConfMan.setBool("confirm_exit", false);
    ConfMan.setBool("object_labels", true);
    ConfMan.setBool("filtering", false);
    ConfMan.setBool("fullscreen", false);
    ConfMan.setInt("boot_param", 0);
    ConfMan.setInt("save_slot", -1);
    ConfMan.setInt("dimuse_tempo", 10);
    ConfMan.setInt("tempo", 0);
    ConfMan.set("render_mode", "");
    ConfMan.set("easter_egg", "");

    printf("SCUMM: Loading plugins...\n");
    PluginManager::instance().loadAllPlugins();

    Common::FSNode gameDir(gamePath);
    if (gameDir.exists() && gameDir.isDirectory()) {
        SearchMan.addDirectory(gamePath, gameDir, 0, 4);
        printf("SCUMM: Added %s to search path\n", gamePath);
    } else {
        printf("SCUMM: WARNING - %s not accessible\n", gamePath);
        return false;
    }

    // The CD/talkie release of Sam & Max ships `samnmax.sm0/.sm1` instead of
    // the floppy-style `samnmax.000/.001` naming. Probe for the primary file
    // with the table-default pattern and fall back to the `sm%d` variant if
    // it is missing.
    const char *pattern = info->pattern;
    {
        char probe[64];
        snprintf(probe, sizeof(probe), "%s/", gamePath);
        int baseLen = (int)strlen(probe);
        snprintf(probe + baseLen, sizeof(probe) - baseLen, pattern, 0);
        if (!cabal_path_exists(probe) && info->id == Scumm::GID_SAMNMAX) {
            snprintf(probe + baseLen, sizeof(probe) - baseLen, "samnmax.sm%d", 0);
            if (cabal_path_exists(probe)) {
                printf("SCUMM: samnmax.000 not found; using samnmax.sm%%d (CD talkie layout)\n");
                pattern = "samnmax.sm%d";
            }
        }
    }

    // Build a minimal DetectorResult matching detection_tables.h entries.
    Scumm::DetectorResult dr;
    memset(&dr, 0, sizeof(dr));
    dr.fp.pattern = pattern;
    dr.fp.genMethod = info->genMethod;
    dr.game.gameid = info->gameid;
    dr.game.variant = 0;
    dr.game.preferredTag = 0;
    dr.game.id = info->id;
    dr.game.version = info->version;
    dr.game.heversion = 0;
    dr.game.midi = info->midi;
    dr.game.features = info->features;
    // Defensive: v6 SCUMM data files are always XOR-encrypted with 0x69,
    // so force GF_USE_KEY on every v6 launch regardless of the table entry.
    if (info->version == 6) {
        dr.game.features |= CABAL_GF_USE_KEY;
    }
    dr.game.platform = Common::kPlatformDOS;
    dr.game.guioptions = "";
    dr.language = Common::EN_ANY;
    dr.extra = info->extra;

    ::Engine *engine = nullptr;
    switch (info->version) {
    case 4:
        printf("SCUMM: Creating v4 engine...\n");
        engine = new Scumm::ScummEngine_v4(g_system, dr);
        break;
    case 5:
        printf("SCUMM: Creating v5 engine...\n");
        engine = new Scumm::ScummEngine_v5(g_system, dr);
        break;
    case 6:
        printf("SCUMM: Creating v6 engine...\n");
        engine = new Scumm::ScummEngine_v6(g_system, dr);
        break;
    case 7:
        printf("SCUMM: Creating v7 engine...\n");
        engine = new Scumm::ScummEngine_v7(g_system, dr);
        break;
    default:
        printf("SCUMM: unsupported version %d\n", info->version);
        return false;
    }

    printf("SCUMM: Running game...\n");
    Common::Error err = engine->run();
    printf("SCUMM: Game finished with code %d (%s)\n",
           err.getCode(), err.getDesc().c_str());
    delete engine;
    return (err.getCode() == Common::kNoError);
}

// Minimum ConfMan defaults every engine expects to exist. The
// per-launcher functions below override some of these, but the
// EVENT_QUIT handler in backends/events/default-events.cpp reads
// keys (like confirm_exit) whose absence triggers an I_Error panic
// — so any key the engine loop itself consumes has to be set here
// regardless of which launcher runs next.
static void registerConfManDefaults() {
    ConfMan.registerDefault("confirm_exit",                    false);
    ConfMan.registerDefault("autosave_period",                 0);
    ConfMan.registerDefault("enable_unsupported_game_warning", false);
    ConfMan.registerDefault("mute",                            false);
    ConfMan.registerDefault("music_mute",                      false);
    ConfMan.registerDefault("sfx_mute",                        false);
    ConfMan.registerDefault("speech_mute",                     false);
    ConfMan.registerDefault("music_volume",                    192);
    ConfMan.registerDefault("sfx_volume",                      192);
    ConfMan.registerDefault("speech_volume",                   192);
    ConfMan.registerDefault("subtitles",                       true);
    ConfMan.registerDefault("talkspeed",                       60);
    ConfMan.registerDefault("midi_gain",                       100);
    ConfMan.registerDefault("native_mt32",                     false);
    ConfMan.registerDefault("enable_gs",                       false);
    ConfMan.registerDefault("multi_midi",                      false);
    ConfMan.registerDefault("aspect_ratio",                    false);
    ConfMan.registerDefault("fullscreen",                      false);
    ConfMan.registerDefault("filtering",                       false);
    ConfMan.registerDefault("copy_protection",                 false);
    ConfMan.registerDefault("music_driver",                    "adlib");
    ConfMan.registerDefault("gm_device",                       "null");
    ConfMan.registerDefault("mt32_device",                     "null");
    ConfMan.registerDefault("language",                        "en");
    ConfMan.registerDefault("gfx_mode",                        "normal");
    ConfMan.registerDefault("render_mode",                     "");
}

// Dispatch a QuestGame to the engine launcher it belongs to. Returns
// true if the engine reported success; false means the engine refused
// to start (bad files, unsupported variant, etc.).
static bool dispatchGame(const QuestGame &g) {
    switch (g.engine) {
    case QuestEngine::Scumm:
        return launchScummGame(g.dirPath);
#ifdef ENABLE_SCI
    case QuestEngine::Sci:
        // SCI detector ids mirror the launchSciGame fallback probe; pass
        // NULL so detectSciGameId() runs against the directory name.
        return launchSciGame(g.dirPath, nullptr);
#endif
#ifdef ENABLE_KYRA
    case QuestEngine::Kyra:
        return launchKyrandiaGame(g.dirPath, (int)g.engineSubtype);
#endif
#ifdef ENABLE_GOB
    case QuestEngine::Gob:
        if (g.engineSubtype == 0) return launchGOBGame(g.dirPath);
        return launchGOBGameWithVersion(g.dirPath, (int)g.engineSubtype);
#endif
#ifdef ENABLE_AGI
    case QuestEngine::Agi:
        return launchAGIGame(g.dirPath);
#endif
    default:                         // PicoMite: an engine left out of this build
        printf("This build has only the SCUMM engine\n");
        return false;
    }
    return false;
}

// Last-selected cursor index across selector re-entries. No reboot
// happens between selector iterations (in-process teardown), so this
// stays a plain global — scratch-register persistence is not needed.
static int g_lastSelectorIndex = 0;

// External flag from rp2350-minimal.cpp — cleared on session boundaries
// so a stale CAD press (modifier still held in the state machine)
// doesn't fire immediately after reinit.
extern "C" void frank_quest_cad_clear(void);

// PS/2 input ring buffer flush.
extern "C" void ps2kbd_flush(void);

// ScummVM singleton teardown. Mirrors the destroy sequence at the end
// of scummvm_main() in src/base/main.cpp so we leave the engine state
// in the same shape a fresh boot would.
#include "base/plugins.h"
#include "common/debug-channels.h"
#include "common/memorypool.h"
#include "audio/musicplugin.h"
#include "audio/audiodev/pcspk.h"
#include "graphics/cursorman.h"
#include "graphics/yuv_to_rgb.h"
#include "backends/fs/rp2350/rp2350-fs-factory.h"

// Common::String's refcount memory pool is a file-static global in
// src/common/str.cpp that's heap-allocated on first use and never
// freed (the file's own comment flags this: "FIXME: This is never
// freed right now"). The pool lives in the PSRAM mspace we wipe on
// each return-to-selector, so the dangling pointer segfaults inside
// allocChunk() the next time a String is constructed. Reach into the
// global and reset it before psram_reset() nukes its backing memory.
namespace Common {
extern MemoryPool *g_refCountPool;
}

static void teardownSingletons() {
    if (g_system) {
        g_system->getEventManager()->resetRTL();
    }

    PluginManager::instance().unloadAllPlugins();
    PluginManager::destroy();
    Common::ConfigManager::destroy();
    Common::DebugManager::destroy();
    Common::SearchManager::destroy();
    MusicManager::destroy();
    EngineManager::destroy();

    // Reset graphics/audio helper singletons that lazy-allocate into
    // PSRAM. Skipping any of these leaves the template's static
    // _singleton pointer dangling after psram_reset() and the next
    // game's first instance() silently returns freed memory.
    // FontManager and CoroutineScheduler have no DECLARE_SINGLETON in
    // this build's link closure, so they don't need teardown here.
    Graphics::CursorManager::destroy();
    Graphics::YUVToRGBManager::destroy();
    Audio::PCSpeakerFactoryManager::destroy();
#ifdef USE_TRANSLATION
    Common::TranslationManager::destroy();
#endif
}

// Main game loop - called from main.c
extern "C" int cabal_main(void) {
    printf("FRANK Quest: Starting with OSystem backend...\n");

    // Seed ConfMan so the engine's default event loop finds every
    // key it reads unconditionally (confirm_exit etc.). Must be
    // re-registered on every selector iteration because ConfMan is
    // torn down along with the other singletons below.
    registerConfManDefaults();

    // Splash screen at cold boot. Auto-advances after 10 s; any key
    // dismisses it immediately. cabal_main() only runs once per
    // power-on — the return-to-selector path loops inside the while
    // below and doesn't re-enter here — so no guard needed.
    frank_quest_show_welcome(10000);

    // Outer loop: selector → game → teardown → back to selector.
    // HDMI stays locked across the loop because the framebuffer lives
    // in a persistent PSRAM slot (psram_get_framebuffer) that isn't
    // touched by psram_reset(), and we never reinitialize the HDMI
    // hardware — only OSystem + singletons + mspace get rebuilt.
    while (true) {
        // Scan /quest on SD card for games. Lives in static storage
        // so it doesn't burn mspace — scan happens before the first
        // engine alloc and wouldn't survive the reset anyway.
        static QuestGame games[64];
        int count = frank_quest_scan_games(games, 64);
        printf("FRANK Quest: %d game(s) detected under /quest\n", count);

        int selected = frank_quest_run_selector(games, count,
                                                g_lastSelectorIndex);

        if (selected < 0 || selected >= count) {
            printf("FRANK Quest: no game selected; idling.\n");
            while (true) g_system->delayMillis(100);
        }

        g_lastSelectorIndex = selected;

        const QuestGame &g = games[selected];
        printf("FRANK Quest: launching %s (engine=%d path=%s)\n",
               g.displayName, (int)g.engine, g.dirPath);

        // Show a "Loading <game>..." window before the engine grabs
        // the frame. Plugin load + resource parsing takes a few
        // seconds; without this the HDMI output just sits on the
        // selector frame and the device looks frozen.
        frank_quest_show_loading(g);

        if (dispatchGame(g)) {
            printf("FRANK Quest: game completed cleanly.\n");
        } else {
            printf("FRANK Quest: game dispatch failed.\n");
        }

        // Engine has returned — either naturally (ChainGamesMan empty,
        // game finished) or via Ctrl+Alt+Del → EVENT_QUIT. Tear down
        // every bit of state it touched: singletons first, then the
        // OSystem, then the entire PSRAM mspace. HDMI scanout stays
        // pointed at the persistent framebuffer slot throughout.
        printf("FRANK Quest: tearing down engine state...\n");
        teardownSingletons();

        // OSystem's destructor is protected, but we own the concrete
        // OSystem_RP2350 subtype and its destructor is public. Cast
        // down before deleting.
        delete static_cast<OSystem_RP2350 *>(g_system);
        g_system = nullptr;

        // Null stale global pointers that reference mspace memory
        // ABOUT to be freed. The String refcount pool is allocated
        // lazily by Common::String and never released; the global
        // pointer in str.cpp would dangle after psram_reset() and
        // crash on the next String construction. A fresh pool is
        // allocated on demand the next time a String is built.
        Common::g_refCountPool = nullptr;

        // Same fix for every Common::Singleton<T> we touch. The
        // template's static _singleton pointer lives in BSS and
        // caches a heap-allocated instance (makeInstance() does
        // `new T()` which routes to PSRAM). Leave it pointing into
        // the about-to-be-wiped mspace and the next instance() call
        // silently reads garbage through the stale vtable — that's
        // why FSNode::exists() returned false on the second engine
        // launch without crashing: the factory-object memory had
        // been partially reused and makeFileNodePath() dispatched
        // into nonsense. destroy() runs the destructor and nulls
        // the static for us.
        RP2350FilesystemFactory::destroy();

        // Wipe the mspace — every engine allocation is freed in one
        // step. The framebuffer sits outside the mspace so HDMI keeps
        // displaying the last rendered frame during this wipe.
        psram_reset();

        // Close any SD file/dir handles left dangling by the engine
        // (graceful exit paths close them, but CAD is a cold cut).
        cabal_fs_session_reset();

        // Clear the CAD modifier state machine so a residual "Ctrl
        // still held" from the trigger event doesn't immediately
        // refire the next time a modifier is pressed.
        frank_quest_cad_clear();

        // Drop any in-flight PS/2 keyboard events — the ring buffer
        // itself lives in SRAM (survives psram_reset) but may hold
        // events produced during the engine's own teardown that the
        // selector shouldn't see (e.g. the Del keyup from CAD).
        ps2kbd_flush();

        // Rebuild OSystem for the next iteration of the selector.
        // cabal_init() is idempotent — cabal_fs stays mounted, the
        // HDMI buffer pointer is unchanged — so we just re-create
        // the ScummVM backend.
        cabal_init();

        // ConfMan was just destroyed; seed it again with the keys
        // the engine's EVENT_QUIT handler expects to find.
        registerConfManDefaults();
    }
}
