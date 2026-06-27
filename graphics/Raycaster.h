#ifndef RAYCASTER_H
#define RAYCASTER_H
/***********************************************************************************************************************
PicoMite MMBasic

Raycaster.h

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed
    on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed
    by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software
    without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/

/*
 * DDA Raycaster for PicoMite MMBasic (RP2350 only)
 *
 * Renders a Wolfenstein 3D-style first-person view into a 4bpp (RGB121)
 * framebuffer. The user is responsible for copying the framebuffer to the
 * physical display (e.g. via BLIT or FRAMEBUFFER commands).
 *
 * Command Reference:
 *
 *   RAY MAP w, h, map%()
 *       Define the world grid. map%() is a 1D integer array with w*h entries.
 *       Each entry is a wall definition index: 0 = empty, 1-31 = wall.
 *
 *   RAY MAP w, h, map$()
 *       Define the world grid using a string array. map$() is a 1D string
 *       array with at least h elements, each string at least w characters.
 *       Characters: '0'-'9' map to values 0-9, 'A'-'Z' / 'a'-'z' to 10-35.
 *       This uses 1 byte per cell instead of 8, greatly reducing memory.
 *
 *   RAY DEFINE type, fg, bg, pattern [, door]
 *       Configure the appearance of a wall type (1-31).
 *       type: integer 1-31, or a single-character string ('1'-'9', 'A'-'Z').
 *       fg, bg: foreground and background colours (RGB121 palette 0-15).
 *       pattern: fill pattern index (0-31, same as Turtle patterns).
 *       door: 0 (default) = solid wall, 1 = this type is a sliding door.
 *       Y-side walls are automatically dimmed (green channel decremented)
 *       for depth cueing. Defaults: types 1-15 green, 16-31 brown + door.
 *
 *   RAY CAMERA x!, y!, angle! [, fov!]
 *       Place the viewer. x!, y! are float coordinates in map space (0-based).
 *       angle! is heading in degrees (0 = +X axis, 90 = +Y axis).
 *       fov! is field of view in degrees (default 60).
 *
 *   RAY COLOUR floor_fg, ceiling_fg [, floor_bg, ceiling_bg, floor_pat, ceiling_pat]
 *       Set floor and ceiling appearance.
 *       2 args: solid floor and ceiling using 4-bit RGB121 palette indices (0-15).
 *       6 args: textured floor/ceiling.  fg/bg are foreground/background colours,
 *               pat selects a fill pattern index (0-31, same as Turtle patterns).
 *               Pattern bits 1 = fg colour, bits 0 = bg colour.
 *
 *   RAY MOVE speed! [, strafe!]
 *       Move the camera with built-in collision detection.
 *       speed > 0 = forward, < 0 = backward.  strafe > 0 = right, < 0 = left.
 *       Uses a 0.25-unit bounding box.  If the full move is blocked, the engine
 *       attempts wall sliding (X-only, then Y-only).  If completely blocked the
 *       camera stays put.  Updates the internal camera position.
 *
 *   RAY TURN degrees!
 *       Adjust camera heading.  Positive = clockwise/right, negative = left.
 *       Automatically normalises to 0-360.
 *
 *   RAY RENDER
 *       Render the scene into the current WriteBuf framebuffer.
 *       A framebuffer must be set up first (e.g. via FRAMEBUFFER CREATE).
 *       The framebuffer is treated as 4bpp: two pixels per byte,
 *       even pixel in low nibble, odd pixel in high nibble.
 *
 *   RAY CLOSE
 *       Free all raycaster state.
 *
 *   RAY CELL x, y, value
 *       Write a map cell at integer coordinates (x, y) to value.
 *       value: integer 0-31, or a single-character string ('0'-'9', 'A'-'Z').
 *       0 = empty, 1-31 = wall type.  Enables doors, switches, etc.
 *
 *   RAY CAST angle!
 *       Cast a single ray from the camera position at the given absolute
 *       angle (degrees).  Stores the result for retrieval via RAY(CASTDIST),
 *       RAY(CASTWALL), RAY(CASTSIDE), RAY(CASTX), RAY(CASTY).
 *       Useful for "use" buttons, shooting, proximity checks.
 *
 *   RAY SPRITE id, spritenum, x!, y!
 *       Place or update a billboard sprite.  id is 0 to 31 (raycaster slot).
 *       spritenum (1-64) is the SPRITE buffer number loaded via SPRITE LOAD.
 *       x!, y! are world coordinates.  The sprite's full-colour 4bpp image
 *       is sampled from spritebuff[spritenum] and scaled to world size.
 *       The global sprite_transparent colour is treated as transparent.
 *       Sprites are depth-sorted and clipped against the wall z-buffer
 *       during RAY RENDER.
 *
 *   RAY SPRITE REMOVE id
 *       Remove the sprite with the given id.
 *
 *   RAY SPRITE CLEAR
 *       Remove all sprites.
 *
 *   RAY DOOR x, y, offset!
 *       Set a sliding door at map cell (x, y) with the given offset.
 *       offset 0.0 = fully closed, 1.0 = fully open.
 *       The map cell must contain a wall type with door=1 (see RAY DEFINE).
 *       During rendering, rays pass through the open portion of the door.
 *       Collision blocks movement until offset >= 1.0.
 *       Up to 8 doors may be active simultaneously.
 *
 *   RAY DOOR CLOSE x, y
 *       Remove the door slot at (x, y). The cell reverts to a normal solid wall.
 *
 *   RAY DOOR CLEAR
 *       Remove all active door slots.
 *
 *   RAY MINIMAP x, y, size
 *       Draw a top-down minimap overlay at screen position (x, y).
 *       Shows the entire map scaled so the longest axis fits 'size' pixels.
 *       Walls use their definition's foreground colour, empty = BLACK,
 *       player = WHITE with direction indicator.
 *       Doors: closed = definition colour, partial = YELLOW, open = BLACK.
 *       Call after RAY RENDER, before FRAMEBUFFER COPY.
 *
 * Function Reference:
 *
 *   RAY(MAPW)       - Returns map width
 *   RAY(MAPH)       - Returns map height
 *   RAY(CAMX)       - Returns camera X position
 *   RAY(CAMY)       - Returns camera Y position
 *   RAY(CAMA)       - Returns camera angle
 *   RAY(DIST col)   - Returns perpendicular distance at screen column col
 *   RAY(WALL col)   - Returns wall type hit at screen column col
 *   RAY(CELL x, y)  - Returns map cell value at (x, y)
 *   RAY(CASTDIST)   - Distance from last RAY CAST
 *   RAY(CASTWALL)   - Wall type from last RAY CAST
 *   RAY(CASTSIDE)   - Side hit from last RAY CAST (0=X, 1=Y)
 *   RAY(CASTX)      - Map X cell hit from last RAY CAST
 *   RAY(CASTY)      - Map Y cell hit from last RAY CAST
 *   RAY(DOOR x, y)  - Door offset at (x, y), or -1.0 if not an active door
 *   RAY(DEFINE type, property)
 *                   - Query wall definition. property: 0=fg, 1=bg, 2=pattern, 3=door
 *   RAY(SPRITES)    - Count of active sprites
 *   RAY(SPRITEX id) - World X of sprite id
 *   RAY(SPRITEY id) - World Y of sprite id
 */

