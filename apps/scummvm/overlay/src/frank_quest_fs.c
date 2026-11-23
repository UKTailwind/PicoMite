/*
 * FRANK Quest
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Filesystem implementation using FatFS.
 *
 * CMM2: the loader's file services (the firmware's FatFs, on the SD card or the USB drive) in place of FatFs here:
 * a handle is the loader's number, and a path keeps its form (/quest/... is on the current drive, A: normally).
 * whether a path is there is found by opening it as a file or a directory; the free space is not known (a large number is given).
 */

#include "frank_quest_fs.h"
#include "cmm2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// External PSRAM allocation
extern void *psram_malloc(size_t size);
extern void psram_free(void *ptr);

// External audio processing (to prevent audio freeze during file I/O)
#ifdef USE_I2S_AUDIO
extern void cabal_audio_process_frame(void);
#endif

//============================================================================
// Internal State
//============================================================================

static bool g_initialized = false;

// File handle wrapper (CMM2: the loader's handle)
struct CabalFile {
    int fil;
    bool valid;
};

// Directory handle wrapper (CMM2: the loader's handle)
struct CabalDir {
    int dir;
    bool valid;
};

// CMM2: FatFs's result codes, which the loader returns negated
#define FR_OK       0
#define FR_NO_FILE  4
#define FR_NO_PATH  5
#define FR_EXIST    8

// PicoMite: the loader's services may not be called from an interrupt, and the sound fill, which mixes ScummVM's
// sound, is one. The firmware's FatFs is not reentrant: a read there while the program itself reads could damage the
// card. So a call made in an interrupt is refused here, counted, and reported by the next call made outside one.
// (The speech is read into memory before it plays, audio/decoders/voc.cpp, so that none should come.)
static volatile uint32_t g_irq_refused, g_irq_reported;

static int fs_in_irq(void) {
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    if (ipsr & 0x1ff) {
        g_irq_refused++;
        return 1;
    }
    if (g_irq_refused != g_irq_reported) {
        g_irq_reported = g_irq_refused;
        printf("Cabal FS: %lu file calls from the sound interrupt refused\n", (unsigned long)g_irq_reported);
    }
    return 0;
}

// Pool of file handles (avoid malloc for small allocations)
#define MAX_OPEN_FILES 8
static struct CabalFile g_files[MAX_OPEN_FILES];

#define MAX_OPEN_DIRS 4
static struct CabalDir g_dirs[MAX_OPEN_DIRS];

//============================================================================
// Initialization
//============================================================================

CabalFsResult cabal_fs_init(void) {
    if (g_initialized) {
        return CABAL_FS_OK;
    }

    printf("Cabal FS: Initializing SD card...\n");

    // CMM2: the card is the firmware's; its root is read to see that it is there
    int d = cmm2->dir_open("/");
    if (d < 0) {
        printf("Cabal FS: Failed to read the card (error=%d)\n", -d);
        return CABAL_FS_NOT_READY;
    }
    cmm2->dir_close(d);

    // Initialize file handle pool
    memset(g_files, 0, sizeof(g_files));
    memset(g_dirs, 0, sizeof(g_dirs));

    g_initialized = true;

    printf("Cabal FS: Mounted successfully\n");

    return CABAL_FS_OK;
}

bool cabal_fs_ready(void) {
    return g_initialized;
}

uint32_t cabal_fs_free_space(void) {
    if (!g_initialized) return 0;

    // CMM2: not known through the loader; enough for any save
    return 0x40000000u;
}

void cabal_fs_session_reset(void) {
    if (!g_initialized) return;
    // Close any file handles the previous session forgot to release,
    // then zero the pools. SD card stays mounted (no f_mount() bounce)
    // because the filesystem state in g_fatfs is independent of the
    // handle pools.
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (g_files[i].valid) {
            cmm2->file_close(g_files[i].fil);
        }
    }
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (g_dirs[i].valid) {
            cmm2->dir_close(g_dirs[i].dir);
        }
    }
    memset(g_files, 0, sizeof(g_files));
    memset(g_dirs, 0, sizeof(g_dirs));
}

