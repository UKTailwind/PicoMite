from fpdf import FPDF

class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'SPRITE Command and Function Reference Manual', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')

    def chapter_title(self, label):
        self.set_font('Helvetica', 'B', 14)
        self.set_fill_color(200, 200, 200)
        self.cell(0, 8, label, 0, 1, 'L', fill=True)
        self.ln(2)

    def section_title(self, label):
        self.set_font('Helvetica', 'B', 12)
        self.cell(0, 7, label, 0, 1, 'L')
        self.ln(1)

    def subsection_title(self, label):
        self.set_font('Helvetica', 'B', 11)
        self.cell(0, 6, label, 0, 1, 'L')
        self.ln(1)

    def body_text(self, text):
        self.set_font('Helvetica', '', 10)
        self.multi_cell(0, 5, text)
        self.ln(2)

    def code_block(self, code):
        self.set_font('Courier', '', 9)
        self.set_fill_color(240, 240, 240)
        self.multi_cell(0, 4, code, fill=True)
        self.ln(2)

    def table_row(self, col1, col2, widths=(40, 150), header=False):
        if header:
            self.set_font('Helvetica', 'B', 9)
            self.set_fill_color(220, 220, 220)
        else:
            self.set_font('Helvetica', '', 9)
            self.set_fill_color(255, 255, 255)
        self.cell(widths[0], 6, col1, 1, 0, 'L', fill=header)
        self.cell(widths[1], 6, col2, 1, 1, 'L', fill=header)

pdf = PDF()
pdf.set_auto_page_break(auto=True, margin=15)
pdf.add_page()

# Overview
pdf.chapter_title('Overview')
pdf.body_text(
    "The SPRITE system provides firmware-accelerated sprite graphics for the PicoMite. "
    "Sprites are rectangular graphic objects that can be moved independently over a background, "
    "with automatic collision detection and layer management.\n\n"
    "Key Features:\n"
    "- Up to 64 sprites (numbered 1-64)\n"
    "- Up to 5 layers (0-4) for z-ordering\n"
    "- Automatic sprite-to-sprite collision detection\n"
    "- Edge-of-screen collision detection\n"
    "- Static object collision detection\n"
    "- Sprite rotation and mirroring\n"
    "- Efficient batch movement operations\n"
    "- Background scrolling with wrap-around"
)

# SPRITE Commands
pdf.chapter_title('SPRITE Commands')

# Loading and Creating Sprites
pdf.section_title('Loading and Creating Sprites')

pdf.subsection_title('SPRITE LOAD')
pdf.body_text("Loads sprites from a sprite definition file.")
pdf.code_block("SPRITE LOAD filename$ [, start_sprite] [, mode]")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('filename$', 'Path to the sprite file (.spr extension added if omitted)')
pdf.table_row('start_sprite', 'First sprite buffer to load into (1-64, default: 1)')
pdf.table_row('mode', 'Color palette mode: 0 = standard, 1 = alternate (default: 0)')
pdf.ln(2)
pdf.body_text(
    "Sprite File Format: The first line contains: width, count [, height]. "
    "If height is omitted, sprites are assumed square. Each sprite is defined as height lines of "
    "width characters. Characters 0-9 and A-F represent colors (hex values). Space represents transparent pixels. "
    "Lines starting with apostrophe are comments."
)

pdf.subsection_title('SPRITE LOADARRAY')
pdf.body_text("Loads a sprite from a numeric array.")
pdf.code_block("SPRITE LOADARRAY #n, width, height, array()")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('#n', 'Sprite buffer number (1-64)')
pdf.table_row('width', 'Sprite width in pixels')
pdf.table_row('height', 'Sprite height in pixels')
pdf.table_row('array()', 'Numeric array containing color values')
pdf.ln(2)

pdf.subsection_title('SPRITE LOADPNG (RP2350 only)')
pdf.body_text("Loads a sprite from a PNG file.")
pdf.code_block("SPRITE LOADPNG #n, filename$ [, transparent] [, cutoff]")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('#n', 'Sprite buffer number (1-64)')
pdf.table_row('filename$', 'Path to PNG file (.png extension added if omitted)')
pdf.table_row('transparent', 'Transparent color index (0-15, default: 0)')
pdf.table_row('cutoff', 'Alpha threshold for transparency (1-254, default: 30)')
pdf.ln(2)

