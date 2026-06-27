# SPRITE Command and Function Reference Manual

## Overview

The SPRITE system provides firmware-accelerated sprite graphics for the PicoMite. Sprites are rectangular graphic objects that can be moved independently over a background, with automatic collision detection and layer management.

**Key Features:**
- Up to 64 sprites (numbered 1-64)
- Up to 5 layers (0-4) for z-ordering
- Automatic sprite-to-sprite collision detection
- Edge-of-screen collision detection
- Static object collision detection
- Sprite rotation and mirroring
- Efficient batch movement operations
- Background scrolling with wrap-around

---

## SPRITE Commands

### Loading and Creating Sprites

#### SPRITE LOAD
Loads sprites from a sprite definition file.

**Syntax:**
```
SPRITE LOAD filename$ [, start_sprite] [, mode]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `filename$` | Path to the sprite file (`.spr` extension added if omitted) |
| `start_sprite` | First sprite buffer to load into (1-64, default: 1) |
| `mode` | Color palette mode: 0 = standard, 1 = alternate (default: 0) |

**Sprite File Format:**
The first line contains: `width, count [, height]`
- If height is omitted, sprites are assumed to be square (height = width)
- Each sprite is defined as height lines of width characters
- Characters 0-9 and A-F represent colors (hex values)
- Space character represents transparent pixels
- Lines starting with `'` (apostrophe) are comments

**Example:**
```basic
SPRITE LOAD "player.spr", 1, 0
```

---

#### SPRITE LOADARRAY
Loads a sprite from a numeric array.

**Syntax:**
```
SPRITE LOADARRAY #n, width, height, array()
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `width` | Sprite width in pixels |
| `height` | Sprite height in pixels |
| `array()` | Numeric array containing color values |

**Example:**
```basic
DIM sprite_data(99)
' ... fill array with color values ...
SPRITE LOADARRAY #1, 10, 10, sprite_data()
```

---

#### SPRITE LOADPNG (RP2350 only)
Loads a sprite from a PNG file.

**Syntax:**
```
SPRITE LOADPNG #n, filename$ [, transparent] [, cutoff]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `filename$` | Path to PNG file (`.png` extension added if omitted) |
| `transparent` | Transparent color index (0-15, default: 0) |
| `cutoff` | Alpha threshold for transparency (1-254, default: 30) |

**Note:** PNG must be in RGBA8888 format.

---

#### SPRITE LOADBMP
Loads a sprite from a BMP file.

**Syntax:**
```
SPRITE LOADBMP #n, filename$ [, x_offset, y_offset, width, height]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `filename$` | Path to BMP file (`.bmp` extension added if omitted) |
| `x_offset` | X offset within image (default: 0) |
| `y_offset` | Y offset within image (default: 0) |
| `width` | Width to load (default: full image) |
| `height` | Height to load (default: full image) |

---

#### SPRITE READ
Reads a sprite directly from the screen.

**Syntax:**
```
SPRITE READ #n, x, y, width, height
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `x, y` | Top-left corner of screen area to read |
| `width, height` | Size of area to capture |

**Example:**
```basic
SPRITE READ #5, 100, 100, 32, 32
```

---

#### SPRITE COPY
Creates copies of a sprite that share the same image data.

**Syntax:**
```
SPRITE COPY #source, #dest, count
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#source` | Source sprite buffer number |
| `#dest` | First destination sprite buffer number |
| `count` | Number of copies to create |

**Notes:**
- Copies share the same image data (memory efficient)
- Each copy has its own position and state
- Cannot copy a copy
- Must close all copies before closing the source

**Example:**
```basic
SPRITE LOAD "enemy.spr", 1
SPRITE COPY #1, #2, 5  ' Creates sprites #2, #3, #4, #5, #6
```

---

### Displaying Sprites

#### SPRITE SHOW
Displays a sprite at a specified position.