//============================================================================
// File Operations
//============================================================================

static struct CabalFile *alloc_file_handle(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!g_files[i].valid) {
            memset(&g_files[i], 0, sizeof(struct CabalFile));
            return &g_files[i];
        }
    }
    return NULL;
}

// Helper to build full path (CMM2: "/x" as it is, on the current drive; "0:/x" is "/x"; "x" is "/x")
static void build_fullpath(char *fullpath, size_t size, const char *path) {
    if (path[0] == '0' && path[1] == ':') path += 2;
    if (path[0] == '/') {
        strncpy(fullpath, path, size - 1);
        fullpath[size - 1] = '\0';
    } else {
        snprintf(fullpath, size, "/%s", path);
    }
}

CabalFile *cabal_file_open(const char *path) {
    if (fs_in_irq()) return NULL;   // PicoMite: (above)
    if (!g_initialized || !path) return NULL;

    struct CabalFile *file = alloc_file_handle();
    if (!file) {
        printf("Cabal FS: No free file handles\n");
        return NULL;
    }

    char fullpath[512];
    build_fullpath(fullpath, sizeof(fullpath), path);

    int res = cmm2->file_open(fullpath, CMM2_O_READ);
    if (res < 0) {
        res = -res;
        // Don't print error for common "file not found" case
        if (res != FR_NO_FILE && res != FR_NO_PATH) {
            printf("Cabal FS: Failed to open '%s' (error=%d)\n", fullpath, res);
        }
        return NULL;
    }

    file->fil = res;
    file->valid = true;
    return file;
}

CabalFile *cabal_file_open_write(const char *path) {
    if (fs_in_irq()) return NULL;   // PicoMite: (above)
    if (!g_initialized || !path) return NULL;

    struct CabalFile *file = alloc_file_handle();
    if (!file) {
        printf("Cabal FS: No free file handles\n");
        return NULL;
    }

    char fullpath[512];
    build_fullpath(fullpath, sizeof(fullpath), path);

    // Create or truncate file for writing
    int res = cmm2->file_open(fullpath, CMM2_O_WRITE | CMM2_O_CREATE);
    if (res < 0) {
        printf("Cabal FS: Failed to create '%s' (error=%d)\n", fullpath, -res);
        return NULL;
    }

    file->fil = res;
    file->valid = true;
    return file;
}

void cabal_file_close(CabalFile *file) {
    if (fs_in_irq()) return;   // PicoMite: (above)
    if (!file || !file->valid) return;

    cmm2->file_close(file->fil);
    file->valid = false;
}

int32_t cabal_file_read(CabalFile *file, void *buffer, uint32_t size) {
    if (fs_in_irq()) return -1;   // PicoMite: (above)
    if (!file || !file->valid || !buffer) return -1;

#ifdef USE_I2S_AUDIO
    // For large reads, chunk the operation and process audio between chunks
    // to prevent audio underruns during file I/O
    // Use 4KB chunks for more frequent audio processing
    if (size > 4096) {
        uint8_t *dst = (uint8_t *)buffer;
        uint32_t total_read = 0;
        while (size > 0) {
            uint32_t chunk = (size > 4096) ? 4096 : size;
            int32_t bytes_read = cmm2->file_read(file->fil, dst, chunk);
            if (bytes_read < 0) {
                return (total_read > 0) ? (int32_t)total_read : -1;
            }
            total_read += bytes_read;
            dst += bytes_read;
            size -= bytes_read;
            if ((uint32_t)bytes_read < chunk) break;  // EOF
            cabal_audio_process_frame();
        }
        return (int32_t)total_read;
    }
#endif

    int32_t bytes_read = cmm2->file_read(file->fil, buffer, size);
    if (bytes_read < 0) {
        return -1;
    }

    return bytes_read;
}