pdf.subsection_title('SPRITE LOADBMP')
pdf.body_text("Loads a sprite from a BMP file.")
pdf.code_block("SPRITE LOADBMP #n, filename$ [, x_offset, y_offset, width, height]")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('#n', 'Sprite buffer number (1-64)')
pdf.table_row('filename$', 'Path to BMP file (.bmp extension added if omitted)')
pdf.table_row('x_offset', 'X offset within image (default: 0)')
pdf.table_row('y_offset', 'Y offset within image (default: 0)')
pdf.table_row('width', 'Width to load (default: full image)')
pdf.table_row('height', 'Height to load (default: full image)')
pdf.ln(2)

pdf.subsection_title('SPRITE READ')
pdf.body_text("Reads a sprite directly from the screen.")
pdf.code_block("SPRITE READ #n, x, y, width, height")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('#n', 'Sprite buffer number (1-64)')
pdf.table_row('x, y', 'Top-left corner of screen area to read')
pdf.table_row('width, height', 'Size of area to capture')
pdf.ln(2)

pdf.subsection_title('SPRITE COPY')
pdf.body_text("Creates copies of a sprite that share the same image data.")
pdf.code_block("SPRITE COPY #source, #dest, count")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('#source', 'Source sprite buffer number')
pdf.table_row('#dest', 'First destination sprite buffer number')
pdf.table_row('count', 'Number of copies to create')
pdf.body_text(
    "Notes: Copies share the same image data (memory efficient). Each copy has its own position and state. "
    "Cannot copy a copy. Must close all copies before closing the source."
)

# Displaying Sprites
pdf.add_page()
pdf.section_title('Displaying Sprites')

pdf.subsection_title('SPRITE SHOW')
pdf.body_text("Displays a sprite at a specified position.")
pdf.code_block("SPRITE SHOW #n, x, y, layer [, rotation]")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('#n', 'Sprite buffer number (1-64)')
pdf.table_row('x, y', 'Display position (can extend off-screen)')
pdf.table_row('layer', 'Display layer (0-4)')
pdf.table_row('rotation', 'Rotation/mirror value (0-7, default: 0)')
pdf.ln(2)

pdf.body_text("Rotation Values:")
pdf.table_row('Value', 'Effect', header=True)
pdf.table_row('0', 'Normal')
pdf.table_row('1', 'Mirror horizontal')
pdf.table_row('2', 'Mirror vertical')
pdf.table_row('3', 'Mirror both (180 degree rotation)')
pdf.table_row('4-7', 'Same as 0-3 but with transparency disabled')
pdf.ln(2)

pdf.body_text("Layer Behavior: Layer 0 scrolls with background (SPRITE SCROLL). Layers 1-4 are fixed position, rendered in order.")

pdf.body_text("Example:")
pdf.code_block("SPRITE SHOW #1, 100, 50, 1, 0")

pdf.subsection_title('SPRITE SHOW SAFE')
pdf.body_text("Displays a sprite safely, properly handling overlapping sprites.")
pdf.code_block("SPRITE SHOW SAFE #n, x, y, layer [, rotation] [, newbuffer]")
pdf.body_text(
    "Use when moving sprites that may overlap with other sprites. Automatically hides and redraws overlapping sprites. "
    "Slower than SPRITE SHOW but produces correct results."
)

pdf.subsection_title('SPRITE WRITE')
pdf.body_text("Draws a sprite directly to the screen without tracking.")
pdf.code_block("SPRITE WRITE #n, x, y [, rotation]")
pdf.body_text(
    "Does not store background or track position. Does not participate in collision detection. "
    "Use for static decorative elements."
)

# Hiding Sprites
pdf.section_title('Hiding Sprites')

pdf.subsection_title('SPRITE HIDE')
pdf.body_text("Hides a single sprite.")
pdf.code_block("SPRITE HIDE #n")
pdf.body_text("Restores the background where the sprite was displayed. Sprite remains loaded and can be shown again.")

pdf.subsection_title('SPRITE HIDE SAFE')
pdf.body_text("Safely hides a sprite, handling overlapping sprites.")
pdf.code_block("SPRITE HIDE SAFE #n")
pdf.body_text("Properly redraws overlapping sprites. Slower than SPRITE HIDE but produces correct results.")

pdf.subsection_title('SPRITE HIDE ALL')
pdf.body_text("Hides all currently displayed sprites.")
pdf.code_block("SPRITE HIDE ALL")
pdf.body_text("Sprites remain loaded and can be restored. Use before drawing to the background.")

