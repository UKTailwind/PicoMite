from fpdf import FPDF

class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'PicoMite Raycaster User Manual', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')

    def chapter_title(self, label):
        self.set_font('Helvetica', 'B', 12)
        self.cell(0, 10, label, 0, 1, 'L')
        self.ln(2)

    def section_title(self, label):
        self.set_font('Helvetica', 'B', 11)
        self.cell(0, 8, label, 0, 1, 'L')
        self.ln(1)

    def subsection_title(self, label):
        self.set_font('Helvetica', 'B', 10)
        self.cell(0, 7, label, 0, 1, 'L')
        self.ln(1)

    def chapter_body(self, body):
        self.set_font('Helvetica', '', 10)
        self.multi_cell(0, 5, body)
        self.ln()

    def code_block(self, code):
        self.set_font('Courier', '', 8)
        self.set_fill_color(240, 240, 240)
        self.multi_cell(0, 4, code, fill=True)
        self.ln()

    def param_table(self, params):
        self.set_font('Helvetica', 'B', 9)
        col1 = 40
        col2 = self.w - self.l_margin - self.r_margin - col1
        self.cell(col1, 6, 'Parameter', 1, 0, 'C')
        self.cell(col2, 6, 'Description', 1, 1, 'C')
        self.set_font('Helvetica', '', 9)
        for param, desc in params:
            self.cell(col1, 6, param, 1, 0, 'L')
            x = self.get_x()
            y = self.get_y()
            self.multi_cell(col2, 6, desc, 1)
            row_h = self.get_y() - y
            if row_h <= 6:
                pass
            else:
                pass
        self.ln()

    def simple_table(self, headers, rows, col_widths=None):
        if col_widths is None:
            n = len(headers)
            total = self.w - self.l_margin - self.r_margin
            col_widths = [total / n] * n
        self.set_font('Helvetica', 'B', 9)
        for i, h in enumerate(headers):
            self.cell(col_widths[i], 6, h, 1, 0, 'C')
        self.ln()
        self.set_font('Helvetica', '', 9)
        for row in rows:
            for i, val in enumerate(row):
                self.cell(col_widths[i], 6, str(val), 1, 0, 'L')
            self.ln()
        self.ln()

    def bullet_list(self, items):
        self.set_font('Helvetica', '', 10)
        for item in items:
            self.cell(5, 5, chr(0x95), 0, 0, 'R')
            self.cell(3, 5, '', 0, 0)
            self.multi_cell(0, 5, item)
        self.ln()

    def numbered_list(self, items):
        self.set_font('Helvetica', '', 10)
        for i, item in enumerate(items, 1):
            self.cell(8, 5, f'{i}.', 0, 0, 'R')
            self.cell(3, 5, '', 0, 0)
            self.multi_cell(0, 5, item)
        self.ln()

    def note_box(self, text):
        self.set_font('Helvetica', 'I', 9)
        self.set_fill_color(255, 255, 220)
        self.multi_cell(0, 5, text, fill=True)
        self.set_fill_color(255, 255, 255)
        self.ln()


pdf = PDF()
pdf.set_auto_page_break(auto=True, margin=20)
pdf.add_page()

# =====================================================================
# Overview
# =====================================================================
pdf.chapter_title('Overview')
pdf.chapter_body(
    "The RAY command set provides a Wolfenstein 3D-style first-person renderer "
    "for the PicoMite MMBasic on RP2350. It renders textured walls, patterned "
    "floors and ceilings, full-colour billboard sprites, and includes built-in "
    "collision detection, ray casting for interaction, and a minimap overlay.\n\n"
    "The raycaster renders directly into the current framebuffer at whatever "
    "resolution is set by the active MODE command. It reads the system HRes and "
    "VRes at render time, so it is not limited to a single resolution. Mode 2 "
    "(320x240, 4-bit RGB121 colour) is recommended for best performance. The "
    "user is responsible for creating the framebuffer and copying it to the display.\n\n"
    "Platform: RP2350 only (PicoMite, PicoMiteVGA, HDMI)."
)

# =====================================================================
# Quick Start
# =====================================================================
pdf.chapter_title('Quick Start')
pdf.code_block(
    "MODE 2\n"
    "CLS\n"
    "DIM INTEGER map%(15)\n"
    "' A simple 4x4 map: walls around the edge, open centre\n"
    "map%() = 1,1,1,1, 1,0,0,1, 1,0,0,1, 1,1,1,1\n"
    "\n"
    "FRAMEBUFFER CREATE\n"
    "FRAMEBUFFER WRITE F\n"
    "\n"
    "RAY MAP 4, 4, map%()\n"
    "RAY CAMERA 2.5, 2.5, 0, 66\n"
    "RAY COLOUR 12, 3, 8, 1, 1, 3\n"
    "\n"
    "DO\n"
    "  RAY RENDER\n"
    "  FRAMEBUFFER COPY F, N\n"
    "  k$ = INKEY$\n"
    '  IF k$ = "w" THEN RAY MOVE 0.1\n'
    '  IF k$ = "s" THEN RAY MOVE -0.1\n'
    '  IF k$ = "a" THEN RAY TURN -5\n'
    '  IF k$ = "d" THEN RAY TURN 5\n'
    "  IF k$ = CHR$(27) THEN EXIT DO\n"
    "LOOP\n"
    "\n"
    "RAY CLOSE\n"
    "FRAMEBUFFER CLOSE"
)

# =====================================================================
# Commands
# =====================================================================
pdf.chapter_title('Commands')

