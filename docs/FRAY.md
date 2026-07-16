# Fray User Manual

Fray is a 42 HP stereo scene-sequenced glitch effect for VCV Rack. It combines a 128-scene pattern bank, an eleven-lane block sequencer, and ten original Shortwav Labs effects in one playable module.

Fray is stereo monophonic: it sums all channels on each polyphonic input cable to one left/right pair. When **IN R** is unpatched, the summed right channel is normalled from the summed **IN L** signal; when it is patched, the two inputs remain independent.

Panel asset: [FRAY.svg](../res/FRAY.svg)

## Quick start

1. Patch a signal to **IN L**. Patch **IN R** for true stereo, or leave it empty to duplicate the left input.
2. Patch **OUT L** and **OUT R** to a mixer or audio output.
3. Leave **CLOCK** unpatched and set **TEMPO** for internal-clock operation.
4. Click or drag cells in an effect lane to author blocks.
5. Select an effect lane to display its controls in the editor.
6. Raise the effect's mix and adjust its parameters.
7. Use **SCENE**, **NEXT**, **PREV**, **RAND**, or **MUTATE** to move beyond the initial pattern.

Fray advances automatically because **RUN** is normalled high when its input is unpatched.

## Signal flow

```text
IN L/R
  -> active scene and block sequencer
  -> active effects in processing order
  -> each effect's filter / mix / pan / gain stage
  -> master mix / pan / gain
  -> OUT L/R
```

Several effect lanes can be active at the same time. A block determines when an effect participates; the effect editor determines what that processor does while active.

The audio processors run serially. Choose **Serial effect order (overlapping lanes)** in Fray's context menu to assign each chain position; selecting an effect swaps it with the effect already occupying that position, so the chain always contains each processor exactly once. Order changes the sound only while two or more effect lanes are active together. With one active effect—or the Randomizer lane selecting a single overlay effect—the inactive stages are transparent, so reordering is intentionally inaudible.

Cell, scene, and live order boundaries use a short Rack `SlewLimiter` dezipper at the chain output. This softens abrupt topology changes without adding a steady-state delay.

## Scenes and the grid

Fray contains one program of 128 scenes. Each scene stores its own timing, seed, block grid, and effect settings. Macro assignments belong to the whole Fray program and remain in place when scenes change.

The grid runs from left to right and has eleven lanes:

1. Randomizer — control lane; it does not process audio
2. Modulator
3. Tape Stop
4. Retrigger
5. Reverser
6. Stretcher
7. Lofi
8. Distortion
9. Gater
10. Delay
11. Shuffler

Click or drag across cells to author blocks. Block starts are distinct from block continuations so Fray can trigger an effect once and sustain it across later cells. Click an effect lane or its name to select that effect in the editor.

The scene length is determined by **BEATS × DIVISIONS**, from 2 to 64 cells. Changing either value changes the active grid length; cells outside that length remain part of the stored scene but do not play.

### Scene controls

| Control or input | Purpose |
| --- | --- |
| **SCENE** | Selects one of 128 scenes from the panel |
| **SCENE CV** | Voltage-addresses scenes; the default mapping is 1 V/octave |
| **SCENE GATE** | Activates the addressed scene from a gate or trigger |
| **NEXT / PREV** | Steps to the adjacent scene |
| **LOOP** | Repeats the current scene when its last cell completes |

Scene changes load the selected scene's grid and effect settings. Use **RESET** when you want the new scene to restart from its first cell.

## Clock and transport

Fray can run from its internal clock or from an external pulse/gate clock.

### Internal clock

With **CLOCK** unpatched, **TEMPO** sets 30–300 BPM. **TEMPO CV** follows a 1 V/octave relationship: +1 V doubles the knob tempo and -1 V halves it, subject to the 30–300 BPM limits.

### External clock

Patching **CLOCK** gives the external signal transport priority. Select one of two interpretations:

| Mode | External-clock meaning |
| --- | --- |
| **Step** | Every rising edge advances one grid cell |
| **Beat** | Every rising edge marks one beat; Fray schedules the scene's divisions between beats |

Step mode is best when the upstream sequencer already supplies the exact cell rate. Beat mode is useful when one clock pulse should represent a quarter-note beat and Fray should generate triplet, quintuplet, or other subdivisions internally.

### Transport inputs

| Input | Behavior |
| --- | --- |
| **CLOCK** | External pulse/gate clock |
| **TEMPO CV** | 1 V/oct modulation of the internal tempo; inactive for advancement while CLOCK is patched |
| **RESET** | Returns transport to the first cell |
| **RUN** | High runs transport; low pauses it; normalled high when unpatched |

The panel **RUN** and **RESET** controls provide the same performance functions without cables.

## Effect editor

Selecting one of the ten audio-effect lanes opens its active-effect editor. The unique controls change with the selected processor, while the common stage remains consistent:

