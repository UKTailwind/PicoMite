from fpdf import FPDF

class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'MMBasic DRAW3D Graphics User Manual', 0, 1, 'C')
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

    def chapter_body(self, body):
        self.set_font('Helvetica', '', 10)
        self.multi_cell(0, 5, body)
        self.ln()

    def code_block(self, code):
        self.set_font('Courier', '', 9)
        self.set_fill_color(240, 240, 240)
        self.multi_cell(0, 4, code, fill=True)
        self.ln()

    def param_table(self, params):
        self.set_font('Helvetica', 'B', 9)
        self.cell(35, 6, 'Parameter', 1, 0, 'C')
        self.cell(0, 6, 'Description', 1, 1, 'C')
        self.set_font('Helvetica', '', 9)
        for param, desc in params:
            self.cell(35, 6, param, 1, 0, 'L')
            self.cell(0, 6, desc, 1, 1, 'L')
        self.ln()

pdf = PDF()
pdf.add_page()

# Overview
pdf.chapter_title('Overview')
pdf.chapter_body(
    "The DRAW3D graphics system in PicoMite MMBasic provides commands and functions for creating, "
    "manipulating, and displaying three-dimensional objects on a 2D screen. The system uses "
    "quaternion-based rotation and supports features like:\n\n"
    "- Multiple 3D objects (up to 8 objects)\n"
    "- Multiple cameras (up to 3 cameras)\n"
    "- Face-based rendering with depth sorting\n"
    "- Surface normal calculations for hidden face removal\n"
    "- Lighting and ambient effects\n"
    "- Face flags for customization"
)

# DRAW3D CREATE
pdf.chapter_title('DRAW3D CREATE')
pdf.chapter_body("Creates a new 3D object in memory.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D CREATE n, nv, nf, camera, vertex(), facecount(), faces(),\n              colours() [, linecolour()] [, fillcolour()]")
pdf.section_title('Parameters')
pdf.param_table([
    ('n', 'Object number (1 to 8)'),
    ('nv', 'Number of vertices (minimum 3)'),
    ('nf', 'Number of faces (minimum 1)'),
    ('camera', 'Camera number to use for this object (1 to 3)'),
    ('vertex()', '2D array of vertex coordinates (nv x 3: x, y, z)'),
    ('facecount()', '1D array: number of vertices for each face'),
    ('faces()', '1D array listing vertex indices for each face'),
    ('colours()', '1D array of color values used by the object'),
    ('linecolour()', 'Optional: 1D array of color indices for edges'),
    ('fillcolour()', 'Optional: 1D array of color indices for fills'),
])
pdf.chapter_body(
    "Notes:\n"
    "- The object is stored in memory and must be closed when no longer needed.\n"
    "- The camera must be configured before creating objects that reference it.\n"
    "- Vertices are normalized to unit vectors internally, with magnitude stored separately.\n"
    "- Centroids and surface normals are calculated automatically for each face."
)

# DRAW3D CAMERA
pdf.chapter_title('DRAW3D CAMERA')
pdf.chapter_body("Configures a camera for 3D projection.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D CAMERA n, viewplane [, x] [, y] [, panx] [, pany]")
pdf.section_title('Parameters')
pdf.param_table([
    ('n', 'Camera number (1 to 3)'),
    ('viewplane', 'Distance from camera to view plane (affects perspective)'),
    ('x', 'Optional: Camera X position (default 0, range -32766 to 32766)'),
    ('y', 'Optional: Camera Y position (default 0, range -32766 to 32766)'),
    ('panx', 'Optional: Horizontal pan offset (default 0)'),
    ('pany', 'Optional: Vertical pan offset (default 0)'),
])
pdf.section_title('Example')
pdf.code_block("DRAW3D CAMERA 1, 500\nDRAW3D CAMERA 1, 400, 100, 50, -20, 10")

