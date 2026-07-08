# Corruption Pedal VCV Module Research

This note summarizes how to implement the Corruption Device pedal as a VCV Rack module in this plugin.

## Source Material

- Local schematic/build PDF: `/Users/shortwavlabs/Documents/schematics/Corruption-Device-1v1-Building-Docs.pdf`
- Original pedal manual, R4 web PDF: `/Users/shortwavlabs/Downloads/EQD-Data-Corrupter-Manual-R4-WEB.pdf`
- Reference interface screenshot: `/var/folders/b7/m6w219tj07n4wfc_776w591h0000gn/T/codex-clipboard-c3e96e87-0c80-4f17-ba5a-a235dfe54aca.png`
- VCV Rack SDK docs via Context7: `/vcvrack/rack`
- EarthQuaker Devices Data Corrupter product/manual pages:
  - https://www.earthquakerdevices.com/data-corrupter
  - https://eqd.squarespace.com/s/EQD-EU-Data-Corrupter-R3.pdf
- EarthQuaker Devices "Decoding the Data Corrupter":
  - https://www.earthquakerdevices.com/blog-posts/aarons-bass-hole-decoding-the-data-corrupter
- DAFx 2024 paper, "Digitizing the Schumann Electronics PLL Analog Harmonizer":
  - https://www.dafx.de/paper-archive/2024/papers/DAFx24_paper_25.pdf

## Executive Summary

The Corruption Device is not best modeled as a simple bit crusher. The PCB Guitar Mania build docs identify it as a pedal based on the EQD Data Corrupter, which is a Schumann PLL-style monophonic analog harmonizer. The audible behavior comes from one-bit signal tracking, CMOS logic, PLL multiplication, counter division, and an analog summing/output stage.

The Rack module should therefore be a monophonic or per-poly-channel PLL synth/fuzz effect with three voices:

- Square: input-derived square-wave fuzz at the original input octave.
- Oscillator: PLL-generated upper voice, selected by root and 8-position multiplier.
- Sub: divided lower voice, derived either from the square input or from the oscillator.

Recommended implementation: create a new standalone-only module named `Korupt` with audio input/output, per-voice outputs, CV-friendly controls, a pedal-inspired interface, and a pure DSP engine in `src/dsp/korupt-dsp.h`. Do not implement it as a Tapestry expander; the source pedal is a self-contained mono effect, so standalone is the intended product shape.

## Confirmed Product Decisions

- Module name: `Korupt`.
- Module type: standalone only, not a Tapestry expander.
- Width: 20HP is acceptable and preferred for legible pedal-style grouping.
- Panel direction: combine the existing Shortwav/Tapestry SVG style with the original pedal's control layout.
- DSP target: faithful recreation of the original pedal's PLL behavior, including its monophonic tracking quirks and chaotic edge cases.

## Circuit Findings From The PDF

The schematic and BOM show these major blocks:

| Hardware block | Parts in PDF | DSP role |
| --- | --- | --- |
| Input bias/filter/buffer | TL072, 1M input impedance, VR bias, coupling caps | Normalize Rack audio, remove DC, emphasize the fundamental for tracking |
| CMOS square shaper | 4069N inverter chain, 40106N Schmitt inverter | Convert input into a one-bit square signal with hysteresis and glitchy gating |
| Root divider | 4024N binary counter and OSC-ROOT switch | Select oscillator reference root: unison, one octave down, or two octaves down |
| PLL | 4046N with loop filter, RATE pot, GLIDE switch | Lock a VCO-like square oscillator to the selected root |
| Program counters | Two 4017N decade counters and 1P8T rotaries | Select oscillator multiplication and subharmonic division programs |
| Mixer/output | SQUARE, OSC, SUB, LEVEL pots, TL072 output stage | Blend three square voices, filter/soft-limit, scale to Rack voltage |

The local Corruption PDF lists these user controls:

