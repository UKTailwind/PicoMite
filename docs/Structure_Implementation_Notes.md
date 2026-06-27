# MMBasic Structure Implementation Technical Notes

## Overview

MMBasic structures (user-defined types) are implemented using a **single vartbl entry per structure variable**, regardless of how many members the structure contains. Structure members do NOT create individual entries in `g_vartbl`. Instead, member metadata is stored in a separate **structure type definition table** (`g_structtbl`), and member access is resolved at runtime by calculating byte offsets into a contiguous memory block.

## Architecture

### Data Structures

#### 1. Structure Type Definition (`s_structdef`)

Defined in [MMBasic.h](MMBasic.h#L288-L294):

```c
typedef struct s_structdef {
    unsigned char name[MAXVARLEN];              // Structure type name (e.g., "POINT")
    int num_members;                            // Number of members in this type
    struct s_structmember members[MAX_STRUCT_MEMBERS];  // Member definitions
    int total_size;                             // Total size in bytes (with alignment)
} structdef_val;
```

#### 2. Structure Member Definition (`s_structmember`)

Defined in [MMBasic.h](MMBasic.h#L280-L286):

```c
typedef struct s_structmember {
    unsigned char name[MAXVARLEN];  // Member name (e.g., "X", "Y")
    unsigned char type;             // T_NBR, T_STR, T_INT, or T_STRUCT (nested)
    unsigned char size;             // String max length, or nested struct type index
    int offset;                     // Byte offset within structure
    short dims[MAXDIM];             // Array dimensions (0 = not an array)
} structmember_val;
```

#### 3. Variable Table Entry (`s_vartbl`)

Structure variables use the standard variable table entry:

```c
typedef struct s_vartbl {
    unsigned char name[MAXVARLEN];  // Variable name (e.g., "PT", "POINTS")
    unsigned char type;             // T_STRUCT | T_IMPLIED (and possibly T_PTR)
    unsigned char level;            // Scope level (0 = global, >0 = local)
    unsigned char size;             // ** STRUCT TYPE INDEX ** (not string size)
    unsigned char namelen;          // Flags (NAMELEN_EXPLICIT, NAMELEN_STATIC)
    int/short dims[MAXDIM];         // Array dimensions (for struct arrays)
    union u_val {
        MMFLOAT f;
        long long int i;
        MMFLOAT *fa;
        long long int *ia;
        unsigned char *s;           // ** POINTER TO STRUCT DATA BLOCK **
    } val;
} vartbl_val;
```

### Global Tables

| Table | Purpose | Size |
|-------|---------|------|
| `g_structtbl[MAX_STRUCT_TYPES]` | Array of pointers to structure type definitions | 32 pointers |
| `g_structcnt` | Count of defined structure types | int |
| `g_vartbl[]` | Variable table (structures use ONE entry each) | MAXVARS entries |

Configuration constants from [configuration.h](configuration.h#L297-L299):
- `MAX_STRUCT_TYPES` = 32 (maximum distinct TYPE definitions)
- `MAX_STRUCT_MEMBERS` = 16 (maximum members per structure)
- `MAX_STRUCT_NEST_DEPTH` = 8 (maximum nesting depth)

## Memory Layout

### Single Structure Variable

For a structure definition:
```basic
TYPE point
    x AS FLOAT
    y AS FLOAT
END TYPE

DIM pt AS point
```

Creates:
1. **One entry in `g_structtbl`** (type definition, allocated once)
2. **One entry in `g_vartbl`** for variable `pt`
3. **One contiguous memory block** (16 bytes) pointed to by `g_vartbl[idx].val.s`

Memory layout for `pt`:
```
Offset  Content         Size
0       x (FLOAT)       8 bytes
8       y (FLOAT)       8 bytes
---     Total           16 bytes
```

### Structure Array

```basic
DIM points(100) AS point
```

Creates:
1. **One entry in `g_vartbl`** for array `points`
2. **One contiguous memory block** (16 × 101 = 1616 bytes, accounting for OPTION BASE)

Memory layout for `points(0)` through `points(100)`:
```
Offset    Content
0         points(0).x, points(0).y     (16 bytes)
16        points(1).x, points(1).y     (16 bytes)
32        points(2).x, points(2).y     (16 bytes)
...
1600      points(100).x, points(100).y (16 bytes)
```

## How vartbl Fields Are Used for Structures

| Field | Usage for Structures |
|-------|---------------------|
| `name[]` | Variable name (e.g., "PT", "POINTS") |
| `type` | `T_STRUCT \| T_IMPLIED` (may include `T_PTR` for STATIC) |
| `level` | Scope level (0 = global) |
| `size` | **Structure type index** into `g_structtbl[]` |
| `namelen` | Flags: `NAMELEN_STATIC` for static variables |
| `dims[]` | Array dimensions (0 for simple struct, >0 for arrays) |
| `val.s` | **Pointer to allocated struct data** |

### Key Insight: `size` Field Repurposing

For regular variables, `size` holds string length. For structures:
- `size` holds the **index into `g_structtbl`** that defines this struct's type
- Retrieved via: `int struct_type = (int)g_vartbl[idx].size;`
- Then access type definition: `g_structtbl[struct_type]`

## Structure Member Access Resolution

When code accesses `pt.x` or `points(5).y`:

### Step 1: Parse Variable Name
In `findvar()` ([MMBasic.c](MMBasic.c#L3378-L3420)):
- Detect dot in name: `unsigned char *dot = strchr(name, '.')`
- Split into base name ("pt") and member path ("x")

### Step 2: Find Base Variable
Using `FindStructBase()` ([MMBasic.c](MMBasic.c#L3202-L3260)):
- Hash-based lookup in local then global variable space
- Verify `type & T_STRUCT`
- Return struct type index from `size` field

### Step 3: Resolve Member Path
Using `ResolveStructMember()` ([MMBasic.c](MMBasic.c#L2944-L3195)):
- Look up member in `g_structtbl[type_idx]->members[]`
- Use `FindStructMember()` for name matching
- Calculate byte offset from `member.offset`
- Handle nested structs recursively
- Handle array indexing for member arrays

### Step 4: Return Pointer
- Final pointer = `base_ptr + total_offset + array_offset`
- Set `g_StructMemberType` for type checking
- Set `g_StructMemberOffset` and `g_StructMemberSize` for STRUCT operations

## Structure Definition Processing

Structure types are parsed during `PrepareProgram()` (program preparation phase), not at runtime.

### Parsing Flow ([MMBasic.c](MMBasic.c#L650-L810))

1. **Detect TYPE token** in token stream
2. **Parse type name** and check for duplicates
3. **Allocate `s_structdef`** in memory
4. **Parse members** until END TYPE:
   - Call `ParseStructMember()` ([Commands.c](Commands.c#L5891-L6070))
   - Calculate aligned offsets
   - Support INTEGER, FLOAT, STRING LENGTH n, nested types
5. **Finalize type** with padding for alignment
6. **Increment `g_structcnt`**

### Member Offset Calculation

From [Commands.c](Commands.c#L6024-L6032):
```c
// Align integers, floats, and nested structures to 8-byte boundary
offset = sd->total_size;
if ((type == T_INT || type == T_NBR || type == T_STRUCT) && (offset % 8) != 0) {
    offset = ((offset / 8) + 1) * 8;
}
```

## Variable Creation (DIM AS structtype)

When `DIM pt AS point` executes ([Commands.c](Commands.c#L4660-L4890)):

1. **Detect AS keyword** and structure type name
2. **Look up type** in `g_structtbl` → get type index
3. **Call `findvar()`** with `V_DIM_VAR`
4. **Allocate memory** for struct data (size from `g_structtbl[idx]->total_size`)
5. **Set vartbl fields**:
   - `type = T_STRUCT | T_IMPLIED`
   - `size = struct_type_index`
   - `val.s = allocated_memory_pointer`

### For Arrays
Memory allocated = `total_size × num_elements`

Where `num_elements` = product of (dim[i] + 1 - OPTION BASE) for all dimensions.

## STATIC Structure Variables

STATIC structures create TWO vartbl entries:

1. **Global entry** with mangled name (`"funcname\x1evarname"`)
   - Holds actual struct data
   - Persists across function calls

2. **Local entry** with original name
   - `type |= T_PTR` (pointer flag)
   - `val.s` points to global entry's data
   - Destroyed when function exits

The `\x1e` (ASCII Record Separator) in the name prevents conflicts with structure member syntax (which uses `.`).

## Example: Complete Memory Trace

```basic
TYPE room
    temp AS FLOAT
    name AS STRING LENGTH 20
END TYPE

FUNCTION thisreading() AS room
    STATIC integer n  ' Creates global "thisreading\x1en" 
    STATIC data AS room  ' Creates global "thisreading\x1edata"
    n = n + 1
    data.temp = 22.5
    data.name = "Kitchen"
    thisreading = data
END FUNCTION
```

### g_structtbl[0] (room type):
```
name = "ROOM"
num_members = 2
members[0] = { name="TEMP", type=T_NBR, offset=0, size=8 }
members[1] = { name="NAME", type=T_STR, offset=8, size=20 }
total_size = 29 (8 + 21, rounded up for alignment)
```

### g_vartbl entries when function runs:

| Index | Name | Type | Size | val.s |
|-------|------|------|------|-------|
| G1 | "THISREADING\x1EN" | T_INT | 8 | (integer value) |
| G2 | "THISREADING\x1EDATA" | T_STRUCT | 0 | → 32-byte block |
| L1 | "N" | T_INT\|T_PTR | 8 | → G1's value |
| L2 | "DATA" | T_STRUCT\|T_PTR | 0 | → G2's data block |

## Summary: Key Design Decisions

1. **One vartbl entry per variable** - Members are NOT individual variables
2. **Type definitions separate from instances** - `g_structtbl` holds metadata
3. **Contiguous memory blocks** - Efficient for arrays and memory operations
4. **Offset-based member access** - Calculated at runtime, no pointer chasing
5. **Alignment enforced** - 8-byte boundary for numeric types
6. **Nested structures supported** - Via recursive type references
7. **STATIC uses mangled names** - With `\x1e` separator to avoid dot conflicts