# DRAW3D SHOW
pdf.chapter_title('DRAW3D SHOW')
pdf.chapter_body("Displays a 3D object on screen, clearing any previous display of that object.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D SHOW n, x, y, z [, nonormals] [, depthmode]")
pdf.section_title('Parameters')
pdf.param_table([
    ('n', 'Object number (1 to 8)'),
    ('x', 'X position in 3D space (range -32766 to 32766)'),
    ('y', 'Y position in 3D space (range -32766 to 32766)'),
    ('z', 'Z position in 3D space (distance from camera)'),
    ('nonormals', 'Optional: 0 = hidden face removal (default), 1 = show all'),
    ('depthmode', 'Optional: 0 = centroid depth, 1 = vertex depth, 2 = hidden-line wireframe (rp2350 only)'),
])
pdf.section_title('Example')
pdf.code_block(
    "DRAW3D SHOW 1, 0, 0, 200\n"
    "DRAW3D SHOW 1, 0, 0, 200, 1     ' Show all faces\n"
    "DRAW3D SHOW 1, 0, 0, 200, 0, 1  ' Vertex-based depth\n"
    "DRAW3D SHOW 1, 0, 0, 200, 0, 2  ' Hidden-line wireframe"
)
pdf.chapter_body(
    "Notes:\n"
    "- depthmode=2 is for closed solid meshes rendered as wireframe.\n"
    "- depthmode=2 is available on rp2350 builds only.\n"
    "- depthmode=2 requires a framebuffer or memory-mapped display target.\n"
    "- Use no-fill faces (wireframe) for hidden-line edge rendering.\n"
    "- Hidden-line mode is slower than standard solid rendering."
)

# DRAW3D WRITE
pdf.chapter_title('DRAW3D WRITE')
pdf.chapter_body("Displays a 3D object on screen without clearing the previous display. Same parameters as DRAW3D SHOW.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D WRITE n, x, y, z [, nonormals] [, depthmode]")
pdf.chapter_body("Note: depthmode=2 hidden-line wireframe is supported on rp2350 builds, with the same constraints as DRAW3D SHOW.")

# DRAW3D ROTATE
pdf.add_page()
pdf.chapter_title('DRAW3D ROTATE')
pdf.chapter_body("Rotates one or more 3D objects using a quaternion.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D ROTATE quaternion(), n1 [, n2, n3, ...]")
pdf.section_title('Parameters')
pdf.param_table([
    ('quaternion()', '1D array of 5 elements: w, x, y, z, m'),
    ('n1, n2, ...', 'Object numbers to rotate (1 to 8)'),
])
pdf.section_title('Example')
pdf.code_block(
    "Dim float quat(4)\n"
    "' Use MATH Q_CREATE to build rotation quaternion\n"
    "Math q_create Rad(10), 0, 1, 0, quat()  ' 10 deg around Y\n"
    "DRAW3D ROTATE quat(), 1"
)
pdf.chapter_body(
    "Notes:\n"
    "- Quaternion rotation avoids gimbal lock.\n"
    "- Rotation is cumulative; use DRAW3D RESET to restore original orientation.\n"
    "- Use MATH Q_CREATE angle, x, y, z, q() to create quaternions easily."
)

# DRAW3D RESET
pdf.chapter_title('DRAW3D RESET')
pdf.chapter_body("Resets one or more 3D objects to their original (pre-rotation) state.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D RESET n1 [, n2, n3, ...]")

# DRAW3D LIGHT
pdf.chapter_title('DRAW3D LIGHT')
pdf.chapter_body("Sets the light source position and ambient lighting level for an object.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D LIGHT n, x, y, z, ambient")
pdf.section_title('Parameters')
pdf.param_table([
    ('n', 'Object number (1 to 8)'),
    ('x', 'Light source X position (-32766 to 32766)'),
    ('y', 'Light source Y position (-32766 to 32766)'),
    ('z', 'Light source Z position (-32766 to 32766)'),
    ('ambient', 'Ambient light level (0 to 100, as percentage)'),
])
pdf.section_title('Example')
pdf.code_block("DRAW3D LIGHT 1, -100, 100, -50, 30")
pdf.chapter_body("Note: The face must have flag bit 3 set (value 8) for lighting to be applied.")

