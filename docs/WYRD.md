# Wyrd User Manual

**Patchable tone source, external processor, nonlinear time/filter instrument, and ambient reverb for VCV Rack**

Wyrd is a playable sound-design instrument built around a tone core, external-signal conditioning, internal modulation, and a feedback delay/filter network. It can operate without an input as a self-contained voice, process audio or CV through the **External** input, or combine both paths. Six touch bridges temporarily reroute internal signals, making the module useful for evolving drones, resonant echoes, unstable feedback, and hands-on performance.

Wyrd's controls are intentionally interactive. **Activation**, **Time**, **Decay**, **Filter**, and **Absorb** all influence the feedback behavior, so small changes can produce large shifts when the network is near instability.

---

## Table of Contents

1. [Overview](#overview)
2. [First Patch](#first-patch)
3. [Signal Flow](#signal-flow)
4. [External and Agitation Sections](#external-and-agitation-sections)
5. [Activation and Tone Core](#activation-and-tone-core)
6. [Time, Filter, and Output Sections](#time-filter-and-output-sections)
7. [Touch Bridges](#touch-bridges)
8. [Ambient Reverb](#ambient-reverb)
9. [Inputs](#inputs)
10. [Outputs and Lights](#outputs-and-lights)
11. [Context Menu](#context-menu)
12. [Factory Presets](#factory-presets)
13. [Patch Recipes](#patch-recipes)
14. [Troubleshooting](#troubleshooting)
15. [Technical Notes](#technical-notes)

---

## Overview

Wyrd contains five interacting blocks:

| Block | Purpose |
| --- | --- |
| External and Strength | Conditions an external audio or CV signal, then extracts two related CV signals |
| Agitation | Generates an internal unipolar gesture from slow movement to audio-rate modulation |
| Tone Core | Produces a pitched oscillator, subharmonics, and a morphing nonlinear tone palette |
| Time/Filter | Adds delay, diffusion, damping, saturation, modulation, and feedback |
| Ambient Reverb | Places the final result in a modulated, diffused stereo-style tank summed to mono outputs |

The module can be used in three basic ways:

- **Standalone voice**: Raise Activation and Level, tune Tonic, and shape the result with Tones, Time, Filter, and Decay.
- **External processor**: Patch audio to External, raise Strength and External Constant, then blend the processed signal with the Time/Filter path.
- **Hybrid instrument**: Mix the internal tone with an external source and use CV1, CV2, Agitation, or a context-menu source to animate the touch bridges.

---

## First Patch

### Standalone Drone

1. Add **Wyrd** to your patch.
2. Patch **Modular** to a mixer or audio interface.
3. Set **Level** near 40%.
4. Raise **Activation Constant** to 30-50%.
5. Set **Tonic** to a comfortable pitch and sweep **Tones**.
6. Raise **Blend** to hear the Time/Filter path.
7. Adjust **Time**, **Decay**, and **Filter** in small amounts.
8. Add **Reverb Blend** to taste.

```text
[Wyrd Modular] -> [Mixer / Audio Interface]
```

### External Processing

1. Patch an oscillator, voice, sample, or other signal to **External**.
2. Patch **Modular** to your mixer.
3. Raise **Strength** until the Strength light responds.
4. Raise **External Constant** to inject the processed signal into the core.
5. Set **Activation Constant** low if you want to hear mostly the external source.
6. Use **Blend**, **Time**, **Decay**, **Filter**, and **Absorb** as the main effect controls.

```text
[Audio or CV] -> [Wyrd External]
[Wyrd Modular] -> [Mixer / Audio Interface]
```

Start with moderate Level and Decay. The feedback path can become loud or self-sustaining at high settings.

---

## Signal Flow

```text
External
  -> Strength gain, saturation, and envelope extraction
  -> External Constant
                              \
Tone Core -> Tones -> Activation -> dry signal
                                      \
                                       -> equal-power Blend -> Level
Time/Filter feedback <----------------/                      -> ambient reverb
                                                               -> Modular / Line

Agitation, CV1, CV2, and touch bridges modulate the network.
```

The **Tone Core** and **Sub Harmonics** outputs are tapped before the main Time/Filter blend. **Strength**, **CV1**, **CV2**, and **Agitation** are also available as independent modulation sources.

---

## External and Agitation Sections

### External and Strength

| Control | Range | Description |
| --- | --- | --- |
| Strength | 0-100% | Sets input gain and nonlinear saturation before CV extraction |
| External Constant | 0-100% | Injects the processed external signal into the dry signal path |

The External input accepts audio or CV. With a signal patched, **Strength** affects three related results:

- **Strength output**: the processed bipolar signal
- **CV1 output**: a 0-10 V envelope follower derived from the processed signal
- **External Constant path**: the amount of processed signal mixed into Wyrd's sound engine

A cable with no active signal can still produce faint circuit-style pickup at some Strength settings. Use the context menu's **Strength calibration** choice to match the source level.

### Agitation

| Control | Range | Description |
| --- | --- | --- |
| Speed | 0-100% | Sets agitation rate from long gestures to audio-rate motion |
| Angle | 0-100% | Skews the rise/fall shape of the gesture |
| Speed CV Amount | 0-100% | Attenuates the patched Speed CV |

**Begin and End** is normalled high, so agitation runs when nothing is patched. A low gate or a dummy cable stops it; a voltage at or above the gate threshold starts it. The **Agitation** output spans approximately 0-6 V.

---

## Activation and Tone Core

### Activation

| Control | Range | Description |
| --- | --- | --- |
| Activation Constant | 0-100% | Baseline amount that opens and drives the selected tone |
| Activation Interference | 0-100% | Amount of Time/Filter feedback folded into Activation |
| Activation CV Attenuverter | -100% to +100% | Scales and optionally inverts Activation CV |

Activation behaves like a nonlinear combination of amplitude, drive, and feedback sensitivity. At low settings the core can nearly disappear; high values add density and asymmetric saturation. Agitation contributes a small internal activation term even without a patch cable.

### Tonic and Tones

| Control | Range | Description |
| --- | --- | --- |
| Tonic Coarse | 20 Hz-10 kHz internally | Sets the core oscillator's base frequency |
| Tonic Fine | approximately +/-1 semitone | Fine-tunes the oscillator |
| Tonic Modulation Interference | 0-100% | Sets Tonic modulation depth |
| Tones | 0-100% | Morphs through triangle-like, subharmonic, even-harmonic, folded, pulse, and bright colors |
| Tones CV Attenuverter | -100% to +100% | Scales and optionally inverts Tones CV |

**1V/oct** is summed with Tonic Coarse and Fine. Tonic Modulation is normalled from the Time/Filter feedback network; patching **Tonic Mod** replaces that normalled source.

The raw core oscillator is available at **Tone Core**. A dedicated lower oscillator derived from the core is available at **Sub Harmonics**.

---

## Time, Filter, and Output Sections

### Time and Decay

| Control | Range | Description |
| --- | --- | --- |
| Time Coarse | 0-100% | Sets the main delay range, from a few milliseconds to about 2.45 seconds |
| Time Fine | approximately +/-12% | Trims the Time setting |
| Time Modulation | 0-100% | Sets modulation depth for the normalled or patched Time Mod source |
| Time CV Attenuverter | -100% to +100% | Scales and optionally inverts bipolar Time CV |
| Decay | 0-100% | Sets feedback persistence and drive |
| Decay CV Attenuverter | -100% to +100% | Scales and optionally inverts Decay CV |

**Time Mod** is normalled from the core's subharmonic signal. Patching the jack replaces the normalled source. **Time CV** is bipolar and attenuverted. **Time Unity CV** is a separate 0-10 V input added directly to Time at full scale.

Long Time settings produce separated echoes and unstable pitch movement. Short settings move into resonator, comb-filter, and metallic territory.

### Filter, Absorb, Blend, and Level

| Control | Range | Description |
| --- | --- | --- |
| Filter | 0-100% | Opens the feedback filter from dark damping to bright feedback |
| Filter CV Attenuverter | -100% to +100% | Scales and optionally inverts Filter CV |
| Absorb | 0-100% | Controls how much energy and brightness survive in the feedback path |
| Blend | 0-100% | Equal-power fade between the dry/core signal and Time/Filter result |
| Level | 0-100% | Sets final level before the integrated ambient reverb |

When **Blend CV** is patched, the Blend knob becomes a unipolar attenuator for the 0-5 V input. When **Absorb CV** is patched, the Absorb knob becomes a unipolar attenuator for the 0-10 V input.

High Decay, high Filter, and high Absorb can make the network persist or self-oscillate. Reduce any of those controls if the feedback becomes too strong.

---

## Touch Bridges

The six momentary buttons temporarily inject the selected **Touch source** into related parts of the circuit.

| Touch bridge | Effect while held |
| --- | --- |
| Activate | Adds an activation burst |
| Tonic | Routes the touch source, agitation, and feedback toward pitch |
| Time | Nudges delay time according to the touch source's polarity |
| Decay | Pushes the feedback network toward longer, less stable persistence |
| Filter | Opens the feedback filter |
| Absorb | Reduces absorption, making the tail crumble and destabilize |

With the default **Manual plate** source, a held bridge applies a steady positive gesture. Other sources from the context menu make the bridge respond dynamically while held.

---

## Ambient Reverb

The ambient reverb follows the main Wyrd result and affects both **Modular** and **Line** outputs.

| Control | Range | Description |
| --- | --- | --- |
| Size | 0-100% | Changes the tank from a close haze to a wide ambient space |
| Decay | 0-100% | Sets reverb feedback and tail length |
| Diffusion | 0-100% | Sets input and tank echo density |
| Tone | 0-100% | Sets damping brightness |
| Mod | 0-100% | Sets slow delay-line modulation depth and rate |
| Blend | 0-100% | Equal-power fade from the dry Wyrd result to the reverb |

CV2 motion subtly lengthens, darkens, and modulates the reverb, linking the ambient tail to the Time/Filter network.

---

## Inputs

| Input | Type | Description |
| --- | --- | --- |
| External | Audio/CV | Source for Strength processing and injection into the sound engine |
| Begin and End | Gate | Normalled high; low voltage or a dummy cable stops Agitation |
| Speed CV | 0-5 V CV | Modulates Agitation speed through Speed CV Amount |
| Activation CV | Bipolar CV | Summed through the Activation CV attenuverter |
| Tonic Mod | Bipolar audio/CV | Replaces normalled feedback modulation of Tonic |
| Tones CV | Bipolar CV | Morphs Tones through its attenuverter |
| 1V/oct | Pitch CV | Tracks the Tone Core chromatically |
| Time Mod | Bipolar audio/CV | Replaces normalled subharmonic modulation of Time |
| Time CV | Bipolar CV | Modulates Time through its attenuverter |
| Time Unity CV | 0-10 V CV | Adds directly to Time without an attenuator |
| Decay CV | Bipolar CV | Modulates Decay through its attenuverter |
| Blend CV | 0-5 V CV | Controls Blend; the knob becomes an attenuator |
| Filter CV | Bipolar CV | Modulates Filter through its attenuverter |
| Absorb CV | 0-10 V CV | Controls Absorb; the knob becomes an attenuator |

Wyrd processes the first channel of each input; its jacks and outputs are monophonic.

---

## Outputs and Lights

### Outputs

| Output | Nominal range | Description |
| --- | ---: | --- |
| Strength | approximately +/-10 V | Processed External signal |
| CV1 | 0-10 V | Envelope-followed Strength CV |
| CV2 | +/-5 V | Bipolar Time/Filter feedback CV |
| Agitation | 0-6 V | Internal gesture generator |
| Tone Core | approximately +/-5 V | Raw core oscillator before the Time/Filter blend |
| Sub Harmonics | approximately +/-5 V | Lower oscillator derived from the Tone Core |
| Modular | approximately +/-5 V | Main result with integrated ambient reverb |
| Line | approximately +/-1.5 V | Lower-level copy of the main result |

### Lights

| Light | Meaning |
| --- | --- |
| Strength | Processed External activity |
| CV1 | Envelope level |
| CV2 +/- | Positive or negative Time/Filter feedback |
| Agitation | Current gesture level |
| Activation +/- | Positive or negative activation |
| Result | Modular output activity |
| Reverb | Reverb contribution to the main outputs |

---

## Context Menu

Right-click Wyrd to configure two patch-persistent options.

### Touch Source

| Source | Touch behavior |
| --- | --- |
| Manual plate | Steady positive gesture while a bridge is held |
| CV1 | Bipolarized version of the External envelope |
| CV2 | Current bipolar Time/Filter feedback |
| Agitation | Bipolarized internal gesture |
| Sub harmonics | Tone Core subharmonic waveform |
| Strength | Processed External signal |
| Noise | Internal bipolar noise |

The selected source only affects a touch bridge while its button is held.

### Strength Calibration

| Mode | Use |
| --- | --- |
| Factory | Default gain, saturation, and envelope response |
| Line gentle | More headroom for line-level or already-hot signals |
| Modular hot | Strongest gain and fastest response for lower modular signals or deliberate saturation |

---

## Factory Presets

| Preset | Character |
| --- | --- |
| `00_Glass_Reliquary` | Bright folded tone, long diffused space, and subharmonic touch behavior |
| `01_Black_Tar_Choir` | Dark, slow, persistent low-register feedback voice |
| `02_Haunted_Shortwave` | External-signal processing with strong CV interaction and unstable radio-like motion |
| `03_Clockless_Bloom` | Slow internal Agitation driving a wide, blooming ambient texture |
| `04_Feedback_Seance` | High-feedback, noise-touched network near self-oscillation |
| `05_CV_Spiderweb` | CV1-controlled touch routing with multiple modulation attenuverters ready to patch |

Presets establish a starting state; they do not include external cables. Some are deliberately close to unstable feedback, so lower Level before loading them into a live mix.

---

## Patch Recipes

### Recipe 1: Pitched Ambient Voice

```text
[Pitch sequencer] -> [Wyrd 1V/oct]
[Wyrd Modular]    -> [Mixer]
```

- Activation Constant: 35-55%
- Blend: 35-60%
- Decay: 40-65%
- Filter: 50-75%
- Reverb Blend: 25-50%

Use Tones to move from a rounded core to folded and subharmonic colors. Patch an envelope to Activation CV for articulated notes.

### Recipe 2: External Feedback Processor

```text
[Audio source] -> [Wyrd External]
[LFO]          -> [Wyrd Time CV]
[Wyrd Modular] -> [Mixer]
```

- Strength: raise until the Strength light responds clearly
- External Constant: 50-100%
- Activation Constant: 0-20%
- Blend: 50-80%
- Time CV Attenuverter: start near 10%

Increase Decay gradually. Use CV1 as an envelope elsewhere in the patch.

### Recipe 3: Self-Patching Motion

```text
[Wyrd Agitation] -> [Wyrd Tones CV]
[Wyrd CV2]       -> [Wyrd Filter CV]
[Wyrd Modular]   -> [Mixer]
```

Keep both attenuverters low at first. Change Agitation Speed and Angle for slow timbral cycles; switch Touch Source to Agitation for animated bridge gestures.

### Recipe 4: Clocked Agitation Gate

```text
[Gate pattern]   -> [Wyrd Begin and End]
[Wyrd Agitation] -> [External modulation destination]
```

Agitation runs while the gate is high and pauses while it is low. Audio-rate Speed settings can turn this path into an additional oscillator-like modulation source.

### Recipe 5: Resonator and Karplus-Like Plucks

```text
[Trigger or short noise burst] -> [Wyrd External]
[Wyrd Modular]                 -> [Mixer]
```

Use short Time, moderately high Filter and Absorb, and raise Decay until the burst rings. Tonic and the touch bridges can destabilize or retune the result, but Time is the primary resonant pitch control in this patch.

---

## Troubleshooting

### No sound

- Raise **Level**; its default is fully down.
- For the internal voice, raise **Activation Constant**.
- For external processing, patch **External** and raise both **Strength** and **External Constant**.
- Confirm that **Modular** or **Line** is connected to the next module.

### Agitation is not moving

- Remove the **Begin and End** cable, or send it a high gate.
- A dummy cable counts as a connected low input and stops Agitation.
- Raise Speed if the cycle is too slow to notice.

### CV has no effect

- Raise the corresponding attenuverter or CV amount.
- **Blend CV** and **Absorb CV** multiply their panel controls; turn the knob up after patching.
- Patching **Tonic Mod** or **Time Mod** replaces its internal normalled source.

### Feedback is too loud or will not decay

- Reduce **Decay**, **Filter**, **Absorb**, or **Blend**.
- Lower **Level** before exploring the upper control ranges.
- Reduce External Constant if the input is repeatedly driving the network.

### Pitch is unstable

- Reduce Tonic Modulation Interference, Activation Interference, Decay, or Time modulation.
- Release the Tonic touch bridge.
- Use **Tone Core** for the cleanest direct pitched output.

### External input is too weak or too saturated

- Choose a different **Strength calibration** in the context menu.
- Use **Line gentle** for hot sources and **Modular hot** for more gain.

---

## Technical Notes

- Wyrd is monophonic.
- The tone core uses band-limited transitions where needed and internally limits pitch to a safe range.
- Tonic Coarse follows an exponential frequency curve; the 1V/oct input is added in pitch space.
- The Time/Filter network provides up to approximately 2.45 seconds of delay and includes multiple taps, diffusion, damping, nonlinear feedback, wow, and low-level noise.
- Blend and Reverb Blend use equal-power crossfades.
- Modular output is limited to approximately +/-5.2 V; Line is a lower-level copy limited to approximately +/-1.6 V.
- Context-menu selections are stored with the patch.
- Reset clears the oscillator, feedback, reverb, envelope, and modulation state without changing front-panel parameters.
