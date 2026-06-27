# MMBasic 3D Graphics User Manual

## Overview

The 3D graphics system in PicoMite MMBasic provides commands and functions for creating, manipulating, and displaying three-dimensional objects on a 2D screen. The system uses quaternion-based rotation and supports features like:

- Multiple 3D objects (up to 8 objects)
- Multiple cameras (up to 3 cameras)
- Face-based rendering with depth sorting
- Surface normal calculations for hidden face removal
- Lighting and ambient effects
- Face flags for customization

---

## 3D Command Reference

### 3D CREATE

Creates a new 3D object in memory.

**Syntax:**
```basic
3D CREATE n, nv, nf, camera, vertex(), facecount(), faces(), colours() [, linecolour()] [, fillcolour()]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n` | Object number (1 to 8) |
| `nv` | Number of vertices (minimum 3) |
| `nf` | Number of faces (minimum 1) |
| `camera` | Camera number to use for this object (1 to 3) |
| `vertex()` | 2D array of vertex coordinates (nv × 3: x, y, z for each vertex) |
| `facecount()` | 1D array containing the number of vertices for each face (minimum 3 per face) |
| `faces()` | 1D array listing vertex indices for each face (concatenated) |
| `colours()` | 1D array of color values used by the object |
| `linecolour()` | Optional: 1D array of color indices for face edges (one per face) |
| `fillcolour()` | Optional: 1D array of color indices for face fills (one per face) |

**Example:**
```basic
' Create a simple triangle (single face)
DIM vertex(2, 2) AS FLOAT
DIM facecount(0) AS INTEGER
DIM faces(2) AS INTEGER
DIM colours(1) AS INTEGER

' Define 3 vertices
vertex(0, 0) = 0 : vertex(0, 1) = 50 : vertex(0, 2) = 0   ' Top
vertex(1, 0) = -50 : vertex(1, 1) = -50 : vertex(1, 2) = 0 ' Bottom-left
vertex(2, 0) = 50 : vertex(2, 1) = -50 : vertex(2, 2) = 0  ' Bottom-right

' Define face with 3 vertices
facecount(0) = 3
faces(0) = 0 : faces(1) = 1 : faces(2) = 2

' Define colors
colours(0) = RGB(WHITE)
colours(1) = RGB(RED)

3D CAMERA 1, 500  ' Set up camera first
3D CREATE 1, 3, 1, 1, vertex(), facecount(), faces(), colours()
```

**Notes:**
- The object is stored in memory and must be explicitly closed when no longer needed.
- The camera must be configured before creating objects that reference it.
- Vertices are normalized to unit vectors internally, with magnitude stored separately.
- Centroids and surface normals are calculated automatically for each face.

---

### 3D CAMERA

Configures a camera for 3D projection.

**Syntax:**
```basic
3D CAMERA n, viewplane [, x] [, y] [, panx] [, pany]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n` | Camera number (1 to 3) |
| `viewplane` | Distance from camera to view plane (affects perspective) |
| `x` | Optional: Camera X position (default 0, range -32766 to 32766) |
| `y` | Optional: Camera Y position (default 0, range -32766 to 32766) |
| `panx` | Optional: Horizontal pan offset (default 0) |
| `pany` | Optional: Vertical pan offset (default 0) |

**Example:**
```basic
' Basic camera setup with default position
3D CAMERA 1, 500

' Camera with custom position and panning
3D CAMERA 1, 400, 100, 50, -20, 10
```

**Notes:**
- The camera must be configured before displaying any 3D objects that use it.
- A larger `viewplane` value reduces perspective distortion.
- The camera position affects how objects are projected onto the screen.

---

### 3D SHOW

Displays a 3D object on screen, clearing any previous display of that object.

**Syntax:**
```basic
3D SHOW n, x, y, z [, nonormals] [, depthmode]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n` | Object number (1 to 8) |
| `x` | X position in 3D space (range -32766 to 32766) |
| `y` | Y position in 3D space (range -32766 to 32766) |
| `z` | Z position in 3D space (distance from camera) |
| `nonormals` | Optional: 0 = use surface normals for hidden face removal (default), 1 = show all faces |
| `depthmode` | Optional: 0 = centroid-based depth sorting (default), 1 = vertex-based depth sorting, 2 = hidden-line wireframe (rp2350 only; closed solids, framebuffer/memory display only) |