# DRAW3D SET FLAGS
pdf.chapter_title('DRAW3D SET FLAGS')
pdf.chapter_body("Sets rendering flags for specific faces of a 3D object.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D SET FLAGS n, flag, face1, count1 [, face2, count2, ...]")
pdf.section_title('Parameters')
pdf.param_table([
    ('n', 'Object number (1 to 8)'),
    ('flag', 'Flag value (0 to 255)'),
    ('face', 'Starting face index'),
    ('count', 'Number of faces to set'),
])
pdf.section_title('Flag Bits')
pdf.set_font('Helvetica', 'B', 9)
pdf.cell(20, 6, 'Bit', 1, 0, 'C')
pdf.cell(20, 6, 'Value', 1, 0, 'C')
pdf.cell(0, 6, 'Description', 1, 1, 'C')
pdf.set_font('Helvetica', '', 9)
flags = [
    ('0', '1', 'Hide face (don\'t draw)'),
    ('1', '2', 'Debug: draw face in red'),
    ('2', '4', 'Invert normal (for inside-out faces)'),
    ('3', '8', 'Apply lighting calculations'),
]
for bit, val, desc in flags:
    pdf.cell(20, 6, bit, 1, 0, 'C')
    pdf.cell(20, 6, val, 1, 0, 'C')
    pdf.cell(0, 6, desc, 1, 1, 'L')
pdf.ln()
pdf.section_title('Example')
pdf.code_block("DRAW3D SET FLAGS 1, 8, 0, 6   ' Enable lighting for all 6 faces")

# DRAW3D HIDE commands
pdf.add_page()
pdf.chapter_title('DRAW3D HIDE')
pdf.chapter_body("Hides one or more 3D objects by clearing their display area.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D HIDE n1 [, n2, n3, ...]")

pdf.chapter_title('DRAW3D HIDE ALL')
pdf.chapter_body("Hides all currently displayed 3D objects.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D HIDE ALL")

# DRAW3D RESTORE
pdf.chapter_title('DRAW3D RESTORE')
pdf.chapter_body("Restores (redraws) previously hidden 3D objects at their last displayed position.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D RESTORE n1 [, n2, n3, ...]")
pdf.chapter_body("Note: An error occurs if the object is not currently hidden.")

# DRAW3D DIAGNOSE
pdf.chapter_title('DRAW3D DIAGNOSE')
pdf.chapter_body("Outputs diagnostic information about a 3D object's faces.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D DIAGNOSE n, x, y, z [, sort]")
pdf.section_title('Parameters')
pdf.param_table([
    ('n', 'Object number (1 to 8)'),
    ('x', 'X position in 3D space'),
    ('y', 'Y position in 3D space'),
    ('z', 'Z position in 3D space'),
    ('sort', 'Optional: 0 = unsorted, 1 = sorted by depth (default)'),
])
pdf.chapter_body(
    "Output for each face:\n"
    "- Face number and distance from camera\n"
    "- Dot product (surface normal vs camera ray)\n"
    "- Whether face is \"Hidden\" or \"Showing\""
)

# DRAW3D CLOSE commands
pdf.chapter_title('DRAW3D CLOSE')
pdf.chapter_body("Closes one or more 3D objects and frees their memory.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D CLOSE n1 [, n2, n3, ...]")

pdf.chapter_title('DRAW3D CLOSE ALL')
pdf.chapter_body("Closes all 3D objects and frees all associated memory.")
pdf.section_title('Syntax')
pdf.code_block("DRAW3D CLOSE ALL")

# Functions
pdf.add_page()
pdf.chapter_title('DRAW3D( Functions')
pdf.chapter_body("Functions to query 3D object properties. All return numeric values.")

pdf.section_title('Bounding Box Functions')
pdf.code_block(
    "x = DRAW3D(XMIN n)   ' Minimum X of bounding box\n"
    "x = DRAW3D(XMAX n)   ' Maximum X of bounding box\n"
    "y = DRAW3D(YMIN n)   ' Minimum Y of bounding box\n"
    "y = DRAW3D(YMAX n)   ' Maximum Y of bounding box"
)

pdf.section_title('Position Functions')
pdf.code_block(
    "x = DRAW3D(X n)      ' Last displayed X position\n"
    "y = DRAW3D(Y n)      ' Last displayed Y position\n"
    "z = DRAW3D(Z n)      ' Last displayed Z position"
)

pdf.section_title('Distance Function')
pdf.code_block("d = DRAW3D(DISTANCE n)  ' Average distance from camera to all faces")
pdf.chapter_body("Useful for collision detection or determining relative object positions.")

# Technical Notes
pdf.add_page()
pdf.chapter_title('Technical Notes')

