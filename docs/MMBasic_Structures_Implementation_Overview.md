# MMBasic Structures Implementation Overview

## From a User Perspective

This document provides a high-level overview of how user-defined structures are implemented in MMBasic for PicoMite, focusing on the capabilities and architecture from both a user and developer perspective.

---

## 1. User-Facing Capabilities

### 1.1 Structure Definition (TYPE...END TYPE)
Users can define custom data types that group multiple related fields together:

```basic
Type Point
  x As INTEGER
  y As INTEGER
End Type

Type Person
  age As INTEGER
  height As FLOAT
  name As STRING LENGTH 50
End Type
```

**Supported Member Types:**
- `INTEGER` - 64-bit signed integer
- `FLOAT` - 64-bit floating point
- `STRING` - Variable-length strings (with optional LENGTH specifier)
- Nested structures - Structures can contain other structures

### 1.2 Structure Variables and Arrays
Structures can be used as:
- **Single variables**: `Dim p As Point`
- **Arrays**: `Dim points(100) As Point`
- **Multi-dimensional arrays**: `Dim grid(10, 20) As Point`

### 1.3 Member Access
Members are accessed using dot notation:
```basic
p.x = 100
p.y = 200
Print points(5).x
```

**Advanced Features:**
- **Nested member access**: `widget.bounds.topLeft.x`
- **Array members**: `record.data(5)`
- **Complex paths**: `objects(0).nested(1).values(2)`

### 1.4 Structure Assignment
Entire structures can be copied:
```basic
destination = source              ' Copy whole structure
points(5) = points(0)            ' Copy array elements
```

### 1.5 Integration with Commands

**MATH Commands with Structure Arrays:**
Structures integrate seamlessly with MATH array operations using stride-aware access:
```basic
Dim pts(100) As Point
' Operate on just the x coordinates across all points
MATH SCALE pts().x, 2.0, pts().x      ' Scale all x values
MATH ADD pts().y, 10, pts().y          ' Add to all y values
sum = MATH(SUM pts().x)                ' Sum all x values
```

**Other Commands:**
- `LIST TYPE` - Display structure definitions
- File I/O - Read/write binary structure data
- SUB/FUNCTION parameters - Pass structures by reference

### 1.6 Function Return Values
Functions can return structure values:
```basic
Function CreatePoint(x%, y%) As Point
  Local pt As Point = (x%, y%)
  CreatePoint = pt
End Function
```

---

## 2. Implementation Architecture

### 2.1 Key Data Structures

**Global Structure Type Table** (`Commands.c`):
```c
struct s_structdef *g_structtbl[MAX_STRUCT_TYPES];  // Array of pointers to type definitions
int g_structcnt;                                     // Number of defined types
```

**Type Definition** (`MMBasic.h`):
```c
typedef struct s_structdef {
    unsigned char name[MAXVARLEN];                      // Type name (e.g., "Point")
    int num_members;                                    // Number of members
    struct s_structmember members[MAX_STRUCT_MEMBERS];  // Member definitions
    int total_size;                                     // Total size in bytes (with padding)
} structdef_val;
```

**Member Definition** (`MMBasic.h`):
```c
typedef struct s_structmember {
    unsigned char name[MAXVARLEN];    // Member name (e.g., "x")
    unsigned char type;               // T_NBR, T_STR, T_INT, or T_STRUCT
    unsigned char size;               // String length OR nested struct index
    int offset;                       // Byte offset within parent structure
    short dims[MAXDIM];              // Array dimensions (0 = not array)
} structmember_val;
```

### 2.2 Global State Variables (`Commands.c`)

These track the current structure access context:

```c
int g_StructArg;              // Struct type index for DIM AS structtype (-1 if none)
int g_StructMemberType;       // Type of member being accessed (0 = whole struct)
int g_StructMemberOffset;     // Byte offset of member within struct
int g_StructMemberSize;       // Size of the member
int g_ExprStructType;         // Struct type from expression evaluation
```

### 2.3 Memory Layout and Alignment

**Alignment Rules:**
- Strings: No alignment (placed sequentially)
- INTEGER/FLOAT: 8-byte alignment
- Nested structures: 8-byte alignment
- Padding inserted automatically between members to maintain alignment

**Array Storage:**
- Structure arrays store elements sequentially
- Total structure size includes padding to maintain alignment for array elements

---

## 3. Core Implementation Components