**Example:**
```basic
' Display object at position (0, 0, 200)
3D SHOW 1, 0, 0, 200

' Display showing all faces (no hidden face removal)
3D SHOW 1, 0, 0, 200, 1

' Display with vertex-based depth sorting
3D SHOW 1, 0, 0, 200, 0, 1

' Wireframe with hidden-line removal (depth tested)
3D SHOW 1, 0, 0, 200, 0, 2
```

**Notes:**
- The previous position of the object is automatically cleared before redrawing.
- The bounding box of the object is tracked for efficient clearing.
- Hidden face removal uses the dot product of the surface normal and the camera ray.
- `depthmode=2` is intended for closed solid meshes rendered as wireframe.
- `depthmode=2` is available on `rp2350` builds only.
- `depthmode=2` requires a framebuffer/memory-mapped display target (for example VGA/HDMI frame memory).
- In hidden-line mode, wireframe edges are depth-tested against visible faces.
- To use hidden-line wireframe, create wireframe faces (`fillcolour` omitted in `3D CREATE`, or face fill set to no-fill).
- Ensure edge colours are visible against the background (for example, avoid black edges on a black background).
- Hidden-line mode is slower than solid rendering because it builds a depth buffer before drawing edges.

---

### 3D WRITE

Displays a 3D object on screen without clearing the previous display.

**Syntax:**
```basic
3D WRITE n, x, y, z [, nonormals] [, depthmode]
```

**Parameters:**
Same as `3D SHOW`.

**Example:**
```basic
' Draw object without clearing (useful for layering)
3D WRITE 1, 0, 0, 200
```

**Notes:**
- Use this when you want to overlay multiple objects or positions.
- Does not update the bounding box for automatic clearing.
- `depthmode=2` is supported for `3D WRITE` on `rp2350` builds, with the same constraints as `3D SHOW`.

---

### 3D ROTATE

Rotates one or more 3D objects using a quaternion.

**Syntax:**
```basic
3D ROTATE quaternion(), n1 [, n2, n3, ...]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `quaternion()` | 1D array of 5 elements: w, x, y, z, m (quaternion components + magnitude) |
| `n1, n2, ...` | Object numbers to rotate (1 to 8) |

**Example:**
```basic
DIM quat(4) AS FLOAT

' Create a rotation quaternion for 10 degrees around Y axis
angle = RAD(10)
quat(0) = COS(angle/2)  ' w
quat(1) = 0             ' x
quat(2) = SIN(angle/2)  ' y
quat(3) = 0             ' z
quat(4) = 1             ' magnitude

' Apply rotation to object 1
3D ROTATE quat(), 1

' Apply rotation to multiple objects
3D ROTATE quat(), 1, 2, 3
```

**Notes:**
- Quaternion rotation avoids gimbal lock that can occur with Euler angles.
- The rotation is applied to the object's internal rotated vertices, allowing cumulative rotations.
- To reset to original orientation, use `3D RESET`.

---

### 3D RESET

Resets one or more 3D objects to their original (pre-rotation) state.

**Syntax:**
```basic
3D RESET n1 [, n2, n3, ...]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n1, n2, ...` | Object numbers to reset (1 to 8) |

**Example:**
```basic
' Reset single object
3D RESET 1

' Reset multiple objects
3D RESET 1, 2, 3
```

---

### 3D LIGHT

Sets the light source position and ambient lighting level for an object.

**Syntax:**
```basic
3D LIGHT n, x, y, z, ambient
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n` | Object number (1 to 8) |
| `x` | Light source X position (range -32766 to 32766) |
| `y` | Light source Y position (range -32766 to 32766) |
| `z` | Light source Z position (range -32766 to 32766) |
| `ambient` | Ambient light level (0 to 100, as percentage) |

**Example:**
```basic
' Set light source above and to the left, with 30% ambient
3D LIGHT 1, -100, 100, -50, 30
```

**Notes:**
- Lighting affects face colors based on the angle between the face normal and light direction.
- The face must have flag bit 3 set (value 8) for lighting to be applied.
- Ambient light provides a base illumination level regardless of light direction.

---

### 3D SET FLAGS

Sets rendering flags for specific faces of a 3D object.

**Syntax:**
```basic
3D SET FLAGS n, flag, face1, count1 [, face2, count2, ...]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n` | Object number (1 to 8) |
| `flag` | Flag value (0 to 255) |
| `face` | Starting face index |
| `count` | Number of faces to set |

**Flag Bits:**

| Bit | Value | Description |
|-----|-------|-------------|
| 0 | 1 | Hide face (don't draw) |
| 1 | 2 | Debug: draw face in red |
| 2 | 4 | Invert normal (useful for inside-out faces) |
| 3 | 8 | Apply lighting calculations |

**Example:**
```basic
' Enable lighting for all 6 faces of a cube
3D SET FLAGS 1, 8, 0, 6

