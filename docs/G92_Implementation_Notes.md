# G92 Workspace Coordinate System Implementation

## Overview
Implemented G92 workspace coordinate offsets for PicoMite stepper system, allowing users to define work coordinate systems independent of machine zero position.

## Coordinate System Hierarchy

### 1. Hardware Position (Machine Coordinates)
- Physical step count from power-on or last G28 home
- Stored in `axis->current_pos` (steps)
- Established by:
  - `STEPPER POSITION` command (user-defined zero)
  - `G28` homing (true machine zero from limit switches)

### 2. G92 Workspace Offset
- Offset between hardware position and user workspace
- Stored in `stepper_system.x/y/z_g92_offset` (mm)
- Set by `G92` command without moving machine
- Cleared by `G28` homing

### 3. Workspace Position (User Coordinates)
- What the user sees and programs
- Formula: `workspace_pos = hardware_pos + g92_offset`
- All G-code moves use workspace coordinates
- Reported by `get_axis_position_mm()`

## Implementation Details

### Data Structure Changes (stepper.h)
```c
typedef struct {
    // ... existing fields ...
    
    // Workspace coordinate system (G92 offsets)
    float x_g92_offset;     // G92 workspace offset for X axis (mm)
    float y_g92_offset;     // G92 workspace offset for Y axis (mm)
    float z_g92_offset;     // G92 workspace offset for Z axis (mm)
    bool position_known;    // True if POSITION or G28 has established machine position
} stepper_system_t;
```

### Key Function Modifications

#### 1. get_axis_position_mm()
**Purpose**: Report workspace position to user

**Formula**:
```c
workspace_position = (current_pos / steps_per_mm) + g92_offset
```

**Example**:
- Hardware at 100mm, G92 offset = -50mm → Reports 50mm workspace

#### 2. position_within_limits()
**Purpose**: Check soft limits in hardware coordinates

**Formula**:
```c
hardware_position = workspace_position - g92_offset
```

**Rationale**: Soft limits are defined in machine coordinates to protect hardware

**Example**:
- Soft limit: 0-200mm hardware
- G92 offset: -50mm (workspace zero at hardware 50mm)
- Workspace move to 150mm → Hardware 100mm (within limits ✓)
- Workspace move to 160mm → Hardware 110mm (within limits ✓)
- Workspace move to 170mm → Hardware 120mm (within limits ✓)

#### 3. planner_sync_to_physical()
**Purpose**: Sync planner coordinates to current position after homing or buffer clear

**Formula**:
```c
planner_x = (current_pos / steps_per_mm) + g92_offset
```

**Rationale**: Planner works in workspace coordinates for seamless move chaining

### G92 Command Implementation

#### Command Syntax
```basic
STEPPER GCODE G92, X, 10, Y, 20, Z, 5
```

#### Behavior
- Does NOT move the machine
- Redefines current position in workspace coordinates
- Formula: `g92_offset = hardware_position - specified_workspace_position`

#### Example
```basic
' Machine is at hardware position 100mm
STEPPER GCODE G92, X, 10
' Now workspace shows X=10, hardware still at 100mm
' g92_offset = 100 - 10 = 90mm
```

### Position Validation

#### Purpose
Prevent motion before position is established (prevents "lost position" errors)

#### Implementation
```c
if (!stepper_system.position_known)
    error("Machine position unknown - use STEPPER POSITION or G28 homing first");
```

#### Position Established By
1. **STEPPER POSITION** command
   - User manually sets position
   - Sets `position_known = true`
   - Useful for soft-start without limit switches

2. **G28 Homing**
   - Automatically finds machine zero via limit switches
   - Sets `position_known = true`
   - Clears all G92 offsets
   - Most accurate method

### G28 Homing Behavior

#### G92 Offset Reset
G28 always clears workspace offsets:
```c
stepper_system.x_g92_offset = 0.0f;
stepper_system.y_g92_offset = 0.0f;
stepper_system.z_g92_offset = 0.0f;
```

**Rationale**: Homing establishes true machine zero, previous workspace definitions become invalid

#### Workflow
1. User runs G28 → Machine homes, position zeroed, offsets cleared
2. User moves to work piece: `G0 X50 Y30`
3. User sets work zero: `G92 X0 Y0`
4. Now workspace 0,0 is at hardware 50,30
5. User programs work in workspace coordinates
6. Re-homing clears G92, back to machine coordinates

## Soft Limits and G92

### Problem
Soft limits protect hardware from over-travel. With G92, workspace coordinates differ from machine coordinates.