pdf.section_title('Coordinate System')
pdf.chapter_body(
    "- X axis: Positive to the right\n"
    "- Y axis: Positive upward\n"
    "- Z axis: Positive toward the camera (away from screen)"
)

pdf.section_title('Quaternion Format')
pdf.chapter_body(
    "The quaternion array has 5 elements:\n"
    "- q(0) = w (scalar/real component)\n"
    "- q(1) = x (i component)\n"
    "- q(2) = y (j component)\n"
    "- q(3) = z (k component)\n"
    "- q(4) = m (magnitude, typically 1.0)\n\n"
    "Use MATH Q_CREATE angle, ax, ay, az, q() to create a quaternion for rotation "
    "of 'angle' radians around the axis (ax, ay, az)."
)

pdf.section_title('Vertex Ordering and Surface Normals')
pdf.chapter_body(
    "CRITICAL: The order in which vertices are listed for each face determines the direction "
    "of the surface normal, which is essential for correct rendering.\n\n"
    "Surface Normal Calculation:\n"
    "The surface normal is calculated using the cross product of two edge vectors. For a face "
    "with vertices V0, V1, V2, the normal is computed as: (V1-V0) x (V2-V0). This follows the "
    "right-hand rule - if you curl your fingers from the first edge to the second, your thumb "
    "points in the normal direction.\n\n"
    "Counter-Clockwise Winding:\n"
    "When vertices are listed in counter-clockwise order (as viewed from outside the object), "
    "the surface normal points outward. This is the standard convention and is required for "
    "correct hidden face removal.\n\n"
    "Why This Matters:\n"
    "1. HIDDEN FACE REMOVAL: The renderer compares the surface normal to the camera direction. "
    "If the normal points away from the camera (dot product > 0), the face is back-facing and "
    "hidden. Wrong vertex order = wrong normal = faces appear/disappear incorrectly.\n\n"
    "2. LIGHTING: Light calculations use the surface normal to determine brightness. Wrong "
    "normals cause faces to be lit from the wrong side or appear black.\n\n"
    "3. INSIDE-OUT OBJECTS: If all faces have reversed normals, the object appears inside-out "
    "(you see the interior instead of exterior).\n\n"
    "Fixing Reversed Faces:\n"
    "- Reverse the vertex order in your face data (e.g., 0,1,2,3 becomes 3,2,1,0)\n"
    "- Or use DRAW3D SET FLAGS with bit 2 (value 4) to invert the normal for specific faces"
)

pdf.section_title('Hidden Face Removal')
pdf.chapter_body(
    "Faces are hidden when the dot product of the surface normal and the camera ray "
    "is positive, meaning the face is pointing away from the camera. This only works correctly "
    "when vertex ordering follows the counter-clockwise convention described above."
)

pdf.section_title('Depth Sorting')
pdf.chapter_body(
    "Two modes are available:\n"
    "- Centroid mode (0): Uses distance from camera to face centroid\n"
    "- Vertex mode (1): Uses maximum distance from camera to any vertex"
)

pdf.section_title('Memory Management')
pdf.chapter_body(
    "- Each 3D object allocates memory for vertices, faces, colors, normals, etc.\n"
    "- Always use DRAW3D CLOSE or DRAW3D CLOSE ALL when finished to free memory.\n"
    "- Maximum 8 simultaneous objects and 3 cameras."
)

# Error Messages
pdf.chapter_title('Error Messages')
pdf.set_font('Helvetica', 'B', 9)
pdf.cell(60, 6, 'Error', 1, 0, 'C')
pdf.cell(0, 6, 'Cause', 1, 1, 'C')
pdf.set_font('Helvetica', '', 9)
errors = [
    ('Object already exists', 'Creating object with ID already in use'),
    ('Minimum of 3 vertices', 'Creating object with < 3 vertices'),
    ('Minimum of 1 face', 'Creating object with no faces'),
    ('Vertex count < 3 for face', 'Face defined with < 3 vertices'),
    ('Camera position not defined', 'Displaying before camera setup'),
    ('Object % is not hidden', 'Restoring non-hidden object'),
    ('Edge colour Index %', 'Color index out of range'),
    ('Fill colour Index %', 'Color index out of range'),
]
for err, cause in errors:
    pdf.cell(60, 6, err, 1, 0, 'L')
    pdf.cell(0, 6, cause, 1, 1, 'L')

