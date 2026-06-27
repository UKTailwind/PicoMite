ONESHOT triggerPin, sense, outputPin, prePulseDelay, pulseWidth [, quiescentPeriod]
or
ONESHOT R, triggerPin, sense, outputPin, prePulseDelay, pulseWidth [, quiescentPeriod]
or
ONESHOT DISABLE

Will configure a background one-shot pulse generator.

When the selected trigger edge is detected on 'triggerPin', MMBasic starts a delay of 'prePulseDelay' microseconds, then toggles 'outputPin'.  After 'pulseWidth' microseconds it toggles 'outputPin' again.  This creates a pulse in the opposite sense to the output pin's idle state.

An optional 'quiescentPeriod' (microseconds) can be supplied.  If present, ONESHOT will ignore retriggers for that period after the pulse completes.

'sense' selects the trigger edge transition and can be POSITIVE (also RISING) or NEGATIVE (also FALLING).  Triggering is edge based (transition), not level-state based.

Timing is interrupt driven and non-blocking.  Delay and pulse width are specified in microseconds.

`ONESHOT R` enables retriggerable mode.  In this mode, a new trigger while active will restart timing instead of being ignored.

In retriggerable mode:

If a trigger arrives during pre-pulse delay, the pre-pulse delay is restarted from that trigger.

Triggers are not queued.  During pre-pulse delay, each new valid trigger replaces the previous pending delay and restarts timing.

This means the pulse starts only after a quiet gap at least as long as `prePulseDelay` following the most recent valid trigger.

If triggers continue to arrive more frequently than `prePulseDelay`, the output transition can be deferred indefinitely until triggers stop.

If a trigger arrives during the pulse, the pulse width timer is restarted from that trigger (the output stays in its active pulse state).

If a trigger arrives during quiescent period, a new cycle is started (pre-pulse delay then pulse).

'prePulseDelay' may be set to 0, in which case the pulse starts immediately on the selected trigger edge.

'pulseWidth' must be a positive value (>0).

Notes:

'outputPin' must be OFF or DOUT when ONESHOT is configured.

If 'outputPin' is OFF it will be configured as DOUT and driven low before ONESHOT is armed.

If 'outputPin' is already DOUT, its current state (high or low) is preserved and the generated pulse is the opposite polarity.

Only one ONESHOT configuration can be active at any one time.

In standard (non-retriggerable) mode, additional triggers received while ONESHOT is in the pre-pulse delay period, while the pulse is active, or during any configured quiescent period are ignored.

ONESHOT DISABLE stops background operation, cancels pending timing events and restores the output pin to its idle state if a pulse was in progress.

ClearExternalIO() also disables and clears ONESHOT background activity.

The trigger and output pins must be different pins.

Example:

SETPIN GP2, DIN
SETPIN GP3, DOUT
ONESHOT GP2, POSITIVE, GP3, 50, 200

This waits for a rising edge on GP2, delays 50µs, toggles GP3, then toggles GP3 again 200µs later.

Retriggerable example:

SETPIN GP2, DIN
SETPIN GP3, DOUT
ONESHOT R, GP2, POSITIVE, GP3, 50, 200, 500

In this mode, each valid rising edge on GP2 retriggers ONESHOT while active.
If a trigger arrives during pre-pulse delay, the 50µs delay restarts.
If a trigger arrives during pulse, the 200µs pulse timer restarts.
If a trigger arrives during the 500µs quiescent period, a new cycle starts.