int32_t cabal_file_write(CabalFile *file, const void *buffer, uint32_t size) {
    if (fs_in_irq()) return -1;   // PicoMite: (above)
    if (!file || !file->valid || !buffer) return -1;

#ifdef USE_I2S_AUDIO
    // For large writes, chunk the operation and process audio between chunks
    if (size > 4096) {
        const uint8_t *src = (const uint8_t *)buffer;
        uint32_t total_written = 0;
        while (size > 0) {
            uint32_t chunk = (size > 4096) ? 4096 : size;
            int32_t bytes_written = cmm2->file_write(file->fil, src, chunk);
            if (bytes_written < 0) {
                return (total_written > 0) ? (int32_t)total_written : -1;
            }
            total_written += bytes_written;
            src += bytes_written;
            size -= bytes_written;
            if ((uint32_t)bytes_written < chunk) break;  // Disk full?
            cabal_audio_process_frame();
        }
        return (int32_t)total_written;
    }
#endif

    int32_t bytes_written = cmm2->file_write(file->fil, buffer, size);
    if (bytes_written < 0) {
        return -1;
    }

    return bytes_written;
}

CabalFsResult cabal_file_flush(CabalFile *file) {
    if (fs_in_irq()) return CABAL_FS_ERROR;   // PicoMite: (above)
    if (!file || !file->valid) return CABAL_FS_INVALID_PARAM;

    return cmm2->file_sync(file->fil) == 0 ? CABAL_FS_OK : CABAL_FS_ERROR;
}

void *cabal_file_read_all(const char *path, uint32_t *size_out) {
    if (fs_in_irq()) return NULL;   // PicoMite: (above)
    if (size_out) *size_out = 0;

    CabalFile *file = cabal_file_open(path);
    if (!file) return NULL;

    uint32_t size = cabal_file_size(file);
    if (size == 0) {
        cabal_file_close(file);
        return NULL;
    }

    // Allocate from PSRAM
    void *buffer = psram_malloc(size);
    if (!buffer) {
        printf("Cabal FS: Failed to allocate %lu bytes for '%s'\n",
               (unsigned long)size, path);
        cabal_file_close(file);
        return NULL;
    }

    int32_t bytes_read = cabal_file_read(file, buffer, size);
    cabal_file_close(file);

    if (bytes_read < 0 || (uint32_t)bytes_read != size) {
        printf("Cabal FS: Read error for '%s'\n", path);
        // Can't free PSRAM with bump allocator, but mark as error
        return NULL;
    }

    if (size_out) *size_out = size;
    return buffer;
}

uint32_t cabal_file_size(CabalFile *file) {
    if (fs_in_irq()) return 0;   // PicoMite: (above)
    if (!file || !file->valid) return 0;
    int32_t n = cmm2->file_size(file->fil);
    return n < 0 ? 0 : (uint32_t)n;
}

uint32_t cabal_file_tell(CabalFile *file) {
    if (fs_in_irq()) return 0;   // PicoMite: (above)
    if (!file || !file->valid) return 0;
    int32_t n = cmm2->file_tell(file->fil);
    return n < 0 ? 0 : (uint32_t)n;
}

CabalFsResult cabal_file_seek(CabalFile *file, uint32_t offset) {
    if (fs_in_irq()) return CABAL_FS_ERROR;   // PicoMite: (above)
    if (!file || !file->valid) return CABAL_FS_INVALID_PARAM;

    return cmm2->file_seek(file->fil, offset) == 0 ? CABAL_FS_OK : CABAL_FS_ERROR;
}

bool cabal_file_eof(CabalFile *file) {
    if (fs_in_irq()) return true;   // PicoMite: (above)
    if (!file || !file->valid) return true;
    return cmm2->file_tell(file->fil) >= cmm2->file_size(file->fil);
}

//============================================================================
// Directory Operations
//============================================================================