# Example description
pdf.add_page()
pdf.chapter_title('Complete Example: Rotating Icosidodecahedron')

pdf.section_title('About This Example')
pdf.chapter_body(
    "This example demonstrates rendering and animating a complex 3D polyhedron called an "
    "icosidodecahedron. This Archimedean solid has:\n\n"
    "- 60 vertices\n"
    "- 32 faces (12 regular pentagons + 20 regular hexagons)\n"
    "- 60 edges\n\n"
    "The program showcases several key features of the DRAW3D system:\n\n"
    "1. VERTEX DEFINITION: Vertices are defined using the golden ratio (phi) to create "
    "mathematically precise coordinates for the polyhedron.\n\n"
    "2. FACE DEFINITION: Faces are defined by listing vertex indices. Pentagons have 5 "
    "vertices each; hexagons have 6. The face vertex counts array tells DRAW3D how many "
    "vertices belong to each face.\n\n"
    "3. COLOR MAPPING: A palette of 3 colors is defined (red, white, black). Each face "
    "references these by index - pentagons use red fill, hexagons use white fill, "
    "all edges use black.\n\n"
    "4. DOUBLE BUFFERING: The FRAMEBUFFER commands enable smooth animation by drawing "
    "to an off-screen buffer, then copying to the display.\n\n"
    "5. QUATERNION ROTATION: MATH Q_CREATE generates rotation quaternions. The parameters "
    "define the rotation angle (in radians) and the axis vector (x, y, z components)."
)

pdf.section_title('Program Flow')
pdf.chapter_body(
    "1. Define vertex coordinates using DATA statements with golden ratio formulas\n"
    "2. Define face connectivity (which vertices form each face)\n"
    "3. Read and scale vertices to fit the screen\n"
    "4. Set up color palette and face-to-color mappings\n"
    "5. Create the 3D object with DRAW3D CREATE\n"
    "6. Configure the camera with DRAW3D CAMERA\n"
    "7. Set up double buffering with FRAMEBUFFER\n"
    "8. Animation loop: rotate, reset accumulated rotation, redraw, copy buffer"
)

pdf.add_page()
pdf.section_title('Program Code')
pdf.code_block(
    "Option explicit            ' Require all variables to be declared\n"
    "Option default none        ' No default variable type\n"
    "\n"
    "' ============================================================\n"
    "' GOLDEN RATIO - fundamental constant for this polyhedron\n"
    "' phi = (1 + sqrt(5)) / 2 = 1.618033988...\n"
    "' ============================================================\n"
    "Dim float phi=(1+Sqr(5))/2\n"
    "\n"
    "' ============================================================\n"
    "' VERTEX DATA - 60 vertices for icosidodecahedron\n"
    "' Coordinates use golden ratio to create precise geometry\n"
    "' Each line: x, y, z coordinates for vertices\n"
    "' ============================================================\n"
    "Data 0,1,3*phi, 0,1,-3*phi, 0,-1,3*phi, 0,-1,-3*phi\n"
    "Data 1,3*phi,0, 1,-3*phi,0, -1,3*phi,0, -1,-3*phi,0\n"
    "Data 3*phi,0,1, 3*phi,0,-1, -3*phi,0,1, -3*phi,0,-1\n"
    "Data 2,(1+2*phi),phi, 2,(1+2*phi),-phi\n"
    "Data 2,-(1+2*phi),phi, 2,-(1+2*phi),-phi\n"
    "Data -2,(1+2*phi),phi, -2,(1+2*phi),-phi\n"
    "Data -2,-(1+2*phi),phi, -2,-(1+2*phi),-phi\n"
    "Data (1+2*phi),phi,2, (1+2*phi),phi,-2\n"
    "Data (1+2*phi),-phi,2, (1+2*phi),-phi,-2\n"
    "Data -(1+2*phi),phi,2, -(1+2*phi),phi,-2\n"
    "Data -(1+2*phi),-phi,2, -(1+2*phi),-phi,-2\n"
    "Data phi,2,(1+2*phi), phi,2,-(1+2*phi)\n"
    "Data phi,-2,(1+2*phi), phi,-2,-(1+2*phi)\n"
    "Data -phi,2,(1+2*phi), -phi,2,-(1+2*phi)\n"
    "Data -phi,-2,(1+2*phi), -phi,-2,-(1+2*phi)\n"
    "Data 1,(2+phi),2*phi, 1,(2+phi),-2*phi\n"
    "Data 1,-(2+phi),2*phi, 1,-(2+phi),-2*phi\n"
    "Data -1,(2+phi),2*phi, -1,(2+phi),-2*phi\n"
    "Data -1,-(2+phi),2*phi, -1,-(2+phi),-2*phi\n"
    "Data (2+phi),2*phi,1, (2+phi),2*phi,-1\n"
    "Data (2+phi),-2*phi,1, (2+phi),-2*phi,-1\n"
    "Data -(2+phi),2*phi,1, -(2+phi),2*phi,-1\n"
    "Data -(2+phi),-2*phi,1, -(2+phi),-2*phi,-1\n"
    "Data 2*phi,1,(2+phi), 2*phi,1,-(2+phi)\n"
    "Data 2*phi,-1,(2+phi), 2*phi,-1,-(2+phi)\n"
    "Data -2*phi,1,(2+phi), -2*phi,1,-(2+phi)\n"
    "Data -2*phi,-1,(2+phi), -2*phi,-1,-(2+phi)\n"
)