- Potentiometers: `LEVEL`, `RATE`, `SQUARE`, `OSC`, `SUB`
- Switches/selectors: `OSC-ROOT`, `OSC-SW`, `SUB-SW`, `SUB-ROOT`, `GLIDE`

## Expected Pedal Behavior

The EQD manual describes the same family behavior:

- The effect expects monophonic, strong, front-of-chain input for best tracking.
- The square fuzz voice is the input converted to a hard square-wave voice.
- The Master Oscillator root chooses the input octave sent into the PLL: unison, -1 octave, or -2 octaves.
- The Master Oscillator rotary chooses one of eight multiplication intervals over three octaves.
- The Frequency Modulator has two confirmed modes: `Glide` and `Vibrato`. `Rate` controls glide speed in Glide mode and vibrato speed in Vibrato mode.
- The Subharmonic section chooses one of eight lower divisions. Its root switch chooses whether the sub is derived from the square input or from the oscillator.

The useful interval tables are:

| Osc program | Multiplier | Musical result relative to selected root |
| --- | ---: | --- |
| 1 | 1x | unison |
| 2 | 2x | +1 octave |
| 3 | 3x | +1 octave + fifth |
| 4 | 4x | +2 octaves |
| 5 | 5x | +2 octaves + major third |
| 6 | 6x | +2 octaves + fifth |
| 7 | 7x | +2 octaves + minor seventh-ish harmonic |
| 8 | 8x | +3 octaves |

| Sub program | Divider | Musical result relative to selected sub root |
| --- | ---: | --- |
| 1 | /2 | -1 octave |
| 2 | /3 | -1 octave + fifth below |
| 3 | /4 | -2 octaves |
| 4 | /5 | -2 octaves + major third below |
| 5 | /6 | -2 octaves + fifth below |
| 6 | /7 | -2 octaves + minor seventh-ish below |
| 7 | /8 | -3 octaves |
| 8 | /9 | -3 octaves + major second below |

## Recommended DSP Model

The DAFx Schumann PLL paper supports a practical digital model: use simple virtual analog filtering for the input/output analog stages, and model CMOS gates/counters/PLL behavior with conditional logic. That is a good fit for Rack's per-sample `process()` callback.

### Stage 1: Input conditioning

Normalize Rack audio from volts to `[-1, 1]`. Because Rack audio is generally +/-5V, use:

```cpp
float in = inputs[AUDIO_INPUT].getPolyVoltage(c) / 5.0f;
```

Then apply:

- DC blocker, matching the existing `TapestryExpander` style.
- Gentle band-pass or high-pass plus low-pass shaping before tracking.
- Fixed input gain/tracking behavior calibrated for Rack levels. The original pedal intentionally has no user gain control, so avoid a front-panel `Track` or `Input` trim unless testing proves an advanced context-menu calibration is necessary.

### Stage 2: One-bit square shaper

Use Rack's built-in `rack::dsp::SchmittTrigger` as the first-pass one-bit shaper and edge detector. It already provides hysteresis and supports explicit low/high thresholds via `process(input, low, high)`, so we should avoid writing a custom trigger class unless we later need behavior that the SDK utility cannot express.

The square shaper should:

- Use a per-channel `dsp::SchmittTrigger` instance.
- Convert rising-edge trigger events into the logic clock for the 4024/4046/4017 model.
- Maintain a separate one-bit square state for audio output, since `SchmittTrigger::process()` reports edge events rather than being a full audio waveform source.
- Use fixed low/high thresholds calibrated for the conditioned audio path. If the stock 0V/1V-style gate thresholds do not suit normalized audio, tune internal thresholds rather than adding a main-panel control.

Use this output for the Square voice and as the timing source for the logic chain. Add an envelope/noise gate before the Schmitt trigger so silence does not chatter.

### Stage 3: Root division

Model the 4024 root divider as edge-triggered flip-flops:

- `OSC_ROOT` unison: reference = square edge stream.
- `OSC_ROOT` -1: reference = divide-by-2 edge stream.
- `OSC_ROOT` -2: reference = divide-by-4 edge stream.