/* Maximum map size */
#define RAY_MAX_MAP_W 256
#define RAY_MAX_MAP_H 256

/* Maximum billboard sprites */
#define RAY_MAX_SPRITES 32

/* Maximum active sliding doors */
#define RAY_MAX_DOORS 8

/* Number of wall definitions (0 = empty, 1-31 usable) */
#define RAY_MAX_WALLDEFS 32

/* Sprite — a billboard object placed in the world */
typedef struct
{
    float x, y;     /* world position */
    int spritenum;  /* SPRITE buffer number (1-MAXBLITBUF), references spritebuff[] */
    uint8_t active; /* 0 = unused, 1 = active */
} RaySprite;

/* Door — a sliding door at a map cell */
typedef struct
{
    int map_x, map_y; /* cell position */
    float offset;     /* 0.0 = closed, 1.0 = fully open */
    uint8_t active;   /* 0 = unused, 1 = active */
} RayDoor;

/* Wall definition — per-type appearance and behaviour */
typedef struct
{
    uint8_t fg;      /* foreground colour, RGB121 palette 0-15 */
    uint8_t bg;      /* background colour, RGB121 palette 0-15 */
    uint8_t pattern; /* fill pattern index, 0-31 (Turtle patterns) */
    uint8_t is_door; /* 0 = solid wall, 1 = sliding door type */
} WallDef;

/* Raycaster state — heap-allocated */
typedef struct
{
    /* Map */
    int map_w, map_h;
    uint8_t *map; /* map_w * map_h bytes, each is wall type 0-31 */

    /* Camera */
    float cam_x, cam_y; /* position in map space */
    float cam_angle;    /* heading in degrees */
    float cam_fov;      /* field of view in degrees */

    /* Colours and floor/ceiling textures */
    uint8_t floor_fg;  /* floor foreground colour, RGB121 palette 0-15 */
    uint8_t floor_bg;  /* floor background colour, RGB121 palette 0-15 */
    uint8_t ceil_fg;   /* ceiling foreground colour, RGB121 palette 0-15 */
    uint8_t ceil_bg;   /* ceiling background colour, RGB121 palette 0-15 */
    uint8_t floor_pat; /* floor fill pattern index 0-31 (0=Solid) */
    uint8_t ceil_pat;  /* ceiling fill pattern index 0-31 (0=Solid) */

    /* Per-column results (allocated HRes entries) */
    float *col_dist;   /* perpendicular distance per column */
    uint8_t *col_wall; /* wall type hit per column */
    int num_cols;      /* HRes at time of last render */

    /* RAY CAST result (last single-ray cast) */
    float cast_dist; /* perpendicular distance to wall */
    int cast_wall;   /* wall type hit (0 = none) */
    int cast_side;   /* 0 = X-side, 1 = Y-side */
    int cast_mapx;   /* map X cell of the hit */
    int cast_mapy;   /* map Y cell of the hit */

    /* Sprites */
    RaySprite sprites[RAY_MAX_SPRITES];

    /* Doors */
    RayDoor doors[RAY_MAX_DOORS];

    /* Wall definitions */
    WallDef walldefs[RAY_MAX_WALLDEFS];
} RayState;

void cmd_ray(void);
void fun_ray(void);
void ray_close(void);

#endif /* RAYCASTER_H */