static struct CabalDir *alloc_dir_handle(void) {
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (!g_dirs[i].valid) {
            memset(&g_dirs[i], 0, sizeof(struct CabalDir));
            return &g_dirs[i];
        }
    }
    return NULL;
}

CabalDir *cabal_dir_open(const char *path) {
    if (fs_in_irq()) return NULL;   // PicoMite: (above)
    if (!g_initialized || !path) return NULL;

    struct CabalDir *dir = alloc_dir_handle();
    if (!dir) {
        printf("Cabal FS: No free directory handles\n");
        return NULL;
    }

    // Build full path
    char fullpath[512];
    build_fullpath(fullpath, sizeof(fullpath), path);

    int res = cmm2->dir_open(fullpath);
    if (res < 0) {
        return NULL;
    }

    dir->dir = res;
    dir->valid = true;
    return dir;
}

void cabal_dir_close(CabalDir *dir) {
    if (fs_in_irq()) return;   // PicoMite: (above)
    if (!dir || !dir->valid) return;

    cmm2->dir_close(dir->dir);
    dir->valid = false;
}

bool cabal_dir_read(CabalDir *dir, CabalDirEntry *entry) {
    if (fs_in_irq()) return false;   // PicoMite: (above)
    if (!dir || !dir->valid || !entry) return false;

    static cmm2_dirent fno;                 // (CMM2: the loader's entry; static, as it is 264 bytes)
    if (cmm2->dir_read(dir->dir, &fno) != 1 || fno.name[0] == 0) {
        return false;
    }

    strncpy(entry->name, fno.name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->size = fno.size;
    entry->isDirectory = fno.is_dir != 0;

    return true;
}

bool cabal_path_exists(const char *path) {
    if (fs_in_irq()) return false;   // PicoMite: (above)
    if (!g_initialized || !path) return false;

    char fullpath[512];
    build_fullpath(fullpath, sizeof(fullpath), path);

    // CMM2: there as a file, or as a directory
    int h = cmm2->file_open(fullpath, CMM2_O_READ);
    if (h >= 0) {
        cmm2->file_close(h);
        return true;
    }
    h = cmm2->dir_open(fullpath);
    if (h >= 0) {
        cmm2->dir_close(h);
        return true;
    }
    return false;
}

bool cabal_path_is_dir(const char *path) {
    if (fs_in_irq()) return false;   // PicoMite: (above)
    if (!g_initialized || !path) return false;

    char fullpath[512];
    build_fullpath(fullpath, sizeof(fullpath), path);

    // CMM2: a directory if it opens as one
    int h = cmm2->dir_open(fullpath);
    if (h < 0) return false;
    cmm2->dir_close(h);
    return true;
}

CabalFsResult cabal_mkdir(const char *path) {
    if (fs_in_irq()) return CABAL_FS_ERROR;   // PicoMite: (above)
    if (!g_initialized || !path) return CABAL_FS_INVALID_PARAM;

    char fullpath[512];
    build_fullpath(fullpath, sizeof(fullpath), path);

    int res = -cmm2->dir_make(fullpath);
    if (res == FR_OK || res == FR_EXIST) {
        return CABAL_FS_OK;
    }
    return CABAL_FS_ERROR;
}

CabalFsResult cabal_remove(const char *path) {
    if (fs_in_irq()) return CABAL_FS_ERROR;   // PicoMite: (above)
    if (!g_initialized || !path) return CABAL_FS_INVALID_PARAM;

    char fullpath[512];
    build_fullpath(fullpath, sizeof(fullpath), path);

    return cmm2->file_remove(fullpath) == 0 ? CABAL_FS_OK : CABAL_FS_ERROR;
}

//============================================================================
// Memory Management
//============================================================================

void cabal_fs_free(void *ptr) {
    // Note: PSRAM uses a bump allocator, can't actually free
    // This is just for API completeness
    (void)ptr;
}