This should be an edge/counter model, not a pitch detector. It preserves the pedal's monophonic and sometimes unstable tracking behavior.

### Stage 4: 4046 PLL

The most faithful approach is a digital 4046 type-II phase-frequency detector:

- Detect rising edges from reference input and feedback input.
- Set `Qup` when reference leads.
- Set `Qdown` when feedback leads.
- Reset both when both have fired.
- Feed `Qup/Qdown/high-Z` into a one-pole or state-space loop filter.
- Use filtered control voltage to advance a numerically controlled oscillator phase.
- Convert the oscillator phase to a one-bit square.

This is more authentic than directly estimating pitch and setting an oscillator. Do not ship a polished "stable tracking" mode in the first version; a pitch-estimator model is acceptable only as a temporary test scaffold or private debug comparison while building the faithful PLL model.

### Stage 5: 4017 counters

Use two edge counters:

- Oscillator program: feedback divider/multiplier table `1..8`. In a PLL, dividing the VCO feedback by `N` makes the VCO lock near `N * reference`.
- Sub program: output divider table `2..9`.

For the sub root:

- `Unison`: divide the input-derived square/root path. This is the more stable lower octave behavior.
- `Oscillator`: divide the PLL oscillator output. This makes subharmonics inherit glide/modulation and creates the unstable interactive behavior.

### Stage 6: Frequency modulation

Implement `RATE` as loop-filter slew or modulation rate, depending on mode:

- `Glide`: slew the PLL control frequency so note changes smear into each other. Higher rate should track faster.
- `Vibrato`: add triangle/sine modulation to the PLL control frequency for a laser-like pitch wobble. In the original manual, Frequency Modulator affects the Master Oscillator only unless Subharmonic Root is set to `Oscillator`, in which case the subharmonic voice inherits the modulation.

### Stage 7: Mixer and output

Blend the three one-bit voices:

```cpp
float mixed = squareMix * squareVoice
            + oscMix * oscVoice
            + subMix * subVoice;
```

Then:

- Apply an output low-pass filter to tame aliasing and mimic the summing/output stage.
- Apply soft saturation or single-supply-style limiting.
- Scale back to Rack voltage with a conservative ceiling, for example `clamp(out * level * 5.0f, -10.0f, 10.0f)`.
- Guard against NaN/Inf before writing outputs.

Because this module generates hard square transitions, aliasing needs attention. Recommended first pass: 4x oversampling inside the DSP engine plus post-mix low-pass filtering. Later, replace hard edge generation with minBLEP/polyBLEP transitions if needed.

## Rack Module Design

Recommended module name: `Korupt`

Recommended width: 20HP. The pedal-inspired interval tables, rotary selectors, and section labels need the extra width to stay legible in Rack.

### Panel/UI Direction

The attached screenshot gives the strongest control-layout target. The existing SVGs (`res/TAPESTRY_PANEL2.svg` and `res/TAPESTRY_EXPANDER.svg`) establish the Shortwav visual language: medium gray base panel, pale angular geometric overlays, charcoal/black vector labels, restrained monochrome contrast, and a clean vertical-module discipline. `Korupt` should merge that Shortwav shell with the original pedal's boxed control grammar, not become a literal branded copy.

- Medium gray Shortwav panel base with pale diagonal/geometric panel bands and dark charcoal linework.
- Heavy section labels with thin boxed/rounded outlines around each functional block, adapted to Rack's vertical proportions.
- Top `Voice Mixer` band with three equal knobs: `Square`, `Subharmonic`, `Oscillator`.
- Large `Level` knob near the upper right, visually separated from the voice mixer.
- Lower-left `Subharmonic` block with a large snapped 8-position rotary, `Octave/Interval` lookup table, and `Root` toggle between `Unison` and `Oscillator`.
- Center `Frequency Modulator` block with a two-position `Glide/Vibrato` toggle and `Rate` knob.
- Lower-right `Master Oscillator` block with a large snapped 8-position rotary, `Octave/Interval` lookup table, and `Root` selector for `Unison`, `-1`, and `-2`.
- Put Rack jacks in a bottom utility strip or narrow side rail so the pedal face remains readable. Audio input/output can be larger or emphasized; CV inputs and individual voice outs should be grouped under the related sections.
- Avoid EQD logos, the exact product name, and a pixel-perfect reproduction of the artwork. The goal is "inspired by the pedal control grammar" while staying clearly Shortwav/Tapestry-family.