**Syntax:**
```
SPRITE SHOW #n, x, y, layer [, rotation]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `x, y` | Display position (can extend off-screen) |
| `layer` | Display layer (0-4) |
| `rotation` | Rotation/mirror value (0-7, default: 0) |

**Rotation Values:**
| Value | Effect |
|-------|--------|
| 0 | Normal |
| 1 | Mirror horizontal |
| 2 | Mirror vertical |
| 3 | Mirror both (180° rotation) |
| 4-7 | Same as 0-3 but with transparency disabled |

**Layer Behavior:**
- Layer 0: Scrolls with background (SPRITE SCROLL)
- Layers 1-4: Fixed position, rendered in order

**Example:**
```basic
SPRITE SHOW #1, 100, 50, 1, 0
```

---

#### SPRITE SHOW SAFE
Displays a sprite safely, properly handling overlapping sprites.

**Syntax:**
```
SPRITE SHOW SAFE #n, x, y, layer [, rotation] [, newbuffer]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `x, y` | Display position |
| `layer` | Display layer (0-4) |
| `rotation` | Rotation/mirror value (0-7, default: 0) |
| `newbuffer` | Force hide and re-show (0 or 1, default: 0) |

**Notes:**
- Use when moving sprites that may overlap with other sprites
- Automatically hides and redraws overlapping sprites
- Slower than SPRITE SHOW but produces correct results

---

#### SPRITE WRITE
Draws a sprite directly to the screen without tracking.

**Syntax:**
```
SPRITE WRITE #n, x, y [, rotation]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `x, y` | Draw position |
| `rotation` | Rotation/mirror value (0-7, default: 4) |

**Notes:**
- Does not store background or track position
- Does not participate in collision detection
- Use for static decorative elements

---

### Hiding Sprites

#### SPRITE HIDE
Hides a single sprite.

**Syntax:**
```
SPRITE HIDE #n
```

**Notes:**
- Restores the background where the sprite was displayed
- Sprite remains loaded and can be shown again
- Use SPRITE HIDE SAFE when other sprites overlap

---

#### SPRITE HIDE SAFE
Safely hides a sprite, handling overlapping sprites.

**Syntax:**
```
SPRITE HIDE SAFE #n
```

**Notes:**
- Properly redraws overlapping sprites
- Slower than SPRITE HIDE but produces correct results

---

#### SPRITE HIDE ALL
Hides all currently displayed sprites.

**Syntax:**
```
SPRITE HIDE ALL
```

**Notes:**
- Sprites remain loaded and can be restored
- Use before drawing to the background
- Must use SPRITE RESTORE to show sprites again

---

#### SPRITE RESTORE
Restores all sprites hidden by SPRITE HIDE ALL.

**Syntax:**
```
SPRITE RESTORE
```

---

### Moving Sprites

#### SPRITE NEXT
Sets the next position for a sprite (used with SPRITE MOVE).

**Syntax:**
```
SPRITE NEXT #n, x, y
```

**Notes:**
- Does not immediately move the sprite
- Position is applied when SPRITE MOVE is called
- Allows batching multiple sprite movements

**Example:**
```basic
SPRITE NEXT #1, 110, 50
SPRITE NEXT #2, 200, 100
SPRITE MOVE  ' Both sprites move simultaneously
```

---

#### SPRITE MOVE
Executes all pending SPRITE NEXT movements.

**Syntax:**
```
SPRITE MOVE
```

**Notes:**
- Moves all sprites to their NEXT positions in one operation
- More efficient than individual SPRITE SHOW commands
- Triggers collision detection after all moves complete

---

#### SPRITE SWAP
Swaps a displayed sprite with a hidden sprite of the same size.

**Syntax:**
```
SPRITE SWAP #displayed, #hidden [, rotation]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#displayed` | Currently displayed sprite |
| `#hidden` | Sprite to swap in (must be same size) |
| `rotation` | New rotation value (0-7, default: 0) |

**Notes:**
- Efficient sprite animation technique
- Both sprites must be the same size
- The displayed sprite becomes hidden, hidden becomes displayed

---

### Scrolling

#### SPRITE SCROLL
Scrolls the background and layer 0 sprites.