' Hide faces 2 and 3
3D SET FLAGS 1, 1, 2, 2

' Combine flags: lighting + inverted normal
3D SET FLAGS 1, 12, 0, 1  ' 12 = 8 + 4
```

---

### 3D HIDE

Hides one or more 3D objects by clearing their display area.

**Syntax:**
```basic
3D HIDE n1 [, n2, n3, ...]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n1, n2, ...` | Object numbers to hide (1 to 8) |

**Example:**
```basic
3D HIDE 1
3D HIDE 1, 2, 3
```

---

### 3D HIDE ALL

Hides all currently displayed 3D objects.

**Syntax:**
```basic
3D HIDE ALL
```

---

### 3D RESTORE

Restores (redraws) previously hidden 3D objects at their last displayed position.

**Syntax:**
```basic
3D RESTORE n1 [, n2, n3, ...]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n1, n2, ...` | Object numbers to restore (1 to 8) |

**Example:**
```basic
3D HIDE 1
' ... do something else ...
3D RESTORE 1  ' Redraws at same position
```

**Notes:**
- The object must have been previously displayed with `3D SHOW`.
- An error occurs if the object is not currently hidden.

---

### 3D DIAGNOSE

Outputs diagnostic information about a 3D object's faces.

**Syntax:**
```basic
3D DIAGNOSE n, x, y, z [, sort]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n` | Object number (1 to 8) |
| `x` | X position in 3D space |
| `y` | Y position in 3D space |
| `z` | Z position in 3D space |
| `sort` | Optional: 0 = unsorted order, 1 = sorted by depth (default) |

**Output:**
For each face, prints:
- Face number
- Distance from camera
- Dot product (surface normal vs camera ray)
- Whether face is "Hidden" or "Showing"

**Example:**
```basic
3D DIAGNOSE 1, 0, 0, 200
' Output:
' Face 0 at distance 210.5 dot product is -0.85 so the face is Showing
' Face 1 at distance 195.2 dot product is 0.42 so the face is Hidden
' ...
```

---

### 3D CLOSE

Closes one or more 3D objects and frees their memory.

**Syntax:**
```basic
3D CLOSE n1 [, n2, n3, ...]
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `n1, n2, ...` | Object numbers to close (1 to 8) |

**Example:**
```basic
3D CLOSE 1
3D CLOSE 1, 2, 3
```

---

### 3D CLOSE ALL

Closes all 3D objects and frees all associated memory.

**Syntax:**
```basic
3D CLOSE ALL
```

---

## 3D Function Reference

### 3D(XMIN n)

Returns the minimum X coordinate of the bounding box for the last display of object `n`.

**Syntax:**
```basic
x = 3D(XMIN n)
```

---

### 3D(XMAX n)

Returns the maximum X coordinate of the bounding box for the last display of object `n`.

**Syntax:**
```basic
x = 3D(XMAX n)
```

---

### 3D(YMIN n)

Returns the minimum Y coordinate of the bounding box for the last display of object `n`.

**Syntax:**
```basic
y = 3D(YMIN n)
```

---

### 3D(YMAX n)

Returns the maximum Y coordinate of the bounding box for the last display of object `n`.

**Syntax:**
```basic
y = 3D(YMAX n)
```

---

### 3D(X n)

Returns the X position where object `n` was last displayed.

**Syntax:**
```basic
x = 3D(X n)
```

---

### 3D(Y n)

Returns the Y position where object `n` was last displayed.

**Syntax:**
```basic
y = 3D(Y n)
```

---

### 3D(Z n)

Returns the Z position where object `n` was last displayed.

**Syntax:**
```basic
z = 3D(Z n)
```

---

### 3D(DISTANCE n)

Returns the average distance from the camera to all faces of object `n`.

**Syntax:**
```basic
d = 3D(DISTANCE n)
```

**Notes:**
- This is useful for determining relative object positions for collision detection or layering.
- The value is calculated as the mean of all face centroid distances.

---

## Complete Example: Rotating Cube