Suggested 20HP Rack layout:

| Area | Controls |
| --- | --- |
| Top-left, boxed | `Square`, `Subharmonic`, `Oscillator` mixer knobs |
| Top-right | Large `Level` knob |
| Middle-left | `Sub Program` snapped rotary, sub interval table, `Sub Root` toggle |
| Middle-center | `Glide/Vibrato` switch, `Rate` knob, optional lock/tracking LEDs |
| Middle-right | `Osc Program` snapped rotary, oscillator interval table, `Osc Root` 3-position selector |
| Bottom row | `In`, `Out`, `Square Out`, `Osc Out`, `Sub Out`, `Lock Out`, CV inputs |

Implementation notes for the panel:

- Use a custom SVG panel at `res/KORUPT.svg`.
- Convert all panel text to paths before release.
- Use a `components` layer if generating placements from SVG markers.
- Use `RoundBlackKnob` or `Davies1900hLargeBlackKnob` as the first pass; custom scalloped knobs can come later if we want the screenshot's knob silhouette.
- Use `RoundBlackSnapKnob` for the 8-position program selectors and `CKSS`/`CKSSThree` for the toggle switches.
- Printed interval tables should be display-only panel art; the Rack parameters remain snapped numeric params with descriptive labels in `configParam()`.
- Keep the Shortwav/Tapestry habit of converted vector text and subtle geometric background shapes; use the pedal screenshot for hierarchy and labeling, not for exact logo/art replication.

### Parameters

| Param | Range | Notes |
| --- | --- | --- |
| `LEVEL_PARAM` | 0..2 | Output gain |
| `RATE_PARAM` | 0..1 | PLL/glide/modulation rate |
| `SQUARE_PARAM` | 0..1 | Square voice mix |
| `OSC_PARAM` | 0..1 | Oscillator voice mix |
| `SUB_PARAM` | 0..1 | Sub voice mix |
| `OSC_ROOT_PARAM` | snapped 0..2 | unison, -1, -2 |
| `OSC_PROGRAM_PARAM` | snapped 0..7 | multiplier table 1..8 |
| `SUB_ROOT_PARAM` | snapped 0..1 | unison or oscillator |
| `SUB_PROGRAM_PARAM` | snapped 0..7 | divider table 2..9 |
| `FREQ_MOD_MODE_PARAM` | snapped 0..1 | Glide or Vibrato |

### Inputs

| Input | Purpose |
| --- | --- |
| `AUDIO_INPUT` | Main audio input, polyphonic supported |
| `RATE_CV_INPUT` | Modulates rate |
| `SQUARE_CV_INPUT` | Modulates square voice level |
| `OSC_CV_INPUT` | Modulates oscillator voice level |
| `SUB_CV_INPUT` | Modulates sub voice level |
| `OSC_PROGRAM_CV_INPUT` | Quantized CV selection for oscillator program |
| `SUB_PROGRAM_CV_INPUT` | Quantized CV selection for sub program |

### Outputs

| Output | Purpose |
| --- | --- |
| `AUDIO_OUTPUT` | Mixed effect output |
| `SQUARE_OUTPUT` | Isolated square voice |
| `OSC_OUTPUT` | Isolated PLL oscillator voice |
| `SUB_OUTPUT` | Isolated subharmonic voice |
| `LOCK_OUTPUT` | Optional gate/CV indicating PLL lock confidence |

### Lights

| Light | Purpose |
| --- | --- |
| `TRACK_LIGHT` | Input gate/tracking activity |
| `LOCK_LIGHT` | PLL near-lock status |
| `GLITCH_LIGHT` | Optional instability/no-lock indicator |