pdf.subsection_title('SPRITE RESTORE')
pdf.body_text("Restores all sprites hidden by SPRITE HIDE ALL.")
pdf.code_block("SPRITE RESTORE")

# Moving Sprites
pdf.add_page()
pdf.section_title('Moving Sprites')

pdf.subsection_title('SPRITE NEXT')
pdf.body_text("Sets the next position for a sprite (used with SPRITE MOVE).")
pdf.code_block("SPRITE NEXT #n, x, y")
pdf.body_text("Does not immediately move the sprite. Position is applied when SPRITE MOVE is called.")

pdf.subsection_title('SPRITE MOVE')
pdf.body_text("Executes all pending SPRITE NEXT movements.")
pdf.code_block("SPRITE MOVE")
pdf.body_text("Moves all sprites to their NEXT positions in one operation. More efficient than individual SPRITE SHOW commands.")

pdf.subsection_title('SPRITE SWAP')
pdf.body_text("Swaps a displayed sprite with a hidden sprite of the same size.")
pdf.code_block("SPRITE SWAP #displayed, #hidden [, rotation]")
pdf.body_text("Efficient sprite animation technique. Both sprites must be the same size.")

# Scrolling
pdf.section_title('Scrolling')

pdf.subsection_title('SPRITE SCROLL')
pdf.body_text("Scrolls the background and layer 0 sprites.")
pdf.code_block("SPRITE SCROLL x, y [, fill_color]")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('x', 'Horizontal scroll amount (positive = right)')
pdf.table_row('y', 'Vertical scroll amount (positive = down)')
pdf.table_row('fill_color', 'Color for exposed areas, -1 no fill, -2 wrap-around (default)')
pdf.body_text("Layer 0 sprites scroll with the background. Layers 1-4 remain stationary. Static objects also scroll.")

# Closing Sprites
pdf.section_title('Closing Sprites')

pdf.subsection_title('SPRITE CLOSE')
pdf.body_text("Closes a single sprite and frees its memory.")
pdf.code_block("SPRITE CLOSE #n")
pdf.body_text("Hides the sprite if displayed. Cannot close a sprite that has active copies.")

pdf.subsection_title('SPRITE CLOSE ALL')
pdf.body_text("Closes all sprites and static objects.")
pdf.code_block("SPRITE CLOSE ALL")

# Collision Detection
pdf.section_title('Collision Detection')

pdf.subsection_title('SPRITE INTERRUPT')
pdf.body_text("Sets the interrupt handler for sprite-to-sprite and edge collisions.")
pdf.code_block("SPRITE INTERRUPT label")
pdf.body_text("Called when a sprite collides with another sprite or screen edge.")

pdf.subsection_title('SPRITE NOINTERRUPT')
pdf.body_text("Disables the sprite collision interrupt.")
pdf.code_block("SPRITE NOINTERRUPT")

# Static Objects
pdf.add_page()
pdf.section_title('Static Objects')
pdf.body_text(
    "Static objects are invisible rectangular regions that trigger collisions when sprites intersect them. "
    "Useful for walls, platforms, obstacles, and trigger zones."
)

pdf.subsection_title('SPRITE STATIC')
pdf.body_text("Defines or removes a static object.")
pdf.code_block("SPRITE STATIC #n, x, y, width, height  ' Define\nSPRITE STATIC #n, OFF                   ' Remove")
pdf.table_row('Parameter', 'Description', header=True)
pdf.table_row('#n', 'Static object number (1-64)')
pdf.table_row('x, y', 'Position of the object')
pdf.table_row('width, height', 'Size of the object')
pdf.ln(2)

pdf.subsection_title('SPRITE STATIC CLEAR')
pdf.body_text("Removes all static objects.")
pdf.code_block("SPRITE STATIC CLEAR")

pdf.subsection_title('SPRITE STINTERRUPT')
pdf.body_text("Sets the interrupt handler for static object collisions.")
pdf.code_block("SPRITE STINTERRUPT label")
pdf.body_text("Called when a sprite collides with a static object. Use SPRITE(ST, COLLISION) and SPRITE(ST, OBJECT) to get details.")

pdf.subsection_title('SPRITE NOSTINTERRUPT')
pdf.body_text("Disables the static object collision interrupt.")
pdf.code_block("SPRITE NOSTINTERRUPT")