# RAY MAP
pdf.section_title('RAY MAP w, h, map%()')
pdf.chapter_body("Define the world grid.")
pdf.param_table([
    ('w', 'Map width in cells (1-256)'),
    ('h', 'Map height in cells (1-256)'),
    ('map%()', '1D integer array with w x h entries'),
])
pdf.chapter_body(
    "Each array element is a wall type:\n"
    "  0 = empty (passable space)\n"
    "  1-31 = wall (rendered using the wall definition assigned by RAY DEFINE)\n\n"
    "Every wall type from 1 to 31 has its own foreground colour, background colour, "
    "fill pattern, and an optional door flag. See RAY DEFINE to customise these. "
    "The defaults give types 1-15 green shades and types 16-31 brown/yellow shades "
    "with the door flag set.\n\n"
    "The array is stored row by row: element y * w + x corresponds to map cell (x, y)."
)

# RAY CAMERA
pdf.section_title('RAY CAMERA x!, y!, angle! [, fov!]')
pdf.chapter_body("Place and orient the viewer camera.")
pdf.param_table([
    ('x!, y!', 'Position in map space (floating-point, 0-based)'),
    ('angle!', 'Heading in degrees (0 = +X/east, 90 = +Y/south)'),
    ('fov!', 'Field of view in degrees (default 60, range 10-170)'),
])
pdf.note_box("Tip: Position the camera at x + 0.5, y + 0.5 to centre it within a map cell.")

# RAY COLOUR
pdf.section_title('RAY COLOUR floor_fg, ceil_fg [, floor_bg, ceil_bg, floor_pat, ceil_pat]')
pdf.chapter_body("Set the floor and ceiling appearance.")
pdf.subsection_title('2-argument form - solid colours:')
pdf.param_table([
    ('floor_fg', 'Floor colour (RGB121 palette index 0-15)'),
    ('ceil_fg', 'Ceiling colour (RGB121 palette index 0-15)'),
])
pdf.subsection_title('6-argument form - textured with fill patterns:')
pdf.param_table([
    ('floor_fg', 'Floor foreground colour (0-15)'),
    ('ceil_fg', 'Ceiling foreground colour (0-15)'),
    ('floor_bg', 'Floor background colour (0-15)'),
    ('ceil_bg', 'Ceiling background colour (0-15)'),
    ('floor_pat', 'Floor fill pattern index (0-31)'),
    ('ceil_pat', 'Ceiling fill pattern index (0-31)'),
])
pdf.chapter_body(
    "Pattern bits set to 1 draw in the foreground colour; bits set to 0 draw in the "
    "background colour."
)
pdf.note_box("Note: RAY COLOR is accepted as an alternative spelling.")

# RAY MOVE
pdf.section_title('RAY MOVE speed! [, strafe!]')
pdf.chapter_body("Move the camera with built-in collision detection.")
pdf.param_table([
    ('speed!', 'Forward speed (positive = forward, negative = backward)'),
    ('strafe!', 'Optional. Sideways speed (positive = right, negative = left)'),
])
pdf.chapter_body(
    "The engine uses a 0.25-unit bounding box around the camera. If the full move is "
    "blocked by a wall, it attempts wall sliding: first X-only movement, then Y-only. "
    "If completely blocked, the camera stays put."
)

# RAY TURN
pdf.section_title('RAY TURN degrees!')
pdf.chapter_body("Rotate the camera heading.")
pdf.param_table([
    ('degrees!', 'Rotation amount (positive = clockwise/right, negative = left)'),
])
pdf.chapter_body("The heading is automatically normalised to 0-360 degrees.")

# RAY CELL
pdf.section_title('RAY CELL x, y, value')
pdf.chapter_body("Write a value to a map cell at runtime.")
pdf.param_table([
    ('x', 'Cell X coordinate (0 to map width - 1)'),
    ('y', 'Cell Y coordinate (0 to map height - 1)'),
    ('value', 'Wall type (0 = empty, 1-31 = wall)'),
])
pdf.chapter_body(
    "Use this for doors, destructible walls, switches, or any dynamic map modification."
)

# RAY CAST
pdf.section_title('RAY CAST angle!')
pdf.chapter_body("Cast a single ray from the camera position at an absolute angle.")
pdf.param_table([
    ('angle!', 'Absolute angle in degrees'),
])
pdf.chapter_body(
    "Results are stored internally and retrieved with RAY(CASTDIST), RAY(CASTWALL), "
    "RAY(CASTSIDE), RAY(CASTX), RAY(CASTY).\n\n"
    "Useful for \"use\" buttons, shooting mechanics, proximity checks, or finding what "
    "the player is looking at."
)

# RAY SPRITE
pdf.section_title('RAY SPRITE id, spritenum, x!, y!')
pdf.chapter_body("Place or update a billboard sprite in the world.")
pdf.param_table([
    ('id', 'Raycaster sprite slot (0-31)'),
    ('spritenum', 'SPRITE buffer number (1-64)'),
    ('x!, y!', 'World-space position (floating-point)'),
])
pdf.chapter_body(
    "The sprite's full-colour 4bpp image is read from the sprite buffer and rendered "
    "as a billboard (always facing the camera). Pixels matching the current SPRITE "
    "TRANSPARENT colour are not drawn. Sprites are automatically depth-sorted and "
    "clipped against the wall z-buffer."
)
pdf.note_box(
    "Note: The sprite buffer must be loaded before calling RAY SPRITE. Use "
    "SPRITE LOADARRAY to create sprites from BASIC arrays, or SPRITE LOAD to load .spr files."
)