## File-Level Implementation Plan

Add these files:

- `src/Korupt.hpp`
- `src/Korupt.cpp`
- `src/dsp/korupt-dsp.h`
- `res/KORUPT.svg`
- `src/tests/test_korupt.cpp`

Modify these files:

- `src/plugin.hpp`: add `extern Model* modelKorupt;`
- `src/plugin.cpp`: add `p->addModel(modelKorupt);`
- `plugin.json`: add a module entry with tags like `Effect`, `Distortion`, `Synth voice`, `External`

The Makefile already compiles `src/*.cpp`, so no source list change is needed unless DSP code moves into `.cpp` files under `src/dsp/`.

## DSP Class Sketch

Keep Rack glue out of the DSP engine:

```cpp
namespace ShortwavDSP {

struct KoruptParams {
    float level = 1.0f;
    float rate = 0.5f;
    float squareMix = 0.5f;
    float oscMix = 0.5f;
    float subMix = 0.5f;
    int oscRoot = 0;
    int oscProgram = 0;
    int subRoot = 0;
    int subProgram = 0;
    int freqModMode = 0; // 0 = Glide, 1 = Vibrato
};

struct KoruptResult {
    float mixed = 0.0f;
    float square = 0.0f;
    float oscillator = 0.0f;
    float sub = 0.0f;
    float lock = 0.0f;
};

class KoruptDSP {
public:
    void reset();
    void setSampleRate(float sampleRate);
    KoruptResult process(float input, const KoruptParams& params);

private:
    // First pass can use rack::dsp::SchmittTrigger in the Rack wrapper.
    // If this DSP class stays Rack-free, mirror only the tiny threshold state here.
};

}
```

For polyphony, store one `KoruptDSP engines[16]` in the Rack module and iterate over input channels using Rack's `getPolyVoltage()`/`setVoltage()` APIs.

## Implementation Sequence

1. Add the standalone Rack module shell with bypass routing from audio input to mixed output.
2. Implement the square voice with Rack's `dsp::SchmittTrigger` and verify Square-only fuzz output.
3. Add root divider and oscillator multiplier table using a minimal PLL/NCO bring-up path.
4. Replace the bring-up path with the faithful type-II PFD plus loop filter before considering the module complete.
5. Add 4017-style sub divider and `SUB_ROOT` behavior.
6. Add Glide/Vibrato mode, rate CV, selector CV, and per-voice outputs.
7. Add oversampling or bandlimited edge handling.
8. Add tests for:
   - `dsp::SchmittTrigger` threshold behavior or any local Rack-free threshold mirror.
   - Osc multiplier ratios `1..8`.
   - Sub divider ratios `2..9`.
   - PLL locks to stable sine/square input within tolerance.
   - Silence does not chatter indefinitely.
   - Outputs stay finite and bounded.

## Open Questions

- The exact analog tone of the 4069 inverter chain and TL072 output stage can be approximated first with saturation and filtering. A component-level model is probably unnecessary for a playable Rack module.
- The module name should avoid implying affiliation with EQD or Schumann. `Korupt` is the chosen module name; avoid `Data Corrupter` and EQD branding.
- The screenshot's layout is landscape, while Rack modules are vertical. The 20HP layout should preserve the same grouped hierarchy without forcing the exact pedal proportions.
- The original pedal intentionally has no user gain/tracking control. If Rack source-level variation makes a track threshold necessary, prefer an internal calibration constant or advanced context-menu option over a front-panel `Track` control.

## Recommendation

Build `Korupt` as a standalone PLL harmonizer/fuzz module, not as a generic crusher. The minimum satisfying version is Square, Oscillator, Sub, Rate, Level, root/program selectors, audio in/out, and individual voice outs. The faithful version adds edge-based 4046 PLL behavior, 4017 counter quirks, glide/vibrato options, oversampling, lock indication, and robust tests around ratio and tracking behavior.