# Miscellaneous
pdf.section_title('Miscellaneous')

pdf.subsection_title('SPRITE SET TRANSPARENT')
pdf.body_text("Sets the transparent color for all sprites.")
pdf.code_block("SPRITE SET TRANSPARENT color")

# SPRITE() Function
pdf.add_page()
pdf.chapter_title('SPRITE() Function')
pdf.body_text("The SPRITE() function returns information about sprites, collisions, and background objects.")

pdf.section_title('Sprite Properties')

pdf.subsection_title('SPRITE(W, #n)')
pdf.body_text("Returns the width of sprite #n in pixels. Returns -1 if sprite not loaded.")

pdf.subsection_title('SPRITE(H, #n)')
pdf.body_text("Returns the height of sprite #n in pixels. Returns -1 if sprite not loaded.")

pdf.subsection_title('SPRITE(X, #n)')
pdf.body_text("Returns the X position of sprite #n. Returns 10000 if sprite not displayed.")

pdf.subsection_title('SPRITE(Y, #n)')
pdf.body_text("Returns the Y position of sprite #n. Returns 10000 if sprite not displayed.")

pdf.subsection_title('SPRITE(L, #n)')
pdf.body_text("Returns the layer of sprite #n. Returns -1 if sprite not displayed.")

pdf.subsection_title('SPRITE(A, #n)')
pdf.body_text("Returns the memory address of sprite #n's image data.")

pdf.section_title('Collision Information')

pdf.subsection_title('SPRITE(C, #n [, index])')
pdf.body_text("Returns collision information for sprite #n.\n\nWithout index: Returns the number of collisions detected.\nWith index: Returns the sprite number or edge code for that collision.")

pdf.body_text("Edge Collision Codes:")
pdf.table_row('Code', 'Meaning', header=True)
pdf.table_row('0xF1', 'Left edge')
pdf.table_row('0xF2', 'Top edge')
pdf.table_row('0xF4', 'Right edge')
pdf.table_row('0xF8', 'Bottom edge')
pdf.table_row('0x80-0xBF', 'Static object (object number = code AND 0x3F)')
pdf.ln(2)

pdf.subsection_title('SPRITE(T, #n)')
pdf.body_text("Returns the cumulative collision bitmask for sprite #n. Each bit represents a sprite that has collided.")

pdf.subsection_title('SPRITE(E, #n)')
pdf.body_text("Returns the edge collision flags for sprite #n. Bit 1=Left, 2=Top, 4=Right, 8=Bottom.")

pdf.subsection_title('SPRITE(S)')
pdf.body_text("Returns the sprite number that triggered the last collision interrupt.")

pdf.section_title('Distance and Direction')

pdf.subsection_title('SPRITE(V, #n1, #n2)')
pdf.body_text("Returns the angle (in radians) from sprite #n1 to sprite #n2, measured clockwise from north.")

pdf.subsection_title('SPRITE(D, #n1, #n2)')
pdf.body_text("Returns the distance in pixels between the centers of sprites #n1 and #n2.")

pdf.section_title('Background Collision Detection')

pdf.subsection_title('SPRITE(B, #n)')
pdf.body_text(
    "Performs pixel-level collision detection between the sprite and the background it is covering. "
    "This function compares the sprite's non-transparent pixels against the background stored when the sprite was displayed."
)
pdf.body_text("Returns:")
pdf.table_row('Value', 'Description', header=True)
pdf.table_row('0', 'No collision - sprite does not overlap non-transparent background')
pdf.table_row('1', 'Background overlap - sprite covers non-transparent background pixels')
pdf.table_row('2', 'Pixel collision - sprite non-transparent pixels overlap background')
pdf.ln(2)
pdf.body_text(
    "Notes: The sprite must be active (displayed) for this function to work. "
    "Uses the blitstoreptr buffer which contains the background captured when the sprite was shown. "
    "Updates the backgroundcollision[] array with detailed collision information."
)