pdf.subsection_title('RAY SPRITE REMOVE id')
pdf.chapter_body("Remove sprite id from the raycaster (stop rendering it).")

pdf.subsection_title('RAY SPRITE CLEAR')
pdf.chapter_body("Remove all sprites.")

# RAY MINIMAP
pdf.section_title('RAY MINIMAP x, y, size')
pdf.chapter_body("Draw a top-down minimap overlay onto the framebuffer.")
pdf.param_table([
    ('x', 'Screen X position of the minimap'),
    ('y', 'Screen Y position of the minimap'),
    ('size', 'Size in pixels (longest map axis fits within this)'),
])
pdf.chapter_body(
    "The minimap scales the entire map to fit, preserving aspect ratio. Each wall cell "
    "is drawn in that wall definition's foreground colour. Door cells show their "
    "definition's foreground colour when closed, yellow when partially open, and black "
    "when fully open. Empty space is black, the player is a white dot with a direction "
    "indicator, and sprites are yellow dots. A dark green border is drawn around the edge.\n\n"
    "Call after RAY RENDER and before FRAMEBUFFER COPY."
)

# RAY DOOR
pdf.section_title('RAY DOOR x, y, offset!')
pdf.chapter_body("Set a sliding door at a map cell with a given open offset.")
pdf.param_table([
    ('x', 'Cell X coordinate (0 to map width - 1)'),
    ('y', 'Cell Y coordinate (0 to map height - 1)'),
    ('offset!', 'Door offset: 0.0 = fully closed, 1.0 = fully open'),
])
pdf.chapter_body(
    "The map cell at (x, y) must contain a wall type whose definition has the door "
    "flag set (see RAY DEFINE). When offset is between 0 and 1, the door is partially "
    "open - rays pass through the open portion and hit the remaining solid portion. "
    "When offset reaches 1.0, the cell becomes fully passable for both rays and movement.\n\n"
    "Up to 8 doors may be active simultaneously. To animate a door, call RAY DOOR each "
    "frame with an incrementally increasing or decreasing offset."
)

pdf.subsection_title('RAY DOOR CLOSE x, y')
pdf.chapter_body(
    "Remove the door slot at (x, y). The cell reverts to a normal solid wall. "
    "Call this after closing animation completes (offset reaches 0.0)."
)

pdf.subsection_title('RAY DOOR CLEAR')
pdf.chapter_body("Remove all active door slots.")

# RAY DEFINE
pdf.section_title('RAY DEFINE type, fg, bg, pattern [, door]')
pdf.chapter_body("Set the visual properties for a wall type.")
pdf.param_table([
    ('type', 'Wall type to define (1-31)'),
    ('fg', 'Foreground colour (RGB121 palette index 0-15)'),
    ('bg', 'Background colour (RGB121 palette index 0-15)'),
    ('pattern', 'Fill pattern index (0-31)'),
    ('door', 'Optional. 1 = door, 0 = normal wall (default 0)'),
])
pdf.chapter_body(
    "Every wall type from 1 to 31 has a definition that controls its rendered "
    "appearance. The definition is used for both X-side and Y-side hits; Y-side hits "
    "are automatically dimmed by the engine for depth cueing.\n\n"
    "Definitions persist until RAY CLOSE. Call RAY DEFINE after RAY MAP but before "
    "RAY RENDER."
)
pdf.subsection_title('Defaults (set by RAY MAP):')
pdf.simple_table(
    ['Types', 'fg', 'bg', 'pattern', 'door'],
    [
        ['1-15', 'GREEN (6)', 'MIDGREEN (4)', 'type - 1', '0'],
        ['16-31', 'YELLOW (14)', 'BROWN (12)', 'type - 1', '1'],
    ],
    [25, 35, 40, 35, 25]
)
pdf.subsection_title('Example - red brick walls for type 1:')
pdf.code_block("RAY DEFINE 1, 8, 10, 3       ' fg=RED, bg=RUST, pattern 3")
pdf.subsection_title('Example - custom blue door:')
pdf.code_block(
    "RAY DEFINE 7, 1, 3, 6, 1     ' fg=BLUE, bg=COBALT, pattern 6, door=1\n"
    "RAY CELL door_x, door_y, 7"
)

# RAY RENDER
pdf.section_title('RAY RENDER')
pdf.chapter_body(
    "Render the complete 3D scene into the current WriteBuf framebuffer. This draws:\n"
    "  1. Textured floor and ceiling (horizontal scanlines)\n"
    "  2. Textured walls (vertical columns via DDA raycasting)\n"
    "  3. Billboard sprites (depth-sorted, z-buffered)\n\n"
    "A framebuffer must be active (FRAMEBUFFER CREATE / FRAMEBUFFER WRITE F)."
)

# RAY CLOSE
pdf.section_title('RAY CLOSE')
pdf.chapter_body(
    "Free all raycaster state (map, column arrays, sprites). Called automatically on "
    "program end."
)