### 3.1 Type Definition Phase (`MMBasic.c`)

**Function: `PrepareProgram()`**
- Scans program for `TYPE...END TYPE` blocks during tokenization
- Allocates memory for each structure type definition
- Parses members and calculates offsets with proper alignment
- Validates structure definitions before execution begins

**Function: `ParseStructMember()`** (NOT in RAM)
- Parses individual member declarations
- Determines member type, size, and array dimensions
- Calculates byte offset based on alignment rules
- Handles nested structure references

### 3.2 Variable Declaration (`MMBasic.c`)

**Function: `findvar()`**
- Handles `DIM varname AS typename` syntax
- Looks up structure type by name in `g_structtbl`
- Stores structure type index in variable's `size` field
- Allocates memory = `total_size * array_elements`

### 3.3 Member Resolution (`MMBasic.c`)

**Function: `ResolveStructMember()`** (NOT in RAM - large function)
- **Central workhorse for all member access operations**
- Handles dot notation parsing (e.g., `point.x`, `widget.bounds.topLeft.x`)
- Resolves nested structures recursively
- Handles array indices at any nesting level
- Updates global state variables (`g_StructMemberType`, etc.)
- Returns pointer to final member data

**Key Features:**
- **Unified resolution**: Handles simple, nested, and arrayed members
- **Depth limiting**: Prevents excessive nesting (MAX_STRUCT_NEST_DEPTH)
- **Type validation**: Ensures member exists and types are compatible
- **Bounds checking**: Validates array indices at each level

**Integration Points:**
The resolution happens inside `findvar()` when it detects:
1. Variable type is `T_STRUCT`
2. Parse position has a dot (`.`)
3. Calls `ResolveStructMember()` to navigate the member path

### 3.4 Assignment Operations (`Commands.c`)

**Function: `cmd_let()`**
- Handles `variable = expression` for all types
- For structures: validates type match and copies entire structure data
- Uses `memcpy()` to transfer structure contents
- Validates `g_ExprStructType` matches destination type

### 3.5 Function Parameter Passing (`MMBasic.c`)

**Function: `DoExpression()`** (SUB/FUNCTION call handler)
- Structure parameters passed by reference (pointer to structure data)
- `ValidateStructParam()` ensures type compatibility
- Nested member access: uses `g_StructMemberType` for actual member type
- Arrays of structures: pass base pointer with element access in SUB

**Function: `ValidateStructParam()`** (NOT in RAM)
- Checks parameter structure type matches formal parameter
- Handles both whole structures and member access
- Returns 1 if struct parameter validated, 0 otherwise

### 3.6 Function Return Values (`MMBasic.c`)

**Function: `CopyStructReturn()`** (NOT in RAM)
- Allocates temporary memory for returned structure
- Copies structure data before `ClearVars()` destroys local variables
- Sets `g_ExprStructType` so caller can validate type match
- Returns pointer to temporary structure data

---

## 4. Integration with MATH Commands (`Draw.c`)

### 4.1 Stride-Aware Array Access

**The Challenge:**
Operating on a single member across an array of structures requires "strided" access:
```
struct Point { x, y }
Array: [x0,y0,x1,y1,x2,y2...]
To access all x values: start at x0, stride by sizeof(Point)
```

**Function: `EvaluateBasicNumericArray()` in Draw.c**
- Detects structure member array access via `g_StructMemberType`
- Calculates stride = `total_size` of structure
- Returns pointer to first member, stride, and member type
- MATH commands iterate: `ptr`, `ptr + stride`, `ptr + 2*stride`, etc.

**Supported Operations:**
- Element-wise: `C_ADD`, `C_SUB`, `C_MUL`, `C_DIV`
- Scalar: `SCALE`, `ADD`, `POWER`
- Statistics: `SUM`, `MEAN`, `MIN`, `MAX`
- Transformations: `V_ROTATE`, `WINDOW`

### 4.2 Example Usage
```basic
Dim points(99) As Point

' Initialize all x coordinates to sequence
For i = 0 To 99
  points(i).x = i
Next

' Scale all x values by 2.0 using MATH command
MATH SCALE points().x, 2.0, points().x

' Sum all y values
total = MATH(SUM points().y)
```

---

## 5. Key Design Principles

### 5.1 Separation of Concerns
- **Commands.c**: High-level commands, global state, user-facing operations
- **MMBasic.c**: Core parsing, variable management, member resolution
- **Draw.c**: MATH command integration, stride handling
- **Maths.c**: (Limited structure involvement - primarily basic math operations)