**Syntax:**
```
SPRITE SCROLL x, y [, fill_color]
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `x` | Horizontal scroll amount (positive = right) |
| `y` | Vertical scroll amount (positive = down) |
| `fill_color` | Color to fill exposed areas, or -1 for no fill, or -2 (default) for wrap-around |

**Notes:**
- Layer 0 sprites scroll with the background
- Layers 1-4 remain stationary
- Static objects also scroll with the background
- With wrap-around (-2), content wraps from one edge to the opposite

---

### Closing Sprites

#### SPRITE CLOSE
Closes a single sprite and frees its memory.

**Syntax:**
```
SPRITE CLOSE #n
```

**Notes:**
- Hides the sprite if displayed
- Cannot close a sprite that has active copies
- Frees sprite image and background storage memory

---

#### SPRITE CLOSE ALL
Closes all sprites and static objects.

**Syntax:**
```
SPRITE CLOSE ALL
```

---

### Collision Detection

#### SPRITE INTERRUPT
Sets the interrupt handler for sprite-to-sprite and edge collisions.

**Syntax:**
```
SPRITE INTERRUPT label
```

**Notes:**
- Called when a sprite collides with another sprite or screen edge
- Use SPRITE() functions to determine collision details

---

#### SPRITE NOINTERRUPT
Disables the sprite collision interrupt.

**Syntax:**
```
SPRITE NOINTERRUPT
```

---

### Static Objects

Static objects are invisible rectangular regions that trigger collisions when sprites intersect them. Useful for walls, platforms, obstacles, and trigger zones.

#### SPRITE STATIC
Defines or removes a static object.

**Syntax:**
```
SPRITE STATIC #n, x, y, width, height  ' Define
SPRITE STATIC #n, OFF                   ' Remove
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Static object number (1-64) |
| `x, y` | Position of the object |
| `width, height` | Size of the object |

**Example:**
```basic
' Define a wall
SPRITE STATIC #1, 100, 0, 20, 240

' Define a platform
SPRITE STATIC #2, 50, 180, 200, 20
```

---

#### SPRITE STATIC CLEAR
Removes all static objects.

**Syntax:**
```
SPRITE STATIC CLEAR
```

---

#### SPRITE STINTERRUPT
Sets the interrupt handler for static object collisions.

**Syntax:**
```
SPRITE STINTERRUPT label
```

**Notes:**
- Called when a sprite collides with a static object
- Use SPRITE(ST, COLLISION) and SPRITE(ST, OBJECT) to get details

---

#### SPRITE NOSTINTERRUPT
Disables the static object collision interrupt.

**Syntax:**
```
SPRITE NOSTINTERRUPT
```

---

### Miscellaneous

#### SPRITE SET TRANSPARENT
Sets the transparent color for all sprites.