# =====================================================================
# Functions
# =====================================================================
pdf.add_page()
pdf.chapter_title('Functions')
pdf.chapter_body("All functions return values from the current raycaster state.")
pdf.simple_table(
    ['Function', 'Returns', 'Type'],
    [
        ['RAY(MAPW)', 'Map width', 'Integer'],
        ['RAY(MAPH)', 'Map height', 'Integer'],
        ['RAY(CAMX)', 'Camera X position', 'Float'],
        ['RAY(CAMY)', 'Camera Y position', 'Float'],
        ['RAY(CAMA)', 'Camera angle (degrees)', 'Float'],
        ['RAY(DIST col)', 'Perpendicular wall distance at column col', 'Float'],
        ['RAY(WALL col)', 'Wall type hit at column col', 'Integer'],
        ['RAY(CELL x, y)', 'Map cell value at (x, y)', 'Integer'],
        ['RAY(DOOR x, y)', 'Door offset, or -1.0 if not active', 'Float'],
        ['RAY(CASTDIST)', 'Distance from last RAY CAST', 'Float'],
        ['RAY(CASTWALL)', 'Wall type from last RAY CAST', 'Integer'],
        ['RAY(CASTSIDE)', 'Side hit (0=X-side, 1=Y-side)', 'Integer'],
        ['RAY(CASTX)', 'Map X cell from last RAY CAST', 'Integer'],
        ['RAY(CASTY)', 'Map Y cell from last RAY CAST', 'Integer'],
        ['RAY(SPRITES)', 'Count of active sprites', 'Integer'],
        ['RAY(SPRITEX id)', 'World X of sprite id', 'Float'],
        ['RAY(SPRITEY id)', 'World Y of sprite id', 'Float'],
        ['RAY(DEFINE t, p)', 'Wall def field: 0=fg,1=bg,2=pat,3=door', 'Integer'],
    ],
    [55, 70, 35]
)

# =====================================================================
# RGB121 Colour Palette
# =====================================================================
pdf.add_page()
pdf.chapter_title('RGB121 Colour Palette')
pdf.chapter_body(
    "The 4-bit RGB121 palette provides 16 colours. Each index encodes 1 bit red, "
    "2 bits green, 1 bit blue."
)
pdf.simple_table(
    ['Index', 'Name', 'RGB888', 'Appearance'],
    [
        ['0', 'BLACK', '000000', 'Black'],
        ['1', 'BLUE', '0000FF', 'Blue'],
        ['2', 'MYRTLE', '004000', 'Dark green'],
        ['3', 'COBALT', '0040FF', 'Dark cyan-blue'],
        ['4', 'MIDGREEN', '008000', 'Medium green'],
        ['5', 'CERULEAN', '0080FF', 'Sky blue'],
        ['6', 'GREEN', '00FF00', 'Bright green'],
        ['7', 'CYAN', '00FFFF', 'Cyan'],
        ['8', 'RED', 'FF0000', 'Red'],
        ['9', 'MAGENTA', 'FF00FF', 'Magenta'],
        ['10', 'RUST', 'FF4000', 'Dark orange'],
        ['11', 'FUCHSIA', 'FF40FF', 'Pink'],
        ['12', 'BROWN', 'FF8000', 'Brown/orange'],
        ['13', 'LILAC', 'FF80FF', 'Light pink'],
        ['14', 'YELLOW', 'FFFF00', 'Yellow'],
        ['15', 'WHITE', 'FFFFFF', 'White'],
    ],
    [20, 35, 30, 45]
)

# Wall Definitions
pdf.section_title('Wall Definitions')
pdf.chapter_body(
    "Every wall type from 1 to 31 has a fully customisable wall definition comprising "
    "a foreground colour, background colour, fill pattern, and a door flag. Use RAY "
    "DEFINE to set these; the defaults are:"
)
pdf.simple_table(
    ['Types', 'Foreground', 'Background', 'Pattern', 'Door'],
    [
        ['1-15', 'GREEN (6)', 'MIDGREEN (4)', 'type - 1', '0 (no)'],
        ['16-31', 'YELLOW (14)', 'BROWN (12)', 'type - 1', '1 (yes)'],
    ],
    [25, 35, 35, 30, 25]
)
pdf.chapter_body(
    "Y-side depth cueing: When a wall column is a Y-side hit, the foreground and "
    "background colours are automatically dimmed by decrementing the green channel of "
    "the RGB121 value (e.g., GREEN 6 -> MIDGREEN 4, YELLOW 14 -> BROWN 12). This gives "
    "walls a bright/dark shade difference that conveys depth without requiring a separate "
    "colour definition per side."
)

# =====================================================================
# Fill Patterns
# =====================================================================
pdf.chapter_title('Fill Patterns')
pdf.chapter_body(
    "Wall and floor/ceiling textures use the Turtle graphics fill patterns (indices "
    "0-31). Each pattern is an 8x8 grid of 1-bit pixels. During rendering, pattern "
    "bit 1 selects the foreground colour and bit 0 selects the background colour.\n\n"
    "Each wall definition specifies a pattern index (0-31) via RAY DEFINE. By default, "
    "pattern = wall_type - 1, giving each wall type a unique texture. Patterns are "
    "tiled twice per map cell for increased texture density."
)

# =====================================================================
# Sprite System
# =====================================================================
pdf.chapter_title('Sprite System')
pdf.chapter_body(
    "Raycaster sprites use the standard PicoMite SPRITE system for image data. Sprites "
    "are loaded into sprite buffers (1-64) using SPRITE LOAD or SPRITE LOADARRAY, then "
    "placed into the raycaster world using RAY SPRITE."
)

pdf.section_title('Loading Sprites from Arrays')
pdf.code_block(
    "' Define a 16-colour 8x8 sprite using RGB888 colour values\n"
    "DIM INTEGER pixels%(63)\n"
    "' ... fill pixels% with RGB888 colours ...\n"
    "SPRITE LOADARRAY buffer_num, width, height, pixels%()"
)
pdf.chapter_body(
    "Each pixel is an RGB888 value (e.g., &hFF0000 for red). The system automatically "
    "converts to the 4-bit RGB121 palette. Pixels are packed two per byte (even pixel "
    "in low nibble, odd pixel in high nibble)."
)