```basic
' 3D Rotating Cube Example
OPTION EXPLICIT

DIM vertex(7, 2) AS FLOAT      ' 8 vertices, 3 coords each
DIM facecount(5) AS INTEGER    ' 6 faces
DIM faces(23) AS INTEGER       ' 6 faces x 4 vertices
DIM colours(6) AS INTEGER      ' Color palette
DIM linecolour(5) AS INTEGER   ' Edge colors per face
DIM fillcolour(5) AS INTEGER   ' Fill colors per face
DIM quat(4) AS FLOAT           ' Rotation quaternion

' Define cube vertices (size 50)
DATA -25,-25,-25, 25,-25,-25, 25,25,-25, -25,25,-25
DATA -25,-25,25, 25,-25,25, 25,25,25, -25,25,25

RESTORE
FOR i = 0 TO 7
  READ vertex(i, 0), vertex(i, 1), vertex(i, 2)
NEXT i

' Each face has 4 vertices
FOR i = 0 TO 5 : facecount(i) = 4 : NEXT i

' Face vertex indices (counter-clockwise when viewed from outside)
DATA 0,1,2,3, 5,4,7,6, 0,4,5,1, 2,6,7,3, 0,3,7,4, 1,5,6,2

RESTORE
FOR i = 0 TO 23 : READ faces(i) : NEXT i

' Define colors
colours(0) = RGB(WHITE)  ' Edge color
colours(1) = RGB(RED)
colours(2) = RGB(GREEN)
colours(3) = RGB(BLUE)
colours(4) = RGB(YELLOW)
colours(5) = RGB(CYAN)
colours(6) = RGB(MAGENTA)

' Set edge color (white) and fill colors for each face
FOR i = 0 TO 5
  linecolour(i) = 0      ' Index into colours() for white
  fillcolour(i) = i + 1  ' Different color for each face
NEXT i

' Set up camera
3D CAMERA 1, 400

' Create the cube
3D CREATE 1, 8, 6, 1, vertex(), facecount(), faces(), colours(), linecolour(), fillcolour()

' Enable lighting
3D LIGHT 1, -100, 100, -50, 40
3D SET FLAGS 1, 8, 0, 6  ' Enable lighting for all faces

' Animation loop
angle = 0
DO
  ' Create rotation quaternion (rotate around Y and X axes)
  ax = RAD(angle * 0.7)
  ay = RAD(angle)
  
  ' Combined rotation quaternion
  quat(0) = COS(ax/2) * COS(ay/2)
  quat(1) = SIN(ax/2) * COS(ay/2)
  quat(2) = COS(ax/2) * SIN(ay/2)
  quat(3) = SIN(ax/2) * SIN(ay/2)
  quat(4) = 1
  
  ' Reset and apply rotation
  3D RESET 1
  3D ROTATE quat(), 1
  
  ' Display
  3D SHOW 1, 0, 0, 150
  
  PAUSE 20
  angle = angle + 2
  IF angle >= 360 THEN angle = 0
LOOP UNTIL INKEY$ <> ""

3D CLOSE ALL
END
```

---

## Technical Notes

### Coordinate System
- X axis: Positive to the right
- Y axis: Positive upward
- Z axis: Positive toward the camera (away from screen)

### Quaternion Format
The quaternion array has 5 elements:
- `q(0)` = w (scalar/real component)
- `q(1)` = x (i component)
- `q(2)` = y (j component)
- `q(3)` = z (k component)
- `q(4)` = m (magnitude, typically 1.0)

### Hidden Face Removal
Faces are hidden when the dot product of the surface normal and the camera ray is positive, meaning the face is pointing away from the camera.

### Depth Sorting
Two modes are available:
- **Centroid mode (0)**: Uses the distance from camera to face centroid
- **Vertex mode (1)**: Uses the maximum distance from camera to any vertex

### Memory Management
- Each 3D object allocates memory for vertices, faces, colors, normals, and other data.
- Always use `3D CLOSE` or `3D CLOSE ALL` when finished to free memory.
- Maximum 8 simultaneous objects and 3 cameras.

---

## Error Messages

| Error | Cause |
|-------|-------|
| "Object already exists" | Attempting to create an object with an ID that's already in use |
| "3D object must have a minimum of 3 vertices" | Creating object with fewer than 3 vertices |
| "3D object must have a minimum of 1 face" | Creating object with no faces |
| "Vertex count less than 3 for face" | A face is defined with fewer than 3 vertices |
| "Camera position not defined" | Trying to display object before setting up its camera |
| "Object % is not hidden" | Trying to restore an object that isn't hidden |
| "Edge colour Index %" | Color index out of range in linecolour array |
| "Fill colour Index %" | Color index out of range in fillcolour array |

---

## See Also

- `POLYGON` - For 2D polygon drawing
- `LINE` - For 2D line drawing
- `MATH` commands - For trigonometric functions used in rotation calculations