### 5.2 Memory Efficiency
- Structure definitions allocated once at program preparation
- Variable data allocated per variable/array
- Temporary memory used for function returns
- Large/complex functions marked as "NOT in RAM" to save SRAM

### 5.3 Type Safety
- Type indices stored in variable table
- Runtime validation of structure type matches
- Member type checking during resolution
- Bounds checking for array access

### 5.4 Flexibility
- Supports arbitrary nesting (with depth limit)
- Arrays at any level (structure arrays, member arrays)
- Seamless integration with existing BASIC commands
- Binary-compatible layout for file I/O

---

## 6. Compilation Control

Structures are conditionally compiled using:
```c
#ifdef STRUCTENABLED
    // Structure-specific code
#endif
```

This allows building PicoMite variants with or without structure support based on memory constraints.

---

## 7. Typical Operation Flow

### Example: `points(5).x = 100`

1. **Tokenization** (prepare time):
   - `TYPE Point` definitions parsed and stored in `g_structtbl`

2. **Variable Declaration**:
   - `DIM points(100) AS Point`
   - `findvar()` allocates array memory
   - Stores structure type index in `g_vartbl[i].size`

3. **Assignment** (`cmd_let`):
   - Left side: `findvar("points")` called
   - Detects array access: parses `(5)`
   - Calculates base: `val.s + (5 * total_size)`
   - Detects dot: calls `ResolveStructMember(base, struct_idx, "x")`
   - Resolution returns pointer to `x` member, sets `g_StructMemberType = T_INT`
   - Right side: evaluates `100`
   - Assignment: `*(long long int *)ptr = 100`

### Example: `MATH SCALE points().x, 2.0, points().x`

1. **Parse arguments**: Detects `points().x` pattern
2. **EvaluateBasicNumericArray()**:
   - Calls `findvar("points")` → returns array base
   - Detects `g_StructMemberType != 0` → structure member access
   - Gets `struct_size` from `g_structtbl[struct_type]->total_size`
   - Sets stride = `struct_size`
   - Returns: `{ptr_to_first_x, stride, member_type}`
3. **MATH SCALE**: Iterates with stride, multiplies each x by 2.0

---

## 8. Notable Implementation Details

### 8.1 String Members
- Stored with length prefix byte
- Size field in member definition = max string length
- Alignment: no special alignment requirement

### 8.2 Nested Structures
- Member `type = T_STRUCT`
- Member `size` field = index into `g_structtbl` for nested type
- Recursive resolution via `ResolveStructMember()`

### 8.3 Array Members Within Structures
- `dims[]` array stores dimensions for each member
- Can have arrays of structures containing array members
- Complex paths like `data(5).values(10)` fully supported

### 8.4 Error Handling
- Comprehensive validation at parse time and runtime
- Type mismatch detection
- Bounds checking
- Nesting depth limits
- Clear error messages with context

---

## 9. Testing and Validation

Extensive test suites demonstrate all features:
- **StructTest.bas**: Basic operations, arrays, parameter passing
- **NestedStructTest.bas**: Multi-level nesting, complex paths
- **MathStructTest.bas**: MATH command integration
- **BoxStructTest.bas**: Real-world 3D graphics use case
- **StructParamTest.bas**: SUB/FUNCTION parameter validation

---

## 10. Limitations and Constraints

- Maximum structure types: `MAX_STRUCT_TYPES` (typically 64)
- Maximum members per structure: `MAX_STRUCT_MEMBERS` (typically 32)
- Maximum nesting depth: `MAX_STRUCT_NEST_DEPTH` (typically 10)
- String length: Maximum 255 characters per string member
- Structures must be defined before use (at program start)
- No runtime type creation (all types defined statically)

---

## Summary

The MMBasic structure implementation provides a robust, type-safe system for user-defined data types with deep integration into the language. The architecture cleanly separates type definition (parse time), variable management (declaration), and member access (runtime), while maintaining efficient memory usage and enabling powerful features like stride-aware MATH operations on structure array members.

The design prioritizes:
- **User convenience**: Natural syntax, seamless integration
- **Performance**: Direct pointer arithmetic, efficient memory layout
- **Safety**: Type checking, bounds validation, error detection
- **Flexibility**: Nesting, arrays, complex access patterns
- **Compatibility**: Optional compilation, memory-constrained targets