pdf.section_title('Transparency')
pdf.code_block("SPRITE TRANSPARENT colour_index")
pdf.chapter_body(
    "Default is 0 (BLACK). Any sprite pixel matching this index is not drawn, allowing "
    "the wall/floor behind to show through."
)

pdf.section_title('Sprite Rendering')
pdf.chapter_body("During RAY RENDER, sprites are:")
pdf.numbered_list([
    "Sorted by distance from camera (furthest first - painter's algorithm)",
    "Projected onto the screen as billboards (always face the camera)",
    "Scaled based on distance, preserving the sprite's aspect ratio",
    "Clipped per-column against the wall z-buffer (walls occlude sprites)",
    "Drawn pixel-by-pixel, skipping transparent pixels",
])
pdf.chapter_body(
    "Up to 32 raycaster sprites can be active simultaneously, referencing up to 64 "
    "sprite buffers."
)

# =====================================================================
# Implementing Doors
# =====================================================================
pdf.add_page()
pdf.chapter_title('Implementing Doors')
pdf.chapter_body(
    "Doors use the RAY DOOR command for smooth sliding animation. Any wall type can "
    "act as a door if its definition has the door flag set (RAY DEFINE type, fg, bg, "
    "pattern, 1). By default, types 16-31 have the door flag."
)

pdf.section_title('Basic Door Setup')
pdf.code_block(
    "' Place a door wall (type 31 has door flag set by default)\n"
    "RAY CELL door_x, door_y, 31\n"
    "\n"
    "' Or define a custom door type (e.g. red/rust with pattern 7)\n"
    "RAY DEFINE 5, 8, 10, 7, 1\n"
    "RAY CELL door_x, door_y, 5"
)

pdf.section_title('Animated Opening')
pdf.code_block(
    "' Start opening: create a door slot and animate over multiple frames\n"
    "door_offset! = 0.0\n"
    "DO WHILE door_offset! < 1.0\n"
    "  door_offset! = door_offset! + 0.1\n"
    "  IF door_offset! > 1.0 THEN door_offset! = 1.0\n"
    "  RAY DOOR door_x, door_y, door_offset!\n"
    "  RAY RENDER\n"
    "  FRAMEBUFFER COPY F, N\n"
    "  PAUSE 50\n"
    "LOOP\n"
    "' Door is now fully open - player can walk through"
)

pdf.section_title('Animated Closing')
pdf.code_block(
    "' Close: animate offset back to 0, then release the slot\n"
    "DO WHILE door_offset! > 0.0\n"
    "  door_offset! = door_offset! - 0.1\n"
    "  IF door_offset! < 0.0 THEN door_offset! = 0.0\n"
    "  RAY DOOR door_x, door_y, door_offset!\n"
    "  RAY RENDER\n"
    "  FRAMEBUFFER COPY F, N\n"
    "  PAUSE 50\n"
    "LOOP\n"
    "RAY DOOR CLOSE door_x, door_y\n"
    "' Door is fully closed and slot is released"
)

pdf.section_title('How It Works')
pdf.chapter_body(
    "When RAY DOOR sets an offset between 0 and 1, the door slides open from one side. "
    "Rays that hit the open portion pass through to the corridor behind; rays that hit "
    "the remaining solid portion render the door texture. The door blocks movement until "
    "offset reaches 1.0, at which point the cell becomes fully passable.\n\n"
    "The minimap shows door state: the definition's foreground colour when closed, "
    "yellow when partially open, empty when fully open."
)

# =====================================================================
# Coordinate System
# =====================================================================
pdf.chapter_title('Coordinate System')
pdf.bullet_list([
    "Map coordinates are 0-based. Cell (0,0) is the top-left corner.",
    "X axis increases to the right (east).",
    "Y axis increases downward (south).",
    "Angle 0 degrees points in the +X direction (east). Angles increase clockwise: "
    "90 = south, 180 = west, 270 = north.",
    "Camera and sprite positions use floating-point coordinates. A position of (2.5, 3.5) "
    "is the centre of cell (2, 3).",
])

# =====================================================================
# Typical Game Loop
# =====================================================================
pdf.chapter_title('Typical Game Loop')
pdf.code_block(
    "MODE 2\n"
    "CLS\n"
    "' ... define map array ...\n"
    "FRAMEBUFFER CREATE\n"
    "FRAMEBUFFER WRITE F\n"
    "\n"
    "RAY MAP w, h, map%()\n"
    "RAY CAMERA start_x, start_y, start_angle, 66\n"
    "RAY COLOUR floor_fg, ceil_fg, floor_bg, ceil_bg, floor_pat, ceil_pat\n"
    "\n"
    "' ... load sprites, place them ...\n"
    "\n"
    "DO\n"
    "  k$ = INKEY$\n"
    "  IF k$ = CHR$(27) THEN EXIT DO\n"
    "\n"
    "  ' Movement\n"
    '  IF k$ = "w" THEN RAY MOVE 0.15\n'
    '  IF k$ = "s" THEN RAY MOVE -0.15\n'
    '  IF k$ = "a" THEN RAY TURN -5\n'
    '  IF k$ = "d" THEN RAY TURN 5\n'
    "\n"
    "  ' Interaction\n"
    '  IF k$ = " " THEN\n'
    "    RAY CAST RAY(CAMA)\n"
    "    IF RAY(CASTDIST) < 2.0 THEN\n"
    "      ' ... handle what was hit ...\n"
    "    ENDIF\n"
    "  ENDIF\n"
    "\n"
    "  ' Render\n"
    "  RAY RENDER\n"
    "  RAY MINIMAP 2, 2, 48\n"
    "  FRAMEBUFFER COPY F, N\n"
    "LOOP\n"
    "\n"
    "RAY CLOSE\n"
    "FRAMEBUFFER CLOSE"
)

