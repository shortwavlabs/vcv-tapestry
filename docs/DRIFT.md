# Drift User Manual

**3HP fluctuating random LFO with offset, range, Signal B blending, jitter, and occasional voltage slips**

Drift generates smooth random control voltage that moves between changing targets instead of stepping abruptly. Its motion includes curved glides, slow interference, subtle jitter, and occasional short voltage slips. A bipolar **Signal B** attenuverter can crossfade the random source toward another CV or its inverted form.

---

## Table of Contents

1. [Overview](#overview)
2. [First Patch](#first-patch)
3. [Signal Flow](#signal-flow)
4. [Controls](#controls)
5. [Input, Output, and Lights](#input-output-and-lights)
6. [Patch Recipes](#patch-recipes)
7. [Troubleshooting](#troubleshooting)
8. [Technical Notes](#technical-notes)

---

## Overview

Drift is a compact modulation source for patches that need movement without a repeating waveform. It is useful for:

- Slow oscillator tuning drift
- Filter, wavefolder, and effect animation
- Evolving envelope or sequencer parameters
- Random stereo movement with two Drift modules
- Blending a predictable LFO with controlled uncertainty
- Adding rare slips and jitter to otherwise stable CV

The internal random motion is deterministic after reset, making a saved patch repeatable from the same initial state while still sounding irregular.

---

## First Patch

1. Add **Drift** to your patch.
2. Patch **CV** to an oscillator's fine-tune, a filter cutoff CV input, or another attenuated modulation destination.
3. Set **Offset** to the center.
4. Set **Range** near 25%.
5. Set **Speed** near 35%.
6. Leave the **Signal B** attenuverter centered.
7. Use the destination's attenuator to limit modulation further if needed.

```text
[Drift CV] -> [Attenuator or destination CV input]
```

The output can reach broad voltage ranges. Start with low Range when patching pitch, delay time, feedback, or another sensitive destination.

---

## Signal Flow

```text
Random target generator
  -> curved glide
  -> amplitude variation
  -> slow/fast interference
  -> jitter and occasional slips
  -> crossfade with Signal B or inverted Signal B
  -> Range
  -> Offset
  -> CV output
```

**Range** is applied after the internal random source and Signal B are mixed. **Offset** is added last.

---

## Controls

| Control | Range | Description |
| --- | --- | --- |
| Offset | -5 V to +5 V | Adds a fixed voltage after Range |
| Speed | approximately 0.01-6 Hz target rate | Sets how quickly the random source chooses and reaches new targets |
| Range | 0-10 V peak-to-peak nominal | Sets the bipolar depth of the mixed signal |
| Signal B Attenuverter | -100% to +100% | Crossfades between Drift and Signal B; negative settings invert Signal B |

### Offset

At center, Drift moves around 0 V. Turn clockwise to bias the output positive or counterclockwise to bias it negative. With Range at zero, Offset acts as a fixed -5 V to +5 V source.

### Speed

Speed changes the random target rate nonlinearly:

- Low settings produce long, uncertain movement.
- Middle settings produce conventional fluctuating-random modulation.
- High settings create animated CV with more frequent jitter and shorter slips.

Speed does not turn Drift into a periodic oscillator. Segment length, bend, amplitude, and interference continue to vary.

### Range

Range controls how far the signal moves around Offset. Its response is shaped for finer adjustment near zero. At maximum, the internal random source is nominally scaled to a 10 V peak-to-peak span, though interference and slips can extend it slightly beyond that before Offset is added.

### Signal B Attenuverter

The attenuverter is a crossfader, not an additive mixer:

| Position | Result |
| --- | --- |
| Center | Drift only |
| Fully clockwise | Signal B only |
| Between center and clockwise | Crossfade from Drift to Signal B |
| Fully counterclockwise | Inverted Signal B only |
| Between center and counterclockwise | Crossfade from Drift to inverted Signal B |

If Signal B is unpatched, the attenuverter has no effect and Drift remains the source.

---

## Input, Output, and Lights

### Input

| Input | Type | Description |
| --- | --- | --- |
| Signal B | Audio/CV | Alternate signal for the bipolar crossfade; nominally scaled around +/-5 V |

Signal B is useful with another random source, a sequencer row, envelope, LFO, or fixed voltage. Audio can be patched, but Drift is designed primarily as a CV source.

### Output

| Output | Type | Description |
| --- | --- | --- |
| CV | Monophonic CV | Offset and ranged random motion, optionally crossfaded toward Signal B |

The final output is safely limited to +/-10 V.

### Lights

| Light | Meaning |
| --- | --- |
| Green | Positive output voltage |
| Red | Negative output voltage |
| Yellow | Internal jitter and voltage-slip activity |

The yellow light does not show overall output level. It highlights the small irregular events layered onto the main random glide.

---

## Patch Recipes

### Recipe 1: Analog-Style Pitch Drift

```text
[Drift CV] -> [Attenuator] -> [Oscillator FM input]
```

- Offset: center
- Speed: 10-25%
- Range: 5-15%
- Signal B Attenuverter: center

Use strong attenuation at the destination. A little pitch movement is usually enough.

### Recipe 2: Wandering Filter

```text
[Drift CV] -> [Filter cutoff CV]
```

- Offset: set the center of the desired cutoff motion
- Speed: 25-50%
- Range: 20-60%

The Offset knob can act as the filter's macro position while Range controls how far it wanders.

### Recipe 3: Stable LFO with Uncertainty

```text
[Triangle LFO] -> [Drift Signal B]
[Drift CV]     -> [Modulation destination]
```

Turn the Signal B attenuverter slightly clockwise. Near center, the result is mostly random; farther clockwise, the triangle becomes clearer while retaining Drift's influence. Turn counterclockwise for an inverted triangle.

### Recipe 4: Random Unipolar Modulation

```text
[Drift CV] -> [Unipolar CV destination]
```

Set Offset above center and reduce Range until the red output light stays off. Drift does not hard-limit the signal to a unipolar range, so leave headroom for rare slips.

### Recipe 5: Correlated Stereo Motion

```text
[Drift A CV] -> [Drift B Signal B]
[Drift A CV] -> [Left parameter]
[Drift B CV] -> [Right parameter]
```

Set Drift B's Signal B attenuverter near, but not at, either extreme. The two destinations share some movement while Drift B retains independent random motion.

---

## Troubleshooting

### No visible movement

- Raise **Range**; at zero the output is only the Offset voltage.
- Raise **Speed** if the random glide is too slow to notice.
- Check the green and red output lights.

### Signal B does not affect the output

- Patch Signal B and move its attenuverter away from center.
- At center, Drift is intentionally 100% of the mix.
- Confirm the Signal B source is producing voltage.

### Output is too wide or clips a destination

- Reduce **Range**.
- Move **Offset** toward center.
- Add an attenuator before sensitive destinations.
- Drift itself limits its output to +/-10 V, but a destination may expect a smaller range.

### Output is mostly positive or negative

- Return **Offset** to center.
- If Signal B is patched, check its DC offset and the attenuverter position.

### The yellow light flashes unexpectedly

This is normal. It reports the internal jitter and occasional voltage slips that give Drift its irregular character.

---

## Technical Notes

- Drift is monophonic and always emits one CV channel.
- The random target rate spans approximately 0.01-6 Hz.
- New targets vary in position, curve, and amplitude.
- Low-level slow and fast interference are always present.
- Range uses a smooth nonlinear curve for finer low-depth control.
- Signal B is normalized around +/-5 V before the crossfade.
- Internal motion can extend slightly beyond its nominal range; the final output is limited to +/-10 V.
- Reset restores the deterministic random sequence and clears smoothing state without changing panel parameters.