pdf.code_block(
    "' ============================================================\n"
    "' FACE DATA - 32 faces total\n"
    "' First 12 faces: pentagons (5 vertices each)\n"
    "' \n"
    "' IMPORTANT: Vertex indices must be listed in COUNTER-CLOCKWISE\n"
    "' order when viewing the face from OUTSIDE the object.\n"
    "' This ensures surface normals point outward for correct:\n"
    "'   - Hidden face removal (back faces not drawn)\n"
    "'   - Lighting calculations (faces lit from correct side)\n"
    "' Wrong order = faces appear/disappear incorrectly!\n"
    "' ============================================================\n"
    "Data 0,28,36,40,32, 33,41,37,29,1, 34,42,38,30,2\n"
    "Data 3,31,39,43,35, 4,12,44,45,13, 15,47,46,14,5\n"
    "Data 17,49,48,16,6, 7,18,50,51,19, 8,20,52,54,22\n"
    "Data 23,55,53,21,9, 26,58,56,24,10, 25,57,59,27,11\n"
    "\n"
    "' Next 20 faces: hexagons (6 vertices each)\n"
    "' Same rule: counter-clockwise from outside view\n"
    "Data 32,56,58,34,2,0, 0,2,30,54,52,28, 29,53,55,31,3,1\n"
    "Data 1,3,35,59,57,33, 13,37,41,17,6,4, 4,6,16,40,36,12\n"
    "Data 5,7,19,43,39,15, 14,38,42,18,7,5, 22,46,47,23,9,8\n"
    "Data 8,9,21,45,44,20, 10,11,27,51,50,26, 24,48,49,25,11,10\n"
    "Data 36,28,52,20,44,12, 13,45,21,53,29,37, 14,46,22,54,30,38\n"
    "Data 39,31,55,23,47,15, 16,48,24,56,32,40, 41,33,57,25,49,17\n"
    "Data 42,34,58,26,50,18, 19,51,27,59,35,43\n"
)

pdf.code_block(
    "' ============================================================\n"
    "' VARIABLE DECLARATIONS\n"
    "' ============================================================\n"
    "Dim float q1(4)            ' Quaternion for rotation (w,x,y,z,m)\n"
    "Dim integer i, j           ' Loop counters\n"
    "Dim integer nf=32          ' Number of faces\n"
    "Dim integer nv=60          ' Number of vertices\n"
    "Dim integer camera=1       ' Camera number to use\n"
    "Dim float vertices(2,59)   ' Vertex array: 3 coords x 60 vertices\n"
    "\n"
    "' ============================================================\n"
    "' READ AND SCALE VERTICES\n"
    "' Scale so object fits nicely on screen (50% of vertical res)\n"
    "' ============================================================\n"
    "For j=0 To 59              ' Loop through all 60 vertices\n"
    "  For i=0 To 2             ' Read x, y, z for each\n"
    "    Read vertices(i,j)\n"
    "  Next i\n"
    "Next j\n"
    "' Scale all vertices: divide by max value, multiply by half screen\n"
    "Math scale vertices(), MM.VRES/Math(max vertices())*0.5, vertices()\n"
)