# =====================================================================
# Limitations
# =====================================================================
pdf.chapter_title('Limitations')
pdf.bullet_list([
    "RP2350 only - not available on RP2040 builds.",
    "Maximum map size: 256 x 256 cells.",
    "Maximum raycaster sprites: 32 active at once.",
    "Maximum active doors: 8 simultaneously.",
    "Maximum sprite buffers: 64 (shared with the standard SPRITE system).",
    "Wall types: 1-31 only (capped at the number of Turtle fill patterns).",
    "Colour depth: 4-bit RGB121 (16 colours). All 16 colours are available for walls "
    "(via RAY DEFINE), floor, ceiling, and sprites.",
    "Resolution: Uses the current HRes and VRes - works at any resolution. Mode 2 "
    "(320x240) is recommended; higher resolutions work but render more slowly.",
    "Single height level: No floor/ceiling height variation (classic Wolfenstein-style).",
])

# =====================================================================
# Appendix A: Technical Implementation
# =====================================================================
pdf.add_page()
pdf.chapter_title('Appendix A: Technical Implementation')

pdf.section_title('Architecture')
pdf.chapter_body(
    "The raycaster is implemented entirely in C (Raycaster.c, ~1600 lines) with a "
    "header (Raycaster.h). It integrates into the MMBasic command/function dispatch "
    "system via cmd_ray() and fun_ray(), registered in AllCommands.h. State cleanup is "
    "hooked into CloseAllFiles() in FileIO.c.\n\n"
    "All state is held in a single heap-allocated RayState structure, accessed through "
    "the static pointer rstate. Memory is managed using PicoMite's GetMemory()/"
    "FreeMemory() allocator."
)

pdf.section_title('DDA Algorithm')
pdf.chapter_body(
    "The wall-casting engine uses the standard Digital Differential Analyzer (DDA) algorithm:"
)
pdf.numbered_list([
    "For each screen column, a ray is cast from the camera position through the "
    "corresponding point on the camera plane.",
    "The ray steps through the grid one cell boundary at a time, alternating between "
    "X-side and Y-side crossings, always choosing the nearer crossing.",
    "When the ray enters a cell with a non-zero wall type, the DDA loop terminates.",
    "The perpendicular distance (not Euclidean) is computed to avoid fisheye distortion: "
    "perp_dist = side_dist - delta_dist for the last step.",
    "The wall strip height is screen_height / perp_dist.",
])
pdf.chapter_body(
    "The perpendicular distance for each column is stored in col_dist[] and reused as "
    "the z-buffer for sprite clipping."
)

pdf.section_title('Wall Rendering')
pdf.chapter_body(
    "Walls are drawn as textured vertical strips using ray_vline_textured(). Each wall "
    "type selects a Turtle fill pattern (8x8, 1-bit). The texture coordinates are:\n\n"
    "Horizontal (tex_x): Derived from the fractional wall-hit position, multiplied by "
    "16 and masked to 0-7. This tiles the 8-texel pattern twice across each wall face.\n\n"
    "Vertical (tex_y): Mapped from the screen Y range with a 16x multiplier and & 7 "
    "wrapping, tiling the pattern twice vertically per wall height.\n\n"
    "Pattern bits select between a foreground/background colour pair. Each wall type has "
    "a configurable foreground/background colour pair and fill pattern, set via RAY DEFINE. "
    "Y-side hits are automatically dimmed by decrementing the RGB121 green channel, "
    "providing a depth cue without requiring separate per-side colours."
)

pdf.section_title('Door Rendering')
pdf.chapter_body(
    "Sliding doors are handled within the DDA loop. When a ray enters a cell whose wall "
    "definition has the door flag set, the engine checks whether an active door slot "
    "exists for that cell:\n\n"
    "1. If the door offset is 0.0 (or no slot exists), the cell is treated as a normal "
    "solid wall.\n"
    "2. If the door offset is >= 1.0, the cell is fully open - the ray passes through "
    "and the DDA continues to the next cell.\n"
    "3. For intermediate offsets (0.0-1.0), the engine computes the fractional hit "
    "position on the wall face. If the fractional position is less than the door offset, "
    "the ray passes through the open portion. Otherwise, the ray hits the remaining "
    "solid door.\n\n"
    "This creates a sliding-door effect: the opening grows from one side of the wall "
    "face as the offset increases. Collision detection also respects door state: cells "
    "with a door offset >= 1.0 are passable; partially-open doors still block movement.\n\n"
    "Up to 8 doors can be active simultaneously, stored in fixed-size slots within RayState."
)

pdf.section_title('Floor and Ceiling Rendering')
pdf.chapter_body(
    "The floor/ceiling renderer uses a horizontal scanline approach, which is more "
    "efficient than per-pixel ray casting:"
)
pdf.numbered_list([
    "For each row below the horizon, the row distance is calculated: "
    "row_dist = half_screen_height / (row - horizon).",
    "The world-space floor position at the leftmost and rightmost screen edges is "
    "computed using the camera's left and right ray directions.",
    "A linear interpolation step is computed per column, and the inner loop advances by "
    "addition only (no per-pixel division).",
    "Texture coordinates are computed by multiplying the world position by 16 and masking "
    "to 0-7, tiling the pattern twice per map cell.",
    "The ceiling row is mirrored: ceil_y = screen_height - 1 - floor_y.",
    "Pixels are written two at a time (even/odd nibble packing) to minimise memory operations.",
])