### Solution
Convert workspace coordinates to hardware coordinates before limit checking:

```c
// In position_within_limits():
float hw_pos_mm = workspace_pos_mm - g92_offset;
int32_t pos_steps = (int32_t)(hw_pos_mm * axis->steps_per_mm);
return (pos_steps >= axis->min_limit && pos_steps <= axis->max_limit);
```

### Example Scenario
```
Hardware soft limits: X 0-200mm
Hardware position: 50mm
G92 offset: -50mm (workspace zero at hardware 50mm)
Workspace shows: 0mm

Move to workspace X150:
- Hardware target: 150 - (-50) = 200mm
- Within limits: 200 <= 200 ✓

Move to workspace X151:
- Hardware target: 151 - (-50) = 201mm
- Exceeds limits: 201 > 200 ✗ ERROR
```

## User Workflows

### Workflow 1: Simple Setup (No Homing)
```basic
STEPPER POSITION X, 0  ' Manually set position
STEPPER POSITION Y, 0
STEPPER RUN            ' Now allowed - position known

STEPPER GCODE G92, X, 10, Y, 5  ' Set work offset
STEPPER GCODE G0, X, 0, Y, 0    ' Move to workspace origin
```

### Workflow 2: With Homing
```basic
STEPPER HWLIMITS 2, 3, 4       ' Configure limit switches
STEPPER GCODE G28, X, 1, Y, 1  ' Home X and Y axes
' Position now known, offsets cleared

STEPPER GCODE G0, X, 50, Y, 30  ' Move to workpiece
STEPPER GCODE G92, X, 0, Y, 0   ' Set workpiece as origin
STEPPER GCODE G1, X, 10, F, 300 ' Cut at workspace X=10
```

### Workflow 3: Re-homing Mid-Job
```basic
' ... job in progress with G92 active ...
STEPPER GCODE G28, X, 1, Y, 1   ' Re-home (clears G92!)
' WARNING: G92 offsets lost - must re-establish:
STEPPER GCODE G0, X, 50, Y, 30  ' Return to workpiece
STEPPER GCODE G92, X, 0, Y, 0   ' Re-establish work zero
```

## Testing Considerations

### Test Case 1: G92 Offset Calculation
```basic
STEPPER POSITION X, 100        ' Hardware at 100mm
STEPPER GCODE G92, X, 10       ' Workspace shows 10mm
' Expected: g92_offset = 100 - 10 = 90mm
' Verify: get_axis_position_mm() returns 10mm
```

### Test Case 2: Soft Limits with G92
```basic
STEPPER LIMITS X, 0, 200       ' Hardware limits 0-200mm
STEPPER POSITION X, 50         ' Hardware at 50mm
STEPPER GCODE G92, X, 0        ' Workspace zero at hardware 50mm
STEPPER GCODE G0, X, 150       ' Try workspace 150 (hardware 200)
' Expected: Success (exactly at limit)
STEPPER GCODE G0, X, 151       ' Try workspace 151 (hardware 201)
' Expected: Error "exceeds soft limits"
```

### Test Case 3: G28 Clears G92
```basic
STEPPER POSITION X, 100
STEPPER GCODE G92, X, 50       ' Set offset
' Verify: workspace shows 50mm
STEPPER GCODE G28, X, 1        ' Home
' Expected: Workspace shows 0mm (offset cleared)
```

### Test Case 4: Position Required Before RUN
```basic
STEPPER INIT
STEPPER RUN                    ' Should fail
' Expected: Error "Machine position unknown"
STEPPER POSITION X, 0
STEPPER RUN                    ' Should succeed
```

## Documentation Updates

### stepper.h Header
- Added G92 documentation to STEPPER GCODE section
- Updated POSITION command documentation
- Added note about soft limits respecting G92 offset
- Clarified G28 clears G92 offsets

### Command Help Text
All commands now document workspace vs hardware coordinate usage.

## Files Modified
1. `stepper.h` - Added g92_offset fields, position_known flag, documentation
2. `stepper.c` - Implemented G92, updated position reporting, soft limits, initialization

## Backward Compatibility
- Existing code without G92 behaves identically (offsets default to 0.0)
- POSITION command still works as before
- G28 behavior unchanged (offsets already were zero)
- All existing G-code commands continue to work

## Future Enhancements
- G92.1 (cancel G92 offsets)
- G54-G59 (multiple work coordinate systems)
- Persistent offset storage (survive power cycle)
- G92 offset display command