pdf.subsection_title('SPRITE(B, #n, side)')
pdf.body_text("Returns detailed collision penetration information after calling SPRITE(B, #n).")
pdf.body_text("Bounding Box Collision (sides 0-3):")
pdf.table_row('Side', 'Description', header=True)
pdf.table_row('0', 'Right-most collision X offset from sprite left edge')
pdf.table_row('1', 'Left-most collision X offset from sprite right edge')
pdf.table_row('2', 'Bottom-most collision Y offset from sprite top edge')
pdf.table_row('3', 'Top-most collision Y offset from sprite bottom edge')
pdf.ln(2)
pdf.body_text("Pixel-Level Collision (sides 4-7):")
pdf.table_row('Side', 'Description', header=True)
pdf.table_row('4', 'Penetration depth from sprite left bound')
pdf.table_row('5', 'Penetration depth from sprite right bound')
pdf.table_row('6', 'Penetration depth from sprite top bound')
pdf.table_row('7', 'Penetration depth from sprite bottom bound')
pdf.ln(2)
pdf.body_text(
    "Notes: Only one of values 4 or 5 will be non-zero (indicates horizontal collision side). "
    "Only one of values 6 or 7 will be non-zero (indicates vertical collision side). "
    "Use these values to determine collision response direction (push-back)."
)

pdf.section_title('Count Information')

pdf.subsection_title('SPRITE(N)')
pdf.body_text("Returns the total number of sprites currently displayed.")

pdf.subsection_title('SPRITE(N, layer)')
pdf.body_text("Returns the number of sprites on the specified layer (0-4).")

# Static Object Properties
pdf.add_page()
pdf.section_title('Static Object Properties')

pdf.subsection_title('SPRITE(ST, #n, X)')
pdf.body_text("Returns the X position of static object #n. Returns -1 if not defined.")

pdf.subsection_title('SPRITE(ST, #n, Y)')
pdf.body_text("Returns the Y position of static object #n. Returns -1 if not defined.")

pdf.subsection_title('SPRITE(ST, #n, W)')
pdf.body_text("Returns the width of static object #n. Returns -1 if not defined.")

pdf.subsection_title('SPRITE(ST, #n, H)')
pdf.body_text("Returns the height of static object #n. Returns -1 if not defined.")

pdf.subsection_title('SPRITE(ST, #n, A)')
pdf.body_text("Returns 1 if static object #n is active, 0 otherwise.")

pdf.subsection_title('SPRITE(ST, COLLISION)')
pdf.body_text("Returns the sprite number that collided with a static object (set when STINTERRUPT fires).")

pdf.subsection_title('SPRITE(ST, OBJECT)')
pdf.body_text("Returns the static object number that was hit (set when STINTERRUPT fires).")

# Memory Usage
pdf.add_page()
pdf.chapter_title('Memory Usage')

pdf.body_text(
    "Understanding how sprites use memory helps you plan your program and avoid "
    "\"Not enough Heap memory\" errors."
)

pdf.section_title('How Much Memory Do Sprites Use?')
pdf.body_text(
    "Each sprite uses heap memory for three purposes:\n\n"
    "- Image data: ~256 bytes - The actual pixels of the sprite\n"
    "- Background buffer: (included above) - Saves what's behind the sprite\n"
    "- Bounds data: ~256 bytes - Used for pixel-perfect collision detection\n\n"
    "Rule of thumb: Each sprite uses approximately 500-600 bytes of heap memory, "
    "regardless of the sprite's pixel dimensions (for small to medium sprites up to about 20x20 pixels)."
)

pdf.section_title('Memory Usage Examples')
pdf.body_text(
    "- 10 sprites: ~5-6 KB\n"
    "- 32 sprites: ~16-19 KB\n"
    "- 64 sprites: ~35-40 KB"
)

pdf.section_title('Tips for Managing Memory')
pdf.body_text(
    "1. Check available memory before creating sprites:\n"
    "   Print \"Heap free: \"; MM.INFO(HEAP FREE)\n\n"
    "2. Close sprites you no longer need:\n"
    "   SPRITE CLOSE n        ' Close a specific sprite\n"
    "   SPRITE CLOSE ALL      ' Close all sprites and free their memory\n\n"
    "3. Use SPRITE COPY for multiple identical sprites:\n"
    "   When you need many copies of the same sprite image (like bullets or particles), "
    "copies share the original's image data, saving significant memory.\n\n"
    "4. Smaller sprites use the same minimum memory:\n"
    "   Due to memory allocation in 256-byte blocks, a 4×4 sprite uses the same memory "
    "as a 16×16 sprite. Consider this when designing your graphics.\n\n"
    "5. Leave headroom for your program:\n"
    "   Your BASIC program, arrays, and strings also use heap memory. "
    "Don't allocate all available memory to sprites."
)