pdf.section_title('Sprite Rendering')
pdf.chapter_body(
    "Billboard sprites are rendered after walls, using the wall z-buffer for per-column "
    "occlusion:"
)
pdf.numbered_list([
    "Active sprites are collected and sorted by squared distance from the camera "
    "(furthest first - painter's algorithm).",
    "Each sprite's world position is transformed into camera space using the inverse of "
    "the 2x2 camera matrix (direction x plane).",
    "The sprite is projected to a screen rectangle based on its distance, with width "
    "scaled by the sprite image's aspect ratio.",
    "For each visible screen column (not occluded by a closer wall), the sprite's 4bpp "
    "pixel data is sampled from the SPRITE buffer.",
    "Pixels matching sprite_transparent are skipped. Other pixels are written directly "
    "into the framebuffer.",
])
pdf.chapter_body(
    "The 4bpp pixel format matches the framebuffer layout: even pixels in the low "
    "nibble, odd pixels in the high nibble of each byte."
)

pdf.section_title('Collision Detection')
pdf.chapter_body("RAY MOVE implements collision detection with wall sliding:")
pdf.numbered_list([
    "A 0.25-unit radius bounding box is checked at the target position.",
    "All four corners of the box are tested against the map grid.",
    "If any corner overlaps a wall cell, the full move is blocked.",
    "The engine then tries X-only movement (slide along Y walls).",
    "If that's also blocked, it tries Y-only movement (slide along X walls).",
    "If completely blocked, the camera doesn't move.",
])

pdf.section_title('RAY CAST')
pdf.chapter_body(
    "RAY CAST runs the same DDA algorithm as the main renderer but for a single ray at "
    "an arbitrary angle. Results are stored in the cast_* fields of the raycaster state "
    "and queried via RAY(CASTDIST), RAY(CASTWALL), RAY(CASTSIDE), RAY(CASTX), RAY(CASTY)."
)

pdf.section_title('Minimap')
pdf.chapter_body(
    "The minimap iterates over all map cells, scaling to fit the longest axis within the "
    "specified pixel size while preserving the map's aspect ratio. Active sprites are "
    "drawn as coloured dots. The player is shown as a white dot with a 2-pixel direction "
    "indicator computed from the camera angle."
)

pdf.section_title('Memory Usage')
pdf.simple_table(
    ['Item', 'Size'],
    [
        ['RayState structure', '~2.6 KB (32 wall defs, 32 sprites, 8 doors)'],
        ['Map storage', 'w x h bytes (max 64 KB for 256x256)'],
        ['Column distance array', 'HRes x 4 bytes (1280 for 320 cols)'],
        ['Column wall-type array', 'HRes bytes (320)'],
    ],
    [55, 105]
)
pdf.chapter_body(
    "Total overhead for a typical 57x51 map at 320x240: approximately 6.5 KB plus the "
    "framebuffer."
)

pdf.section_title('Compilation')
pdf.chapter_body(
    "The raycaster is compiled with -Os (optimise for size) across all RP2350 build "
    "variants. It is conditionally included via #ifdef rp2350 in AllCommands.h and "
    "CMakeLists.txt."
)

# =====================================================================
# Appendix B: Demo Program
# =====================================================================
pdf.add_page()
pdf.chapter_title('Appendix B: Demo Program')
pdf.chapter_body(
    "The following program demonstrates all raycaster features in a continuous automated "
    "walkthrough. It defines a 57x51 map, creates five colourful sprites, builds a wall "
    "with an animated sliding door, and runs a pre-programmed round-trip sequence that "
    "loops indefinitely."
)

pdf.section_title('Demo Code')
pdf.code_block(
    "' Raycaster Demo for PicoMite MMBasic (RP2350)\n"
    "' Continuous auto-play loop with animated sliding door\n"
    "' Press ESC at any time to quit\n"
    "OPTION EXPLICIT\n"
    "MODE 2\n"
    "CLS\n"
    "\n"
    "CONST MAP_W = 57\n"
    "CONST MAP_H = 51\n"
    "CONST FOV = 66\n"
    "CONST START_X = 25.5\n"
    "CONST START_Y = 23.5\n"
    "CONST START_A = 0\n"
    "\n"
    "DIM INTEGER x%, y%, i%, cx%, cy%, door_x%, door_y%, door_wall%\n"
    "DIM k$\n"
    "DIM FLOAT moveSpeed, rotSpeed\n"
    "moveSpeed = 0.2\n"
    "rotSpeed = 5.625   ' exactly 90 degrees per 16 steps\n"
    "\n"
    "DIM FLOAT door_offset!, door_target!, door_step!\n"
    "DIM INTEGER door_animating%\n"
    "door_step! = 0.1\n"
    "\n"
    "DIM INTEGER world%(MAP_W * MAP_H - 1)\n"
    "RESTORE MapData1\n"
    "FOR y% = 0 TO MAP_H - 1\n"
    '  READ k$: k$ = k$ + "1"\n'
    "  FOR x% = 0 TO MAP_W - 1\n"
    "    world%(y% * MAP_W + x%) = VAL(MID$(k$, x% + 1, 1))\n"
    "  NEXT x%\n"
    "NEXT y%\n"
    "\n"
    "' Vary wall types for texture variation\n"
    "FOR y% = 0 TO MAP_H - 1\n"
    "  FOR x% = 0 TO MAP_W - 1\n"
    "    IF world%(y% * MAP_W + x%) > 0 THEN\n"
    "      world%(y% * MAP_W + x%) = ((x% + y%) MOD 5) + 1\n"
    "    ENDIF\n"
    "  NEXT x%\n"
    "NEXT y%\n"
    "\n"
    "FRAMEBUFFER CREATE\n"
    "FRAMEBUFFER WRITE F\n"
    "RAY MAP MAP_W, MAP_H, world%()\n"
    "RAY COLOUR 12, 3, 8, 1, 1, 3\n"
    "\n"
    "' ... (sprite creation and placement code) ...\n"
    "\n"
    "DIM seq$\n"
    'seq$ = "P5W22P3O1P10W15P5D16W5P5W5D16W30D16"\n'
    'seq$ = seq$ + "W5P5W5P5D16W15P3C1P10A16W5P5D32"\n'
    'seq$ = seq$ + "W5D16W22D16D16P5"\n'
    "\n"
    "DO  ' Main loop - runs continuously until ESC\n"
    "  RAY CAMERA START_X, START_Y, START_A, FOV\n"
    "  ' ... rebuild door wall, reset animation state ...\n"
    "  ' ... execute sequence with per-frame rendering ...\n"
    "  PAUSE 500\n"
    "LOOP"
)