| Common control | Function |
| --- | --- |
| **Filter** | Shapes that effect's output with selectable filtering, cutoff, and resonance |
| **Mix** | Blends the effect's dry and wet signals |
| **Pan** | Positions the effect contribution in the stereo field |
| **Gain** | Sets the effect-stage output level |

The master **MIX**, **PAN**, and **GAIN** controls operate after the complete effect chain. **MIX CV** modulates the master dry/wet balance.

Parameter mappings remain associated with their effect even when another editor is visible. Changing the selected lane only changes which controls are shown.

Fray's audio layer uses Rack's `SlewLimiter`, `ExponentialSlewLimiter`, and `ExponentialFilter` for activation and parameter smoothing; `TBiquadFilter<double>` and `RCFilter` for common, tone, and DC filtering; `dsp::hann()` for grain windows; `math::crossfade()` for linear blends; and Rack's fixed-ratio resamplers for 2x distortion. Common biquad coefficients update on a Rack `ClockDivider`, and double coefficient/state precision keeps the 20 Hz, high-Q settings stable at 384 and 768 kHz. Fray keeps custom circular histories and four-point Hermite reads because Rack's FIFO and linear interpolation utilities do not provide arbitrary fractional-delay playback.

### Latency

Fray does not add a fixed delay to its dry or bypass paths. Modulator, Lofi, and Gater also operate without lookahead. Tape Stop, Retrigger, Reverser, Stretcher, Delay, and Shuffler intentionally play past audio as part of their effect.

Distortion's **QUALITY** control is a discrete choice:

| Mode | Added distortion wet-path delay |
| --- | --- |
| **Raw** | 0 samples of fixed FIR/lookahead delay; this is the default |
| **2x oversampled** | 7 samples from Rack's linear-phase upsampler/decimator pair (about 0.15 ms at 48 kHz) |

Only the selected quality path runs. A live quality change pre-rolls the newly selected FIR state and applies a short wet-only transition; the immediate dry contribution is never frozen or delayed, and Raw and 2x are not continuously mixed. Rack bypass and Fray's master dry path remain immediate; Fray does not impose a seven-sample delay on the whole module. Partial Distortion Dry, common Mix, or master Mix settings can still combine immediate dry audio with the delayed oversampled wet path. The Raw path still has the frequency-dependent phase response of its DC and tone filters, but it avoids the fixed seven-sample FIR delay; use Raw when that fixed latency is undesirable.

## Randomize, Mutate, and macros

**RAND** creates a new version of the active scene. It can be triggered by the panel control or the **RAND** input. The Randomizer lane supplies scene-timed variation without adding another audio processor.

**MUTATE** makes a smaller change to the active scene while preserving more of its current identity. Its panel control and **MUTATE** input are intended for live variation.

Fray uses deterministic scene seeds, so a saved patch recalls the same authored scene state. Explicit randomize or mutate actions create new state rather than adding uncontrolled sample-by-sample randomness.

Four macro controls, **A–D**, provide performance modulation for assigned effect parameters. Their corresponding **MOD A–D** inputs add CV control without requiring a dedicated jack for every effect setting.

Choose **Macro destinations** in Fray's context menu to route each macro. A destination can follow one of the first four controls in the currently selected editor or remain pinned to a named control on a specific effect.

## Outputs

| Output | Signal |
| --- | --- |
| **OUT L / OUT R** | Processed stereo audio |
| **STEP** | Trigger when Fray advances to a new cell |
| **EOC** | Trigger at the end of the active scene cycle |
| **ACTIVITY** | Polyphonic gate channels representing active audio-effect lanes |

## Rack integration and saving

Fray participates in Rack's native module workflow:

- **Initialize** restores Fray's default program and runtime state.
- **Randomize** randomizes the active scene rather than all 128 scenes.
- Rack presets can capture and recall the complete one-program scene bank.
- Panel editing and native Initialize/Randomize actions participate in Rack undo and redo.
- Module bypass passes stereo audio through; the right-channel normal remains available when **IN R** is unpatched.

Use Rack's module context menu for Initialize, Randomize, preset management, Scene CV mapping, optional reset-on-Run, macro destinations, and serial effect order.

## Version 1 scope

- One program containing 128 scenes
- Eleven sequencer lanes: one control lane and ten audio effects
- Stereo monophonic processing at Rack's native sample rate
- Internal and external clocking with Step and Beat interpretations
- Four assignable macro CV inputs
- No MIDI Program Change or multi-program bank switching

The effect algorithms and sonic character are original Fray designs. Fray adopts a scene-sequenced multi-effect workflow, but it is not an exact emulation of another product and does not claim identical parameter ranges, transfer functions, timing quirks, presets, or sound.

For the design rationale, clean-room boundaries, and implementation research, see [Fray: Glitch2-Inspired VCV Rack Module Research](GLITCH2_VCV_MODULE_RESEARCH.md).
