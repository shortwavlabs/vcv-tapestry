# Tapestry FAQ

**Frequently Asked Questions and Solutions**

---

## Table of Contents

1. [General Questions](#general-questions)
2. [Installation and Setup](#installation-and-setup)
3. [Recording and Playback](#recording-and-playback)
4. [Granular Processing](#granular-processing)
5. [Markers and Navigation](#splices-and-navigation)
6. [Clock Synchronization](#clock-synchronization)
7. [Expander Module](#expander-module)
8. [File Management](#file-management)
9. [Performance and CPU](#performance-and-cpu)
10. [Troubleshooting](#troubleshooting)

---

## General Questions

### What is Tapestry?

Tapestry is a VCV Rack plugin that combines tape music techniques with granular synthesis. It allows you to record audio, create splice markers, and manipulate sound using granular processing techniques inspired by musique concrète.

### What makes Tapestry different from other granular modules?

**Unique features**:
- **Marker system**: Up to 300 markers for precise audio segmentation
- **Integrated recording**: Built-in tape-style recorder (up to 2.9 minutes)
- **Clock sync**: Both Grain Next and Time Stretch modes
- **Expander effects**: Integrated bit crusher and Moog VCF filter
- **Musique concrète workflow**: Designed for experimental and concrete music techniques

### Is Tapestry based on hardware?

Tapestry is inspired by the Make Noise Tapestry hardware module but is an independent implementation designed for VCV Rack.

### What VCV Rack version is required?

Tapestry requires **VCV Rack 2.0 or later**.

### Is Tapestry free?

Yes! Tapestry is open source under the GPL-3.0-or-later license.

---

## Installation and Setup

### How do I install Tapestry?

**Method 1 (Recommended)**: VCV Library
1. Open VCV Rack
2. View → Library (or press F1)
3. Search for "Tapestry"
4. Click Install
5. Restart VCV Rack

**Method 2**: Manual build
```bash
git clone https://github.com/shortwavlabs/tapestry.git
cd tapestry
make install
```

### Why isn't Tapestry showing up after installation?

**Possible causes**:
1. VCV Rack wasn't restarted after installation
2. Plugin failed to load (check VCV Rack log)
3. Wrong VCV Rack version (must be 2.0+)

**Solutions**:
- Restart VCV Rack completely
- Check `~/.Rack2/log.txt` for errors
- Verify VCV Rack version: Help → About

### Can I use Tapestry with VCV Rack 1.x?

No. Tapestry is built for VCV Rack 2.0+ and uses API features not available in 1.x.

---

## Recording and Playback

### How much audio can I record?

**Maximum recording time**: 2.9 minutes (174 seconds) stereo at 48kHz

**Buffer size**: 8,352,000 frames

### Can I record while playing back?

Yes! Tapestry supports **overdub mode**:
1. Enable Overdub toggle (switch to ON/up position)
2. Use Mix knob to control blend between old and new audio
3. Record while playing to layer sounds

### Why is there no sound during recording?

**Recording is pass-through**: Audio flows through even while recording. You should hear your input signal.

**If you don't hear anything**:
- Check audio input connections
- Verify input source is active
- Ensure VCV Audio interface is configured
- Check that Mix knob isn't at 0%

### How do I clear the recorded audio?

**Hold REC button** for 1 second to clear the entire reel.

**Warning**: This is irreversible unless you saved the reel to a file first!

### Can I record mono sources?

Yes! Connect your mono source to either L or R input (or both). Tapestry will record mono to that channel.

### Why does my recording sound distorted?

**Cause**: Input level too high

**Solutions**:
- Reduce source module volume
- Use attenuator before Tapestry input
- Keep peaks around -6dB (check with scope)
- Avoid recording signals > ±5V

---

## Granular Processing

### What is a "grain" (gene)?

A grain (or "gene" in Tapestry terminology) is a small segment of audio read from the buffer, typically 1-200ms in length. Grains are windowed with a smooth envelope (Hann window) to avoid clicks.

### How many grain voices can play simultaneously?

**Up to 4 voices**, controlled by the Density parameter:
- Density = 0%: 1 voice
- Density = 33%: ~2 voices
- Density = 66%: ~3 voices
- Density = 100%: 4 voices

### Why do my grains sound clicky?

**Common causes**:
1. **Grain Size too small**: Increase to 20ms+ (Grain Size > 10%)
2. **Source material has clicks**: Record clean audio
3. **Marker boundaries**: Create markers at zero-crossings

**Solutions**:
- Increase Grain Size parameter
- Increase Density for smoother overlap
- Re-record with cleaner source material

### What does the Scan parameter do?

**Scan** offsets where grains read from within the current segment:
- 0%: Grains read from current playback position
- 50%: Grains read from middle of splice
- 100%: Grains read from end of splice

**Use case**: Scanning through audio like a wavetable

### Why doesn't Speed affect pitch?

**In Time Stretch mode** (Density > 50% with clock connected), playback speed adapts to the clock while preserving pitch. This is intentional for tempo-synced playback.

**To change pitch**: Disconnect clock or use Speed without clock sync.

### Can I use Tapestry as a regular sampler?

Yes! For traditional sampler behavior:
1. Set Grain Size to 100% (full length)
2. Set Density to 0% (single voice)
3. Create splices around each sample
4. Use Select to select samples
5. Use Speed for pitch shifting

---

## Markers and Navigation

### What are splices?

**Markers** are markers that divide your recorded audio into segments. They're similar to cue points in DJ software or markers in a DAW.

### How many splices can I create?

**Maximum**: 300 splices per reel

### How do I create a splice?

1. Record or load audio first
2. Click SPLICE button (or send gate to SPLICE input)
3. Marker is created at current playback position

### How do I delete a splice?

**Delete current splice**: Hold SPLICE button for 1 second

**Delete all splices**: Hold NEXT, then click CLEAR button

### How do I navigate between splices?

**Method 1**: Click NEXT button (advances to next splice)

**Method 2**: Use Select knob (manual selection, 0 to N-1)

**Method 3**: Send CV to ORGANIZE input (0-5V = full range)

### Why can't I create splices?

**Common causes**:
1. No audio recorded in buffer
2. Already at maximum (300 splices)
3. Position too close to existing splice

**Solutions**:
- Record audio first
- Delete unused splices
- Move playback position

### Do splices save with my patch?

**Yes!** Marker positions are saved in the VCV Rack patch file (.vcv) as normalized positions (0.0-1.0).

When you load the patch, splices are restored automatically.

### What is splice count mode?

**Marker count mode** determines how many markers NEXT button advances:
- Mode 1: Advance by 1 splice (default)
- Mode 4: Advance by 4 splices
- Mode 8: Advance by 8 splices
- Mode 16: Advance by 16 splices

**Toggle**: Hold NEXT, then click COUNT button

**Use case**: Quick navigation through many splices

---

## Clock Synchronization

### What's the difference between Grain Next and Time Stretch modes?

**Grain Next** (Density < 50%):
- Clock triggers individual grains
- No playback speed adaptation
- Best for: Rhythmic, percussive material

**Time Stretch** (Density > 50%):
- Playback adapts to clock tempo
- Pitch preservation
- Best for: Melodic, sustained material

### How do I enable clock sync?

Simply connect a clock signal to the CLK input. Mode is determined automatically by Density parameter.

### My clock sync isn't working. Why?

**Checklist**:
- ✅ Clock signal connected to CLK input
- ✅ Clock outputting 0-10V gates
- ✅ Density set appropriately (< 50% or > 50%)
- ✅ Grain Size not too large (< 60%)
- ✅ Marker has reasonable length

### Can I sync to multiple clocks?

You can only connect one clock to CLK input, but you can use clock dividers/multipliers externally to create complex rhythms before feeding to Tapestry.

### What clock rates work best?

**For Grain Next**:
- 1/4 notes: Sparse
- 1/8 notes: Moderate
- 1/16 notes: Busy
- 1/32 notes: Very dense

**For Time Stretch**:
- 1/bar: Match full phrase
- 1/2 notes: Phrase at half-note rate
- 1/4 notes: Quarter-note rate

---

## Expander Module

### What does the Tapestry Expander do?

The expander adds two effects:
1. **Bit Crusher**: Sample rate reduction and bit depth quantization
2. **Moog VCF**: 24dB/octave resonant lowpass filter

### How do I connect the expander?

**Physical placement**: Place expander **directly to the right** of Tapestry. No cables needed!

**Verify**: CONNECTED light on expander should be lit.

### Can I use multiple expanders?

Currently, only one expander is supported per Tapestry instance.

### Why isn't the expander working?

**Common issues**:
1. **Not positioned correctly**: Must be immediate right neighbor
2. **Mix controls at 0%**: Increase Crush MIX or Filter MIX
3. **No audio from Tapestry**: Verify Tapestry is generating audio

**Solutions**:
- Reposition expander directly right of Tapestry
- Set MIX controls to 50%+
- Check Tapestry output with scope module

### Can I use the expander without Tapestry?

No. The expander receives audio from Tapestry via the expander protocol and has no independent inputs/outputs.

### Does the expander add latency?

**Yes**, but minimal: **2 samples** total (one frame in each direction).

This is negligible for most applications (< 0.05ms at 48kHz).

---

## File Management

### How do I save my recorded audio?

1. Right-click Tapestry module
2. Select "Save Current Reel"
3. Choose location and filename
4. Click Save

**File format**: 32-bit float stereo WAV at 48kHz

### How do I load audio files?

1. Right-click Tapestry module
2. Select "Load Reel"
3. Choose WAV file
4. Click Open

**Supported formats**:
- Mono or stereo WAV
- Any sample rate (resampled to 48kHz)
- 16-bit, 24-bit, 32-bit int, 32-bit float

### Where are my reels stored?

**Reels are stored**:
1. In VCV Rack patch file (if you save the patch)
2. As separate WAV files (if you use "Save Current Reel")

**Note**: Saving the patch saves splice positions but NOT audio data. Use "Save Current Reel" to export audio.

### Can I use audio from other DAWs?

Yes! Export audio from your DAW as WAV, then load into Tapestry using "Load Reel".

### What are reel slots?

Tapestry supports **32 reel slots** (numbered 0-31). Each slot stores one recording independently.

**Access**: Right-click → Select Reel → Choose 0-31

**Use case**: Store multiple recordings in one patch

### Do reel slots save with the patch?

**Marker positions**: Yes (saved in patch JSON)

**Audio data**: No (save as WAV files for persistence)

**File paths**: Yes (if you saved reels, paths are remembered)

---

## Performance and CPU

### How much CPU does Tapestry use?

**Typical usage** (at 48kHz):
- Idle: ~1-2%
- 1 voice: ~3-5%
- 4 voices: ~8-12%
- With expander: +2-4%

**Total**: ~10-16% for full featured use

### How can I reduce CPU usage?

**Optimization strategies**:
1. **Reduce Density**: Lower voice count (Density at 50% = 2-3 voices vs 4)
2. **Increase Grain Size**: Fewer retriggerings
3. **Limit splices**: Use < 32 splices
4. **Lower sample rate**: 44.1kHz vs 48kHz (Engine → Sample Rate)

### Why is Tapestry using so much CPU?

**Common causes**:
1. **Many active voices**: Density at 100% = 4 voices
2. **Small grains**: Grain Size < 10% = many retriggerings
3. **Many splices**: > 100 splices = overhead
4. **High sample rate**: 96kHz or higher

**Solutions**: See optimization strategies above

### Does Tapestry cause audio dropouts?

In normal usage, no. If you experience dropouts:
1. Reduce voice count (lower Density)
2. Increase audio buffer size (VCV Rack settings)
3. Close other applications
4. Check overall patch CPU usage

---

## Troubleshooting

### No audio output

**Checklist**:
- ✅ Audio recorded in buffer (waveform visible)
- ✅ PLAY enabled (button on or gate high)
- ✅ Outputs connected to audio interface
- ✅ VCV Audio interface configured
- ✅ Mix knob not at 0%
- ✅ Grain Size not 0%

### Audio is very quiet

**Possible causes**:
1. Grain Size too small (< 5%)
2. Density too low (single voice, sparse)
3. Mix control low
4. Output level on expander low

**Solutions**:
- Increase Grain Size to 20%+
- Increase Density to 40%+
- Check all level controls

### Clicks and pops during playback

**Causes**:
1. Grain Size too small
2. Marker boundaries not on zero-crossings
3. Buffer discontinuities
4. CPU overload

**Solutions**:
- Increase Grain Size
- Recreate splices carefully
- Reduce CPU usage
- Increase buffer size (VCV settings)

### Waveform display not updating

**Usually harmless**: Display updates are throttled for CPU efficiency.

**If stuck**: Save and reload the patch.

### Patch won't load/crashes

**Debugging steps**:
1. Check VCV Rack log: `~/.Rack2/log.txt`
2. Try opening patch in text editor
3. Look for corrupted JSON
4. Remove Tapestry module from patch (manual JSON edit)
5. Report bug with log file

### "Expander connection lost" during performance

**Cause**: Modules were rearranged, breaking connection

**Prevention**: Don't move expander once connected

**Recovery**: Move expander back to immediate right position

---

## Advanced Questions

### Can I script or automate Tapestry?

**Limited**: VCV Rack doesn't have built-in scripting, but you can:
- Use CV sequencers for automation
- Use MIDI-CV for external control
- Create complex patches with logic modules

### Is there a polyphonic mode?

No. Tapestry is monophonic (mono or stereo audio, but one "voice" per instance).

**Workaround**: Use multiple Tapestry instances for polyphony.

### Can I synchronize multiple Tapestry modules?

Yes! Send the same clock to multiple instances for synchronized granulation.

**Example**:
```
[Clock] → Split:
    → Tapestry A CLK
    → Tapestry B CLK
    → Tapestry C CLK
```

### How is pitch randomization calculated?

At high Density values (> 50%), pitch variation is applied:
- **Maximum deviation**: ±3 semitones (at Density = 100%)
- **Distribution**: Uniform random
- **Algorithm**: Xorshift128+ fast RNG

### What's the grain windowing function?

Tapestry uses **Hann window**:
```cpp
amplitude = 0.5 * (1 - cos(2π * phase))
```

This provides smooth attack/decay with no artifacts.

---

## Support and Community

### Where can I get help?

1. **This FAQ**
2. **Documentation**: [README.md](../README.md), [Quick Start](../QUICKSTART.md)
3. **GitHub Issues**: [github.com/shortwavlabs/tapestry/issues](https://github.com/shortwavlabs/tapestry/issues)
4. **Email**: contact@shortwavlabs.com

### How do I report a bug?

See [CONTRIBUTING.md](../CONTRIBUTING.md) for bug report template.

**Include**:
- VCV Rack version
- Tapestry version
- Operating system
- Steps to reproduce
- Patch file (if applicable)

### How can I contribute?

We welcome contributions! See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines on:
- Reporting bugs
- Suggesting features
- Submitting code
- Improving documentation

### Is there a Discord or forum?

Not yet! For now, use:
- GitHub Discussions
- GitHub Issues
- Email: contact@shortwavlabs.com

---

## Still Have Questions?

If your question isn't answered here:

1. Search [existing GitHub issues](https://github.com/shortwavlabs/tapestry/issues)
2. Check [documentation](../README.md)
3. Open a new GitHub issue
4. Email: contact@shortwavlabs.com

**We're here to help!** 🎵

---

*Last Updated: January 4, 2026*
