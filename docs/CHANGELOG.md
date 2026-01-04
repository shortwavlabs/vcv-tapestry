# Changelog

All notable changes to the Tapestry VCV Rack plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned Features
- Additional filter types (high-pass, band-pass, notch)
- MIDI control support for parameters
- Preset management system
- Advanced marker automation modes
- Real-time waveform analysis and markers
- Multi-channel expander support (4+ channels)

---

## [2.0.0] - 2026-01-04

### Initial Release

**Tapestry** - A granular microsound processor for VCV Rack inspired by musique concrète.

### Core Features
- **Granular synthesis engine** with up to 4 simultaneous voices
- **Recording system** supporting up to 2.9 minutes of stereo audio (48kHz)
- **Marker system** with up to 300 markers per reel for audio segmentation
- **Clock synchronization** with Grain Shift and Time Stretch modes
- **Comprehensive CV control** for all major parameters
- **32 reel slots** for storing multiple recordings per patch
- **WAV file import/export** with automatic resampling support

### Modules
- **Tapestry** - Main granular processor module
- **Tapestry Expander** - Effects processor with bit crusher and Moog VCF 24dB/octave filter

### Parameters & Controls
- **MIX**: Recording crossfade/blend control (0-100%, 0-8V CV)
- **Grain Size**: Grain length control (1ms to full reel, ±8V CV)
- **Speed**: Playback rate/direction (-26 to +12 semitones, ±4V CV)
- **Density**: Grain overlap control (1-4 voices, 0-5V CV)
- **Scan**: Grain position offset within markers (0-100%, 0-8V CV)
- **Select**: Manual marker selection (0-N range, 0-5V CV)
- **Overdub Toggle**: Recording mode (replace vs. mix)
- **Marker Count Mode**: Toggle (1/4/8/16 markers per NEXT cycle)

### Buttons & Gates
- **REC**: Record toggle (hold 1s to clear reel)
- **MARKER**: Create/delete marker (hold 1s to delete current)
- **NEXT**: Advance to next marker
- **CLEAR**: Clear all markers (requires NEXT held)
- **PLAY**: Playback gate input
- **CLK**: Clock input for time-stretched granulation

### Outputs
- **Audio Out L/R**: Stereo audio outputs (±5V)
- **Envelope CV**: Grain amplitude envelope (0-8V)
- **EOG (End-of-Grain)**: Gate output when grain completes (0-10V)
- **EOM (End-of-Marker)**: Gate output when marker segment completes (0-10V)

### Expander Effects
- **Bit Crusher**: Sample rate reduction (0-100%) and bit depth (1-16 bits)
- **Moog VCF**: 24dB/octave resonant lowpass filter with cutoff and resonance controls
- **Individual Dry/Wet Mix**: Separate mix controls for each effect
- **Output Level**: Post-effects gain control (0-200%)
- **Full CV Control**: All parameters voltage-controllable

### Technical Features
- **Hann windowing** for smooth, artifact-free grains
- **Pitch and pan randomization** at high density values (±3 semitones, stereo spread)
- **Real-time safe** DSP with no allocations in audio path
- **Thread-safe file I/O** with atomic flags
- **Double-buffered expander protocol** for seamless audio routing
- **Parameter smoothing** for zipper-free CV modulation
- **JSON state persistence** including marker positions and file paths
- **Waveform display** with 7 color themes

### Visual Features
- **Real-time waveform display** with current position indicator
- **7 color themes**: Red, Amber, Green, Baby Blue, Peach, Pink, White
- **Connected indicator** on expander module
- **Visual feedback** for recording, playback, and marker states

### Documentation
- Comprehensive user documentation suite
- Quick Start Guide with step-by-step tutorials
- Complete API Reference for developers
- Advanced Usage guide with expert techniques
- Real-world examples and patch templates
- FAQ covering common questions
- Contributing guidelines for open-source contributors

### Performance
- **Optimized DSP** for real-time performance
- **CPU usage**: ~1-2% idle, ~8-12% with 4 voices at 48kHz
- **Memory footprint**: ~35MB per module instance
- **Low latency**: 2-sample latency through expander

### Compatibility
- **VCV Rack 2.0+** required
- **Cross-platform**: macOS, Windows, Linux
- **Open source**: GPL-3.0-or-later license

---

## Version History Summary

| Version | Release Date | Key Features |
|---------|--------------|--------------|
| **2.0.0** | 2026-01-04 | Initial release with full feature set |

---

## Contributing

We welcome bug reports, feature requests, and contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Reporting Issues

When reporting bugs, please include:
- VCV Rack version
- Tapestry plugin version
- Operating system
- Steps to reproduce
- Expected vs. actual behavior
- Patch file (if applicable)

**Submit issues**: [GitHub Issues](https://github.com/shortwavlabs/tapestry/issues)

---

## Links

- **Repository**: [github.com/shortwavlabs/tapestry](https://github.com/shortwavlabs/tapestry)
- **Documentation**: [docs/](docs/)
- **VCV Library**: [library.vcvrack.com](https://library.vcvrack.com/)
- **Website**: [shortwavlabs.com](https://shortwavlabs.com)
- **Email**: contact@shortwavlabs.com

---

## License

This project is licensed under the **GPL-3.0-or-later** license.

See [LICENSE](../LICENSE) file for full details.

---

**Thank you for using Tapestry!** 🎵

*Last Updated: January 4, 2026*