**Syntax:**
```
SPRITE SET TRANSPARENT color
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `color` | Color index (0-15) to treat as transparent |

---

## SPRITE() Function

The SPRITE() function returns information about sprites, collisions, and static objects.

### Sprite Properties

#### SPRITE(W, #n)
Returns the width of sprite #n in pixels. Returns -1 if sprite not loaded.

#### SPRITE(H, #n)
Returns the height of sprite #n in pixels. Returns -1 if sprite not loaded.

#### SPRITE(X, #n)
Returns the X position of sprite #n. Returns 10000 if sprite not displayed.

#### SPRITE(Y, #n)
Returns the Y position of sprite #n. Returns 10000 if sprite not displayed.

#### SPRITE(L, #n)
Returns the layer of sprite #n. Returns -1 if sprite not displayed.

#### SPRITE(A, #n)
Returns the memory address of sprite #n's image data.

---

### Collision Information

#### SPRITE(C, #n [, index])
Returns collision information for sprite #n.

**Without index:** Returns the number of collisions detected.

**With index:** Returns the sprite number or edge code for collision `index`.

**Edge Collision Codes:**
| Code | Meaning |
|------|---------|
| 0xF1 | Left edge |
| 0xF2 | Top edge |
| 0xF4 | Right edge |
| 0xF8 | Bottom edge |
| 0x80-0xBF | Static object collision (object number = code AND 0x3F) |

**Example:**
```basic
num_collisions = SPRITE(C, #1)
FOR i = 1 TO num_collisions
    collision = SPRITE(C, #1, i)
    IF collision < 0x80 THEN
        PRINT "Collided with sprite"; collision
    ELSEIF collision >= 0xF0 THEN
        PRINT "Hit screen edge"
    ELSE
        PRINT "Hit static object"; collision AND &H3F
    ENDIF
NEXT
```

---

#### SPRITE(T, #n)
Returns the cumulative collision bitmask for sprite #n. Each bit represents a sprite that has collided (bit 0 = sprite 1, etc.).

#### SPRITE(E, #n)
Returns the edge collision flags for sprite #n.

| Bit | Edge |
|-----|------|
| 1 | Left edge |
| 2 | Top edge |
| 4 | Right edge |
| 8 | Bottom edge |

#### SPRITE(S)
Returns the sprite number that triggered the last collision interrupt.

---

### Distance and Direction

#### SPRITE(V, #n1, #n2)
Returns the angle (in radians) from sprite #n1 to sprite #n2, measured clockwise from north. Returns -1 if either sprite is not displayed.

#### SPRITE(D, #n1, #n2)
Returns the distance in pixels between the centers of sprites #n1 and #n2. Returns -1 if either sprite is not displayed.

---

### Background Collision Detection

#### SPRITE(B, #n)
Performs pixel-level collision detection between the sprite and the background it is covering. This function compares the sprite's non-transparent pixels against the background stored when the sprite was displayed.

**Returns:**
| Value | Description |
|-------|-------------|
| 0 | No collision - sprite's non-transparent pixels do not overlap any non-transparent background pixels |
| 1 | Background overlap detected - sprite covers non-transparent background pixels, but no pixel-level collision with sprite content |
| 2 | Pixel collision detected - the sprite's non-transparent pixels overlap with non-transparent background pixels |

**Notes:**
- The sprite must be active (displayed) for this function to work
- Uses the `blitstoreptr` buffer which contains the background captured when the sprite was shown
- Updates the `backgroundcollision[]` array with detailed collision information (accessible via `SPRITE(B, #n, side)`)

**Example:**
```basic
SPRITE SHOW #1, 100, 100, 1
' ... move sprite ...
collision_type = SPRITE(B, #1)
IF collision_type = 2 THEN
    PRINT "Pixel collision with background!"
ENDIF
```

---

#### SPRITE(B, #n, side)
Returns detailed collision penetration information after calling `SPRITE(B, #n)`.

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `#n` | Sprite buffer number (1-64) |
| `side` | Side index (0-7) |

**Side Values (Bounding Box Collision):**
| Side | Description |
|------|-------------|
| 0 | Right-most collision X offset from sprite left edge |
| 1 | Left-most collision X offset from sprite right edge |
| 2 | Bottom-most collision Y offset from sprite top edge |
| 3 | Top-most collision Y offset from sprite bottom edge |

**Side Values (Pixel-Level Collision):**
| Side | Description |
|------|-------------|
| 4 | Penetration depth from sprite's left bound |
| 5 | Penetration depth from sprite's right bound |
| 6 | Penetration depth from sprite's top bound |
| 7 | Penetration depth from sprite's bottom bound |

**Notes:**
- Values 4-7 use the pre-calculated sprite bounds (left/right/top/bottom edges of non-transparent pixels per row/column)
- Only one of values 4 or 5 will be non-zero (indicates which horizontal side has deeper penetration)
- Only one of values 6 or 7 will be non-zero (indicates which vertical side has deeper penetration)
- Use these values to determine collision response direction (push-back)

**Example:**
```basic
collision_type = SPRITE(B, #1)
IF collision_type = 2 THEN
    ' Check which side had the collision
    left_pen = SPRITE(B, #1, 4)
    right_pen = SPRITE(B, #1, 5)
    top_pen = SPRITE(B, #1, 6)
    bottom_pen = SPRITE(B, #1, 7)
    
    IF left_pen > 0 THEN
        ' Collision from the left - push sprite right
        new_x = SPRITE(X, #1) + left_pen
    ELSEIF right_pen > 0 THEN
        ' Collision from the right - push sprite left
        new_x = SPRITE(X, #1) - right_pen
    ENDIF
ENDIF
```

---

### Count Information

#### SPRITE(N)
Returns the total number of sprites currently displayed.

#### SPRITE(N, layer)
Returns the number of sprites on the specified layer (0-4).

---

### Static Object Properties

#### SPRITE(ST, #n, X)
Returns the X position of static object #n. Returns -1 if not defined.

#### SPRITE(ST, #n, Y)
Returns the Y position of static object #n. Returns -1 if not defined.

#### SPRITE(ST, #n, W)
Returns the width of static object #n. Returns -1 if not defined.

#### SPRITE(ST, #n, H)
Returns the height of static object #n. Returns -1 if not defined.

#### SPRITE(ST, #n, A)
Returns 1 if static object #n is active, 0 otherwise.

#### SPRITE(ST, COLLISION)
Returns the sprite number that collided with a static object (set when STINTERRUPT fires).

#### SPRITE(ST, OBJECT)
Returns the static object number that was hit (set when STINTERRUPT fires).

---

## Example Programs

### Basic Sprite Display
```basic
' Load and display a sprite
SPRITE LOAD "player.spr", 1
SPRITE SHOW #1, 160, 120, 1
```

### Collision Detection
```basic
SPRITE LOAD "player.spr", 1
SPRITE LOAD "enemy.spr", 2

SPRITE INTERRUPT collision_handler
SPRITE SHOW #1, 100, 100, 1
SPRITE SHOW #2, 150, 100, 1

DO
    ' Game logic here
LOOP

collision_handler:
    which = SPRITE(S)
    PRINT "Sprite"; which; "collided!"
    FOR i = 1 TO SPRITE(C, which)
        PRINT "  With:"; SPRITE(C, which, i)
    NEXT
    IRETURN
```

### Static Objects for Walls
```basic
' Create a simple room with walls
SPRITE STATIC #1, 0, 0, 320, 10      ' Top wall
SPRITE STATIC #2, 0, 230, 320, 10    ' Bottom wall
SPRITE STATIC #3, 0, 0, 10, 240      ' Left wall
SPRITE STATIC #4, 310, 0, 10, 240    ' Right wall

SPRITE STINTERRUPT wall_hit
SPRITE LOAD "player.spr", 1
SPRITE SHOW #1, 160, 120, 1

' ... game code ...

wall_hit:
    ' Player hit a wall - handle collision
    PRINT "Hit wall"; SPRITE(ST, OBJECT)
    IRETURN
```

### Smooth Animation with SPRITE NEXT/MOVE
```basic
SPRITE LOAD "anim.spr", 1, 0
FOR i = 2 TO 10
    SPRITE COPY #1, #i, 1
NEXT

' Display all sprites
FOR i = 1 TO 10
    SPRITE SHOW #i, i * 30, 100, 1
NEXT

' Animate
DO
    FOR i = 1 TO 10
        x = SPRITE(X, #i)
        y = SPRITE(Y, #i)
        SPRITE NEXT #i, x + 1, y + SIN(TIMER / 100 + i) * 2
    NEXT
    SPRITE MOVE
    PAUSE 16
LOOP
```

### Scrolling Background
```basic
SPRITE LOAD "player.spr", 1
SPRITE LOAD "tree.spr", 2

' Player on layer 1 (doesn't scroll)
SPRITE SHOW #1, 160, 120, 1

' Tree on layer 0 (scrolls with background)
SPRITE SHOW #2, 200, 150, 0

' Scroll the world
DO
    SPRITE SCROLL 1, 0  ' Scroll right
    PAUSE 16
LOOP
```

---

## Technical Notes

### Memory Usage
- Each sprite buffer uses: `(width * height + 1) / 2` bytes for image data
- Each sprite also requires the same amount for background storage
- Sprite copies share image data but have their own background storage

### Performance Tips
1. Use SPRITE SHOW for non-overlapping sprites
2. Use SPRITE SHOW SAFE when sprites may overlap
3. Batch movements with SPRITE NEXT and SPRITE MOVE
4. Use SPRITE SWAP for animation instead of loading new images
5. Use SPRITE HIDE ALL before drawing to the background
6. Minimize the number of active layers

### Limitations
- Maximum 64 sprite buffers
- Maximum 64 static objects
- Maximum 5 layers (0-4)
- Maximum 4 simultaneous collision reports per sprite
- Sprites must fit within screen resolution
- Only available on VGA and HDMI displays or framebuffers. Not available on SPI, I2C or parallel connected displays

---

## Complete Demo: Static Object and Sprite Collision Game

This comprehensive demo demonstrates both `SPRITE STINTERRUPT` (for static object collisions) and `SPRITE INTERRUPT` (for sprite-to-sprite collisions) working together.

**Game Features:**
- Player sprite controlled by arrow keys
- Red danger zones as static objects - lose a life if touched
- Yellow coins as sprites - collect for points
- Green goal zone as a static object - bonus points
- Separate interrupt handlers for each collision type

```basic
Option explicit
Option default none
Option console serial
Option base 1
MODE 2
FRAMEBUFFER create
FRAMEBUFFER write f
CLS

' Static Object Interrupt Demo
' Demonstrates both SPRITE STINTERRUPT and SPRITE INTERRUPT
' - Static objects for walls/goal (STINTERRUPT)
' - Sprite collisions for collectible coins (INTERRUPT)

Dim integer player_x = 20, player_y = 20
Dim integer player_sprite = 1
Dim integer score = 0, lives = 3
Dim integer last_st_hit = 0, sprite_hit = 0
Dim integer coin_collected = 0
Dim integer i
Dim string msg$ = ""

' Coin positions (center x, center y)
Dim integer coin_x(5) = (100, 220, 160, 100, 220)
Dim integer coin_y(5) = (75, 75, 145, 175, 175)

' Create player sprite (green square with white border)
Box 0, 0, 20, 20, 1, RGB(white), RGB(green)
Sprite read player_sprite, 0, 0, 20, 20
CLS

' Create coin sprite (yellow circle) - 15x15 with circle centered
Circle 7, 7, 6, 1, 1, RGB(yellow), RGB(yellow)
Sprite read 2, 0, 0, 15, 15
CLS

' Make copies of coin sprite for the other 4 coins
Sprite copy 2, 3, 4

' Draw the game area
Box 0, 0, MM.HRES, MM.VRES, 1, RGB(white)

' Draw visible obstacles (red danger zones)
Box 50, 50, 40, 40, 2, RGB(red), RGB(red)
Box 230, 50, 40, 40, 2, RGB(red), RGB(red)
Box 140, 100, 40, 40, 2, RGB(red), RGB(red)
Box 50, 150, 40, 40, 2, RGB(red), RGB(red)
Box 230, 150, 40, 40, 2, RGB(red), RGB(red)

' Draw safe zone (green goal)
Box 140, 200, 40, 30, 2, RGB(green), RGB(cyan)
Text 145, 208, "GOAL", L, 1, 1, RGB(black)

' Define static objects for the danger zones (1-5)
Sprite static 1, 50, 50, 40, 40    ' Danger 1
Sprite static 2, 230, 50, 40, 40   ' Danger 2
Sprite static 3, 140, 100, 40, 40  ' Danger 3
Sprite static 4, 50, 150, 40, 40   ' Danger 4
Sprite static 5, 230, 150, 40, 40  ' Danger 5

' Define static object for goal zone (6)
Sprite static 6, 140, 200, 40, 30  ' Goal

' Show the coin sprites (sprites 2-6) - center at coin_x, coin_y
For i = 1 To 5
  Sprite show i + 1, coin_x(i) - 7, coin_y(i) - 7, 1
Next i

' Show the player sprite
Sprite show player_sprite, player_x, player_y, 1

' Small pause to let collision state settle
Pause 50

' Set up interrupt handlers AFTER sprites are positioned
Sprite stinterrupt st_collision    ' For walls and goal
Sprite interrupt coin_collision    ' For coin collection

' Display instructions
Text 5, 5, "Arrows: move. Collect coins, avoid red!", L, 1, 1, RGB(white), RGB(black)
Text 5, 220, "Score: 0  Lives: 3", L, 1, 1, RGB(white), RGB(black)

FRAMEBUFFER copy f, n

' Main game loop
Dim string key$
Dim integer need_redraw = 0
Do
  key$ = Inkey$
  
  If key$ <> "" Then
    need_redraw = 1
    
    ' Move based on key press
    Select Case Asc(key$)
      Case 128 ' Up
        player_y = player_y - 5
        If player_y < 5 Then player_y = 5
      Case 129 ' Down
        player_y = player_y + 5
        If player_y > MM.VRES - 25 Then player_y = MM.VRES - 25
      Case 130 ' Left
        player_x = player_x - 5
        If player_x < 5 Then player_x = 5
      Case 131 ' Right
        player_x = player_x + 5
        If player_x > MM.HRES - 25 Then player_x = MM.HRES - 25
    End Select
  EndIf
  
  ' Hide collected coin sprite using SAFE to handle overlaps
  If coin_collected > 0 Then
    Sprite hide safe coin_collected + 1
    coin_collected = 0
    need_redraw = 1
  EndIf
  
  If need_redraw Then
    need_redraw = 0
    
    ' Hide sprite, update position, show sprite
    Sprite hide player_sprite
    Sprite show player_sprite, player_x, player_y, 1
    
    ' Update status display
    Box 5, 220, 310, 12, 0, RGB(black), RGB(black)
    Text 5, 220, "Score: " + Str$(score) + "  Lives: " + Str$(lives), L, 1, 1, RGB(white), RGB(black)
    
    ' Handle message display - show in status area, stays until next move
    If msg$ <> "" Then
      Text 200, 220, msg$, L, 1, 1, RGB(yellow), RGB(black)
      msg$ = ""
    EndIf
    
    FRAMEBUFFER copy f, n
  EndIf
  
  Pause 10
Loop Until lives <= 0 Or score >= 100

' Game over
Sprite hide player_sprite
Box 100, 100, 120, 40, 2, RGB(white), RGB(black)
If lives <= 0 Then
  Text 120, 115, "GAME OVER!", L, 1, 1, RGB(red), RGB(black)
Else
  Text 130, 115, "YOU WIN!", L, 1, 1, RGB(green), RGB(black)
EndIf
FRAMEBUFFER copy f, n

End

' Static object collision interrupt - handles walls and goal
' This is called when ANY sprite overlaps a static object
Sub st_collision
  sprite_hit = Sprite(ST, COLLISION)  ' Which sprite hit?
  last_st_hit = Sprite(ST, OBJECT)    ' Which ST object was hit?
  
  ' Only react if it was the player sprite that hit the ST object
  ' (Coin sprites may also overlap ST objects but we ignore those)
  If sprite_hit <> player_sprite Then Exit Sub
  
  If last_st_hit >= 1 And last_st_hit <= 5 Then
    ' Hit a danger zone - lose a life
    lives = lives - 1
    msg$ = "OUCH! -1 Life"
    ' Reset player position
    player_x = 20
    player_y = 20
    
  ElseIf last_st_hit = 6 Then
    ' Reached the goal!
    score = score + 50
    msg$ = "GOAL! +50!"
  EndIf
End Sub

' Sprite collision interrupt - handles coin collection
' This is called when sprites collide with each other
Sub coin_collision
  Local integer hit_sprite, col_count, c, other
  hit_sprite = Sprite(S)  ' Which sprite triggered the collision?
  
  ' Only process if player sprite triggered the collision
  If hit_sprite = player_sprite Then
    col_count = Sprite(C, player_sprite)  ' How many collisions?
    For c = 1 To col_count
      other = Sprite(C, player_sprite, c)  ' Get each colliding sprite
      ' Check if it's a coin sprite (2-6)
      If other >= 2 And other <= 6 Then
        score = score + 10
        msg$ = "+10 Points!"
        coin_collected = other - 1  ' Flag which coin to hide (1-5)
        Exit For
      EndIf
    Next c
  EndIf
End Sub
```

**Key Points Demonstrated:**

1. **Separate Interrupt Handlers**: `SPRITE STINTERRUPT` handles static object collisions (walls, goal), while `SPRITE INTERRUPT` handles sprite-to-sprite collisions (coins).

2. **Checking Which Sprite Collided**: In `st_collision`, we use `SPRITE(ST, COLLISION)` to determine which sprite hit the static object. This is important because ALL sprites are checked against static objects - not just the player.

3. **Using SPRITE HIDE SAFE**: When hiding coin sprites, we use `SPRITE HIDE SAFE` to properly redraw any overlapping sprites.

4. **Deferred Actions**: The interrupt handlers set flags (`coin_collected`, `player_x/y`) rather than directly manipulating sprites. The main loop performs the actual sprite operations to avoid conflicts.

5. **Static Objects for Game Elements**: Invisible rectangular zones define collision areas for walls and goals without needing visible sprites.

---

## Memory Usage

Understanding how sprites use memory helps you plan your program and avoid "Not enough Heap memory" errors.

### How Much Memory Do Sprites Use?

Each sprite uses heap memory for three purposes:

| Component | Typical Size | Description |
|-----------|--------------|-------------|
| Image data | 256 bytes | The actual pixels of the sprite |
| Background buffer | (included above) | Saves what's behind the sprite when displayed |
| Bounds data | 256 bytes | Used for pixel-perfect collision detection |

**Rule of thumb:** Each sprite uses approximately **500-600 bytes** of heap memory, regardless of the sprite's pixel dimensions (for small to medium sprites up to about 20×20 pixels).

### Memory Usage Examples

| Number of Sprites | Approximate Memory Used |
|-------------------|------------------------|
| 10 sprites | ~5-6 KB |
| 32 sprites | ~16-19 KB |
| 64 sprites | ~35-40 KB |

### Tips for Managing Memory

1. **Check available memory before creating sprites:**
   ```basic
   Print "Heap free: "; MM.INFO(HEAP FREE)
   ```

2. **Close sprites you no longer need:**
   ```basic
   SPRITE CLOSE n        ' Close a specific sprite
   SPRITE CLOSE ALL      ' Close all sprites and free their memory
   ```

3. **Use SPRITE COPY for multiple identical sprites:**
   When you need many copies of the same sprite image (like bullets or particles), use `SPRITE COPY`. Copies share the original's image data, saving significant memory:
   ```basic
   SPRITE READ 1, 0, 0, 16, 16    ' Create master sprite
   SPRITE COPY 1, 2, 10           ' Create 10 copies (sprites 2-11)
   ' The 10 copies use much less memory than 10 separate sprites
   ```

4. **Smaller sprites use the same minimum memory:**
   Due to memory allocation in 256-byte blocks, a 4×4 sprite uses the same memory as a 16×16 sprite. Consider this when designing your graphics.

5. **Leave headroom for your program:**
   Your BASIC program, arrays, and strings also use heap memory. Don't allocate all available memory to sprites.

### Checking Memory During Development

Add diagnostic prints while developing to understand your program's memory usage:

```basic
Dim integer start_mem = MM.INFO(HEAP FREE)
Print "Before sprites: "; start_mem

' ... create your sprites ...

Print "After sprites: "; MM.INFO(HEAP FREE)
Print "Sprites used: "; start_mem - MM.INFO(HEAP FREE); " bytes"
```

---

## Version History

| Version | Changes |
|---------|---------|
| 6.00.00 | Initial sprite system |
| 6.02.00 | Added static object collision detection |