pdf.code_block(
    "' ============================================================\n"
    "' READ FACE CONNECTIVITY DATA\n"
    "' 12 pentagons x 5 + 20 hexagons x 6 = 180 vertex indices\n"
    "' ============================================================\n"
    "Dim integer faces(179)     ' Array for all face vertex indices\n"
    "For i=0 To 179\n"
    "  Read faces(i)\n"
    "Next i\n"
    "\n"
    "' ============================================================\n"
    "' FACE VERTEX COUNTS\n"
    "' First 12 faces have 5 vertices (pentagons)\n"
    "' Next 20 faces have 6 vertices (hexagons)\n"
    "' ============================================================\n"
    "Dim integer fc(nf-1) = (5,5,5,5,5,5,5,5,5,5,5,5,\n"
    "                        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6)\n"
    "\n"
    "' ============================================================\n"
    "' COLOR PALETTE\n"
    "' Index 0 = Red (for pentagon fills)\n"
    "' Index 1 = White (for hexagon fills)\n"
    "' Index 2 = Black (for all edges)\n"
    "' ============================================================\n"
    "Dim integer colours(2) = (RGB(red), RGB(white), RGB(black))\n"
    "\n"
    "' Edge colors: all faces use color index 2 (black)\n"
    "Dim integer edge(nf-1) = (2,2,2,2,2,2,2,2,2,2,2,2,\n"
    "                          2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2)\n"
    "\n"
    "' Fill colors: pentagons=0 (red), hexagons=1 (white)\n"
    "Dim integer fill(nf-1) = (0,0,0,0,0,0,0,0,0,0,0,0,\n"
    "                          1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)\n"
)

pdf.add_page()
pdf.code_block(
    "' ============================================================\n"
    "' CREATE 3D OBJECT AND SET UP CAMERA\n"
    "' ============================================================\n"
    "' Create initial rotation quaternion (small rotation to start)\n"
    "Math q_create Rad(2), 1, 0.5, 0.25, q1()\n"
    "\n"
    "' Create the 3D object with all geometry and color data\n"
    "' Parameters: object#, vertices, faces, camera, vertex data,\n"
    "'             face counts, face indices, colors, edge colors, fills\n"
    "Draw3D create 1, nv, nf, camera, vertices(), fc(), faces(),\n"
    "              colours(), edge(), fill()\n"
    "\n"
    "' Set up camera: viewplane distance=800, position at origin\n"
    "Draw3D camera 1, 800, 0, 0\n"
    "\n"
    "' ============================================================\n"
    "' SET UP DOUBLE BUFFERING FOR SMOOTH ANIMATION\n"
    "' ============================================================\n"
    "FRAMEBUFFER create         ' Create off-screen frame buffer\n"
    "FRAMEBUFFER write f        ' Direct drawing to frame buffer\n"
    "\n"
    "' Initial display of the object\n"
    "Draw3D show 1, 0, 0, 1000  ' Show at position (0,0,1000)\n"
    "\n"
    "' ============================================================\n"
    "' MAIN ANIMATION LOOP\n"
    "' ============================================================\n"
    "Do\n"
    "  ' Create rotation quaternion: 0.25 degrees around axis (1,3,5)\n"
    "  ' This creates a tumbling motion around a diagonal axis\n"
    "  Math q_create Rad(0.25), 1, 3, 5, q1()\n"
    "  \n"
    "  ' Apply rotation to object 1\n"
    "  Draw3D rotate q1(), 1\n"
    "  \n"
    "  ' Reset prepares for next rotation (updates base orientation)\n"
    "  Draw3D reset 1\n"
    "  \n"
    "  ' Redraw the object at same position\n"
    "  Draw3D show 1, 0, 0, 1000\n"
    "  \n"
    "  ' Copy frame buffer to display (f=frame, n=now/display)\n"
    "  FRAMEBUFFER copy f, n\n"
    "Loop                       ' Infinite loop - press Ctrl+C to stop\n"
)

pdf.output("3D_Graphics_User_Manual.pdf")
print("PDF generated: 3D_Graphics_User_Manual.pdf")
