# Advanced Keys Architecture

This document describes the internal state machines and lifecycle for Advanced Keys in `libhmk/src/advanced_keys.c`.

## Tap-Hold

A Tap-Hold key sends one keycode if tapped, and a different one if held. It relies on a time threshold (Hold Timeout) to transition states, or can be forced to resolve early by other key events.

```mermaid
stateDiagram-v2
    [*] --> NONE
    
    NONE --> TAP: Key Pressed
    
    TAP --> HOLD: Timeout Reached
    TAP --> RESOLVE_TAP: Key Released (before timeout)
    TAP --> RESOLVE_HOLD: Another Key Pressed (Interrupted)
    
    HOLD --> NONE: Key Released
    
    RESOLVE_TAP --> NONE: Tap Action Dispatched
    RESOLVE_HOLD --> HOLD: Hold Action Dispatched
```

## Toggle

Toggle keys alternate between a toggled state and a non-toggled state on tap. If held past a timeout, they behave as a normal momentary key.

```mermaid
stateDiagram-v2
    [*] --> NONE
    
    NONE --> TOGGLE_STAGE_TOGGLE: Key Pressed
    
    TOGGLE_STAGE_TOGGLE --> TOGGLE_STAGE_NORMAL: Timeout Reached (Hold)
    TOGGLE_STAGE_TOGGLE --> NONE: Key Released (before timeout) -> Toggles State
    
    TOGGLE_STAGE_NORMAL --> NONE: Key Released (Reverts State)
```

## Dynamic Keystroke (DKS)

Dynamic Keystroke triggers different actions based on traversal through the keystroke: Press, Bottom Out, Release from Bottom Out, and Full Release.

```mermaid
stateDiagram-v2
    [*] --> RESTING
    
    RESTING --> PRESS: Key passes Actuation Point
    PRESS --> BOTTOM_OUT: Key reaches Bottom Out threshold
    
    BOTTOM_OUT --> RELEASE_FROM_BOTTOM: Key lifts from Bottom Out
    RELEASE_FROM_BOTTOM --> RESTING: Key passes Release Point
    
    PRESS --> RESTING: Key passes Release Point (without bottoming out)
```

## Combo Engine

The combo engine buffers incoming key presses for a short duration (`DEFAULT_COMBO_TERM = 50ms`) to see if they match a configured chord.

```mermaid
stateDiagram-v2
    [*] --> IDLE
    
    IDLE --> BUFFERING: First Combo-eligible Key Pressed
    BUFFERING --> BUFFERING: Additional Key Pressed (within Combo Term)
    
    BUFFERING --> MATCH_FOUND: Exact Combo Match
    BUFFERING --> TIMEOUT: Combo Term Expires (No Match)
    
    MATCH_FOUND --> ACTIVE: Dispatch Combo Output
    ACTIVE --> IDLE: All Combo Keys Released
    
    TIMEOUT --> FLUSH: Flush buffered keys to standard layout processor
    FLUSH --> IDLE
```

## Macro Engine

The macro engine executes a sequence of recorded actions (Tap, Press, Release, Delay). 

```mermaid
stateDiagram-v2
    [*] --> IDLE
    
    IDLE --> PLAYING: Macro Triggered
    
    PLAYING --> DELAYING: Delay Action Encountered
    DELAYING --> PLAYING: Delay Timer Expired
    
    PLAYING --> PLAYING: Execute Action & Advance Index
    
    PLAYING --> IDLE: Reached END action or max events
```
