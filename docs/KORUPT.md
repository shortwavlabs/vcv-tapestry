# Korupt User Manual

**Standalone PLL harmonizer, square-wave fuzz, and subharmonic generator for VCV Rack**

Korupt is a monophonic-per-channel effect inspired by analog PLL harmonizer pedals and CMOS divider circuits. It converts incoming audio into a one-bit square voice, locks a voltage-controlled master oscillator to that signal, and derives subharmonics from either the input square or the oscillator. The result ranges from tight octave fuzz to unstable divider chatter, pitch-glide laser tones, and broken synth stacks.

Korupt works best when each channel carries a single pitched source. It can process polyphonic cables, but each channel is tracked independently as its own monophonic signal.

---

## Table of Contents

1. [Overview](#overview)
2. [First Patch](#first-patch)
3. [Signal Flow](#signal-flow)
4. [Controls](#controls)
5. [Inputs](#inputs)
6. [Outputs and Lights](#outputs-and-lights)
7. [Factory Presets](#factory-presets)
8. [Patch Recipes](#patch-recipes)
9. [Tracking Tips](#tracking-tips)
10. [Troubleshooting](#troubleshooting)
11. [Technical Notes](#technical-notes)

---

## Overview

Korupt behaves more like a circuit that is trying to follow your signal than a clean pitch shifter. It wants a strong monophonic input, especially if you want stable tracking. Feed it complex chords, fast transients, or weak levels and it will respond with jumps, missed locks, divider artifacts, and hard-edged CMOS fuzz.

### Main Voices

| Voice | Source | Character |
| --- | --- | --- |
| Square | Input audio through the one-bit shaper | Raw square-wave fuzz at the input pitch |
| Oscillator | PLL master oscillator | Upper harmonics, octaves, fifths, thirds, and unstable lock behavior |
| Subharmonic | Counter divider | Lower square pulses derived from the input or oscillator |

### Best Uses

- Fuzz bass and guitar-style monophonic processing
- Synth leads with harmonized upper voices
- Sub-octave reinforcement
- Rhythmic CMOS divider patterns
- Glitchy tracking artifacts from drums, vocals, and complex modular patches
- Per-channel polyphonic processing when each channel carries a separate monophonic voice

---

## First Patch

### Basic Setup

1. Add **Korupt** to your patch.
2. Patch a monophonic oscillator, guitar input, bass line, or simple sample into **IN**.
3. Patch **OUT** to your mixer or audio interface.
4. Start with the preset **00_Classic_PLL_Fuzz**.
5. Play single notes and adjust **Square**, **Sub**, **Oscillator**, and **Level**.

Basic patch:

```text
[Monophonic source] -> [Korupt IN]
[Korupt OUT]       -> [Mixer / Audio Interface]
```

### Good Initial Settings

| Control | Starting value |
| --- | --- |
| Square | 60-70% |
| Subharmonic | 40-50% |
| Oscillator | 70-80% |
| Level | 50-60% |
| Rate | 35-50% |
| Frequency Modulator | Glide |
| Master Oscillator Root | Unison |
| Master Oscillator Program | 3 |
| Subharmonic Root | Input |
| Subharmonic Program | 2 |

---

## Signal Flow

Korupt is organized like the original pedal-style circuit:

```text
Audio In
  -> input conditioning
  -> CMOS square shaper
  -> Square voice
  -> Root divider
  -> 4046-style PLL master oscillator
  -> Oscillator voice
  -> 4017-style subharmonic divider
  -> Subharmonic voice
  -> Voice mixer
  -> output saturation/filtering
  -> Audio Out
```

The three voice level controls are not normalized. Turning up all three voices drives the output stage harder, much like pushing a resistor mixer into a saturating analog stage.

---

## Controls

### Voice Mixer

| Control | Range | Description |
| --- | --- | --- |
| Square | 0-100% | Level of the input-derived square fuzz voice |
| Subharmonic | 0-100% | Level of the lower divider voice |
| Oscillator | 0-100% | Level of the PLL master oscillator voice |
| Level | 0-100% | Final output level after mixing, clipping, DC blocking, and filtering |

The voice mixer can be subtle or abusive. Low settings isolate individual voices. High settings let the voices fight and saturate together.

### Subharmonic Section

| Control | Range | Description |
| --- | --- | --- |
| Subharmonic Program | 1-8 | Selects the divider program |
| Subharmonic Root | Input / Oscillator | Chooses whether the sub divider follows the input square or the master oscillator |

Subharmonic programs:

| Program | Divider | Musical result relative to selected sub root |
| --- | ---: | --- |
| 1 | /2 | 1 octave down |
| 2 | /3 | 1 octave down plus fifth relationship |
| 3 | /4 | 2 octaves down |
| 4 | /5 | 2 octaves down plus major-third relationship |
| 5 | /6 | 2 octaves down plus fifth relationship |
| 6 | /7 | Minor-seventh-ish divider color |
| 7 | /8 | 3 octaves down |
| 8 | /9 | 3 octaves down plus major-second relationship |

**Input root** is usually more stable and works well for bass reinforcement. **Oscillator root** follows the PLL voice, so glide and vibrato can ripple into the subharmonic output.

### Frequency Modulator

| Control | Range | Description |
| --- | --- | --- |
| Mode | Glide / Vibrato | Chooses how Rate affects the master oscillator |
| Rate | 0-100% | Glide speed in Glide mode, vibrato speed in Vibrato mode |

In **Glide** mode, lower Rate settings smear note changes and make the oscillator lag behind the input. Higher settings track faster.

In **Vibrato** mode, Rate controls the modulation speed injected into the PLL control node. The modulation affects the master oscillator, and it also affects the subharmonic voice when Subharmonic Root is set to Oscillator.

### Master Oscillator Section

| Control | Range | Description |
| --- | --- | --- |
| Master Oscillator Program | 1-8 | Selects the PLL feedback multiplier |
| Master Oscillator Root | Unison / -1 / -2 | Chooses the reference octave feeding the PLL |

Master Oscillator programs:

| Program | Multiplier | Musical result relative to selected root |
| --- | ---: | --- |
| 1 | 1x | Unison |
| 2 | 2x | 1 octave up |
| 3 | 3x | 1 octave up plus fifth |
| 4 | 4x | 2 octaves up |
| 5 | 5x | 2 octaves up plus major third |
| 6 | 6x | 2 octaves up plus fifth |
| 7 | 7x | Minor-seventh-ish harmonic color |
| 8 | 8x | 3 octaves up |

Master Oscillator Root changes the pitch range before the multiplier. Use **Unison** for bright upper voices, **-1** for more grounded octave harmonies, and **-2** for slower, wobblier PLL response.

---

## Inputs

| Input | Type | Description |
| --- | --- | --- |
| IN | Audio | Main audio input. Designed for +/-5V Rack audio. Polyphonic input is processed per channel. |
| SQ CV | CV | Adds to the Square voice level. 0-10V covers the full knob range. |
| SUB CV | CV | Adds to the Subharmonic voice level. 0-10V covers the full knob range. |
| OSC CV | CV | Adds to the Oscillator voice level. 0-10V covers the full knob range. |
| RATE | CV | Adds to the Rate control. 0-10V covers the full knob range. |
| SUB P | CV | Quantized CV offset for Subharmonic Program. 0-10V spans programs 1-8. |
| OSC P | CV | Quantized CV offset for Master Oscillator Program. 0-10V spans programs 1-8. |

CV inputs are additive and clamped to the legal control range. For stepped program inputs, CV is quantized after being added to the front-panel program selector.

---

## Outputs and Lights

### Outputs

| Output | Type | Description |
| --- | --- | --- |
| OUT | Audio | Mixed Korupt output |
| SQ | Audio | Isolated Square voice |
| SUB | Audio | Isolated Subharmonic voice |
| OSC | Audio | Isolated Oscillator voice |
| LOCK | CV | 0-10V lock-confidence output from the PLL model |

The isolated voice outputs are useful for external mixing, filtering, logic processing, or sending each voice to a different effects chain.

### Lights

| Light | Meaning |
| --- | --- |
| TRK | Input tracking/envelope activity |
| LCK | PLL is near lock |
| GL | Tracking instability, lock error, or chaotic divider behavior |

Lights show the highest activity across all polyphonic channels.

---

## Factory Presets

Korupt includes factory presets in the module preset menu.

| Preset | Character |
| --- | --- |
| `00_Classic_PLL_Fuzz` | Balanced square, oscillator, and sub voices |
| `01_Square_Only_Tracker` | Raw input-derived square fuzz |
| `02_Fifths_Above_Below` | Fifth-based upper and lower divider stack |
| `03_Slow_Glide_Octave_Bloom` | Slow PLL glide with octave movement |
| `04_Three_Octave_Laser` | High oscillator program with fast vibrato |
| `05_Doom_Divider` | Heavy low subharmonic setting |
| `06_Sub_From_Oscillator` | Sub divider follows the PLL oscillator |
| `07_Broken_Organ_Stack` | Dense upper/lower square organ texture |
| `08_Vibrato_Chip_Choir` | Vibrato-driven stacked chip harmonies |
| `09_Unstable_Ringdown` | Intentionally high, twitchy, unstable tracking |

---

## Patch Recipes

### Recipe 1: Bass Reinforcer

**Goal**: Add a strong lower voice under a mono bass line.

```text
[Bass voice] -> [Korupt IN]
[Korupt OUT] -> [Mixer]
```

Settings:

| Control | Value |
| --- | --- |
| Square | 20-35% |
| Subharmonic | 80-100% |
| Oscillator | 20-40% |
| Subharmonic Root | Input |
| Subharmonic Program | 1 or 3 |
| Master Oscillator Root | -1 |
| Rate | 40-60% |

Use a low-pass filter after Korupt if you want the sub voice to sit underneath the dry bass instead of dominating the patch.

### Recipe 2: Glide Lead

**Goal**: Make a monophonic oscillator smear into PLL harmonies.

Settings:

| Control | Value |
| --- | --- |
| Square | 35% |
| Subharmonic | 45% |
| Oscillator | 85% |
| Frequency Modulator | Glide |
| Rate | 10-25% |
| Master Oscillator Program | 4 or 5 |
| Master Oscillator Root | Unison |
| Subharmonic Root | Oscillator |

Lower Rate values exaggerate the slide. If the PLL falls apart too often, raise Rate or simplify the input.

### Recipe 3: Vibrato Laser

**Goal**: Get bright, modulated upper harmonics.

Settings:

| Control | Value |
| --- | --- |
| Square | 10-25% |
| Subharmonic | 10-25% |
| Oscillator | 90-100% |
| Frequency Modulator | Vibrato |
| Rate | 65-90% |
| Master Oscillator Program | 7 or 8 |
| Master Oscillator Root | Unison |

Patch **LOCK** to a scope or meter to see how the modulation affects PLL confidence.

### Recipe 4: External Voice Mixer

**Goal**: Treat Korupt as three separate voices.

```text
[Source]      -> [Korupt IN]
[Korupt SQ]  -> [Filter 1] -> [Mixer Ch 1]
[Korupt SUB] -> [Filter 2] -> [Mixer Ch 2]
[Korupt OSC] -> [Delay]    -> [Mixer Ch 3]
```

Set Korupt's main voice levels however you like for **OUT**, or ignore **OUT** and use only the isolated outputs.

---

## Tracking Tips

Korupt is intentionally sensitive to input shape. These tips help when you want more stable tracking:

1. Use monophonic sources.
2. Keep input around normal Rack audio level, roughly +/-5V.
3. Prefer simple waveforms, bass, guitar, voice, or single-note samples.
4. Avoid full chords if you want predictable oscillator intervals.
5. Raise Rate in Glide mode for faster lock.
6. Use Master Oscillator Root `Unison` before trying `-1` or `-2`.
7. Use Subharmonic Root `Input` for a more stable low voice.

For more chaotic behavior, do the opposite: feed drums, chords, noisy material, or quickly changing CV-controlled program selections.

---

## Troubleshooting

### I hear no output

- Confirm **IN** is receiving audio.
- Raise at least one of **Square**, **Subharmonic**, or **Oscillator**.
- Raise **Level**.
- Check whether you are listening to **OUT** or an isolated voice output.

### The oscillator is not tracking cleanly

This is often expected. Korupt uses edge tracking and a PLL model, not a clean pitch detector.

Try:

- Use a simpler monophonic input.
- Increase input level before Korupt.
- Increase **Rate** in Glide mode.
- Set Master Oscillator Root to **Unison**.
- Reduce very fast changes to Oscillator Program CV.

### The subharmonic is too clicky or sparse

The sub voice is counter-based, so some divider programs produce pulse-like patterns.

Try:

- Use lower-numbered Subharmonic Programs for stronger octave behavior.
- Set Subharmonic Root to **Input** for more stable pulses.
- Filter the **SUB** output externally.

### The output distorts when all voices are high

That is part of the design. Korupt does not normalize the voice mixer, so stacked voices drive the output stage.

Try:

- Lower individual voice levels.
- Lower **Level**.
- Use isolated outputs and mix externally.

### Polyphony sounds strange

Korupt processes each polyphonic channel independently, but each channel is still monophonic. A polyphonic chord on one channel will not track like several independent notes. Use one note per polyphonic channel for best results.

---

## Technical Notes

Korupt models the major functional blocks of a pedal-style PLL harmonizer circuit:

- Input coupling, filtering, and CMOS inverter-style shaping
- Rack SDK `dsp::SchmittTrigger` edge detection in the module wrapper
- 4024-style root division
- 4046-style phase-frequency detector, charge-pump-like loop behavior, and VCO control node
- 4017-style program counters for feedback and subharmonic division
- Non-normalized voice mixing
- Output saturation, DC blocking, and filtering using Rack SDK DSP utilities

The module is a faithful behavioral model, not a literal component-level SPICE simulation. Its goal is to preserve the playable behavior of the circuit: hard square voices, PLL lock and unlock, musical divider ratios, and useful instability.

---

## Next Steps

- Try the factory presets from the module preset menu.
- Use **LOCK** and **GL** to learn how different sources affect PLL tracking.
- Patch the isolated voice outputs into different filters, wavefolders, or delays.