pdf.section_title('How the Demo Works')

pdf.subsection_title('Initialisation')
pdf.chapter_body(
    "The demo starts by setting Mode 2 (320x240 @ 4bpp RGB121), then reads a 57x51 map "
    "from DATA statements. Each DATA line is a string of digits where 1 = wall and "
    "0 = empty. After reading, wall cells are varied to types 1-5 using a "
    "(x + y) MOD 5 + 1 formula, ensuring different fill patterns appear across the map.\n\n"
    "A framebuffer is created and the raycaster is initialised with RAY MAP, RAY CAMERA, "
    "and RAY COLOUR (brown floor pattern 1, cobalt ceiling pattern 3).\n\n"
    "The rotation speed is set to exactly 5.625 degrees per step, so that D16 (16 "
    "right-turn steps) makes exactly 90 degrees and four such turns return to the "
    "original heading - essential for the seamless loop."
)

pdf.subsection_title('Door Construction')
pdf.chapter_body(
    "Six RAY CELL commands build a north-south wall segment at x=30 (rows 19-24). Five "
    "cells use wall type 3 (green by default), while the cell at y=23 uses type 31 "
    "(brown/yellow with door flag by default). From the starting position, this wall is "
    "clearly visible ahead with the door panel standing out."
)

pdf.subsection_title('Sprite Creation')
pdf.chapter_body("Five 8x8 sprites are created procedurally using SPRITE LOADARRAY:")
pdf.simple_table(
    ['Buffer', 'Design', 'Method'],
    [
        ['1', 'Red cross', '2-pixel-wide cross at cols 3-4 and rows 3-4'],
        ['2', 'Yellow diamond', 'Manhattan distance from centre <= 3'],
        ['3', 'Green/cyan stripes', 'Alternating columns'],
        ['4', 'Magenta/blue check', '(x + y) MOD 2 test'],
        ['5', 'White ring', 'Border pixels and corner diagonals'],
    ],
    [25, 50, 85]
)

pdf.subsection_title('Door Animation System')
pdf.chapter_body(
    "The demo uses per-frame door animation driven by BASIC variables:\n\n"
    "  door_offset! - current door position (0.0-1.0)\n"
    "  door_target! - where the door is heading (0.0 or 1.0)\n"
    "  door_animating% - whether animation is active\n"
    "  door_step! - offset change per frame (0.1 = 10 frames to fully open/close)\n\n"
    "The O command sets door_target! = 1.0 and starts animating. Each frame, if "
    "door_animating% is set, the offset moves toward the target by door_step!. When the "
    "target is reached, animation stops. For closing (C command), the same logic runs in "
    "reverse; when offset reaches 0.0, RAY DOOR CLOSE releases the slot."
)

pdf.subsection_title('Sequence Engine')
pdf.chapter_body(
    "The auto-play system uses a compact string encoding: each command is a single letter "
    "followed by a repeat count."
)
pdf.simple_table(
    ['Letter', 'Action'],
    [
        ['W', 'Walk forward (RAY MOVE moveSpeed)'],
        ['S', 'Walk backward (RAY MOVE -moveSpeed)'],
        ['A', 'Turn left (RAY TURN -rotSpeed)'],
        ['D', 'Turn right (RAY TURN rotSpeed)'],
        ['O', 'Start door open animation'],
        ['C', 'Start door close animation'],
        ['P', 'Pause (render without moving)'],
    ],
    [25, 135]
)
pdf.chapter_body(
    "The parser reads one letter, then digits for the repeat count (e.g., W22 = walk "
    "forward 22 steps, D16 = turn right 16 steps = 90 degrees). Each step renders a "
    "frame, draws the minimap, adds a crosshair, and copies the framebuffer to screen "
    "with a 50ms delay."
)

pdf.subsection_title('Continuous Loop')
pdf.chapter_body(
    "The entire demo is wrapped in an outer DO...LOOP. At the top of each iteration, the "
    "camera is reset to the start position (25.5, 23.5) facing east, the door wall is "
    "rebuilt via RAY CELL commands, door animation state is cleared, and the sequence "
    "replays from the beginning. This creates a seamless continuous demonstration. "
    "Pressing ESC exits cleanly via GOTO Done."
)

# =====================================================================
# Output
# =====================================================================
pdf.output('Raycaster_User_Manual.pdf')
print("Generated Raycaster_User_Manual.pdf")