pdf.section_title('Checking Memory During Development')
pdf.code_block(
    "Dim integer start_mem = MM.INFO(HEAP FREE)\n"
    "Print \"Before sprites: \"; start_mem\n\n"
    "' ... create your sprites ...\n\n"
    "Print \"After sprites: \"; MM.INFO(HEAP FREE)\n"
    "Print \"Sprites used: \"; start_mem - MM.INFO(HEAP FREE); \" bytes\""
)

# Technical Notes
pdf.add_page()
pdf.chapter_title('Technical Notes')

pdf.section_title('Performance Tips')
pdf.body_text(
    "1. Use SPRITE SHOW for non-overlapping sprites\n"
    "2. Use SPRITE SHOW SAFE when sprites may overlap\n"
    "3. Batch movements with SPRITE NEXT and SPRITE MOVE\n"
    "4. Use SPRITE SWAP for animation instead of loading new images\n"
    "5. Use SPRITE HIDE ALL before drawing to the background\n"
    "6. Minimize the number of active layers"
)

pdf.section_title('Limitations')
pdf.body_text(
    "- Maximum 64 sprite buffers\n"
    "- Maximum 64 static objects\n"
    "- Maximum 5 layers (0-4)\n"
    "- Maximum 4 simultaneous collision reports per sprite\n"
    "- Sprites must fit within screen resolution\n"
    "- Only available on VGA and HDMI displays or framebuffers. Not available on SPI, I2C or parallel connected displays"
)

# Example Programs
pdf.add_page()
pdf.chapter_title('Example Programs')

pdf.section_title('Basic Sprite Display')
pdf.code_block("' Load and display a sprite\nSPRITE LOAD \"player.spr\", 1\nSPRITE SHOW #1, 160, 120, 1")

pdf.section_title('Collision Detection')
pdf.code_block(
    "SPRITE LOAD \"player.spr\", 1\n"
    "SPRITE LOAD \"enemy.spr\", 2\n\n"
    "SPRITE INTERRUPT collision_handler\n"
    "SPRITE SHOW #1, 100, 100, 1\n"
    "SPRITE SHOW #2, 150, 100, 1\n\n"
    "DO\n"
    "    ' Game logic here\n"
    "LOOP\n\n"
    "collision_handler:\n"
    "    which = SPRITE(S)\n"
    "    PRINT \"Sprite\"; which; \"collided!\"\n"
    "    IRETURN"
)

pdf.section_title('Static Objects for Walls')
pdf.code_block(
    "' Create a simple room with walls\n"
    "SPRITE STATIC #1, 0, 0, 320, 10      ' Top wall\n"
    "SPRITE STATIC #2, 0, 230, 320, 10    ' Bottom wall\n"
    "SPRITE STATIC #3, 0, 0, 10, 240      ' Left wall\n"
    "SPRITE STATIC #4, 310, 0, 10, 240    ' Right wall\n\n"
    "SPRITE STINTERRUPT wall_hit\n"
    "SPRITE LOAD \"player.spr\", 1\n"
    "SPRITE SHOW #1, 160, 120, 1\n\n"
    "' ... game code ...\n\n"
    "wall_hit:\n"
    "    PRINT \"Hit wall\"; SPRITE(ST, OBJECT)\n"
    "    IRETURN"
)

pdf.section_title('Smooth Animation with SPRITE NEXT/MOVE')
pdf.code_block(
    "SPRITE LOAD \"anim.spr\", 1, 0\n"
    "FOR i = 2 TO 10\n"
    "    SPRITE COPY #1, #i, 1\n"
    "NEXT i\n\n"
    "' Display all sprites\n"
    "FOR i = 1 TO 10\n"
    "    SPRITE SHOW #i, i * 30, 100, 1\n"
    "NEXT i\n\n"
    "' Animate\n"
    "DO\n"
    "    FOR i = 1 TO 10\n"
    "        x = SPRITE(X, #i)\n"
    "        y = SPRITE(Y, #i)\n"
    "        SPRITE NEXT #i, x + 1, y + SIN(TIMER/100 + i) * 2\n"
    "    NEXT i\n"
    "    SPRITE MOVE\n"
    "    PAUSE 16\n"
    "LOOP"
)

# Save the PDF
pdf.output('SPRITE_User_Manual.pdf')
print("PDF generated: SPRITE_User_Manual.pdf")
