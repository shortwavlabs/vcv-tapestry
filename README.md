# Tapestry

![Version](https://img.shields.io/badge/version-2.0.0-blue)
![License](https://img.shields.io/badge/license-GPL--3.0--or--later-green)
![VCV Rack](https://img.shields.io/badge/VCV%20Rack-2.0+-orange)

**A granular microsound processor for VCV Rack inspired by musique concrète**

Tapestry brings the world of tape music manipulation and granular synthesis to your modular patches. Record, splice, and morph audio with sophisticated granular processing, creating everything from subtle textures to extreme sound design.

---

## 🎵 Overview

Tapestry is a VCV Rack plugin that combines traditional tape music techniques with modern granular synthesis. It provides:

- **Reels**: Record up to 2.9 minutes of stereo audio
- **Markers**: Create up to 300 markers for precise audio segmentation
- **Grains**: Granular particles with variable size and overlap
- **Time Manipulation**: Speed control for playback rate and direction
- **Mix**: Crossfade recording with overdub capabilities
- **Time Stretch**: Clock-synced granular playback for rhythmic effects
- **Expander Module**: Add bit crushing and Moog-style filtering

---

## ✨ Key Features

### Tapestry Main Module

- 🎙️ **High-Quality Recording**: Stereo recording at 48kHz with overdub mode
- 📍 **Marker System**: Mark and navigate between up to 300 audio segments
- 🌊 **Granular Engine**: 4-voice polyphonic granular synthesis with Hann windowing
- ⚡ **CV Control**: Full voltage control over all parameters
- 🎚️ **Speed**: ±26 semitones range with CV control
- 🔄 **Select Mode**: Select and jump between markers
- 💾 **File Management**: Save/load reels as WAV files (up to 32 slots)
- 🎨 **Visual Feedback**: Real-time waveform display with 7 color themes

### Tapestry Expander

- 🔊 **Bit Crusher**: Sample rate reduction and bit depth quantization
- 🔈 **Moog VCF**: 24dB/octave resonant lowpass filter
- 🎛️ **Dry/Wet Mix**: Individual mix controls for each effect
- 📊 **CV Control**: Voltage control for all effect parameters

---

## 📦 Installation

### Method 1: VCV Rack Library (Recommended)

1. Open VCV Rack
2. Open the VCV Library browser (View → Library)
3. Search for "Tapestry"
4. Click "Subscribe" or "Install"
5. Restart VCV Rack

### Method 2: Manual Installation

```bash
# Clone the repository
git clone https://github.com/shortwavlabs/tapestry.git
cd tapestry

# Build the plugin
make install

# The plugin will be installed to your VCV Rack plugins folder
```

### Build Requirements

- VCV Rack SDK 2.0+
- C++17 compatible compiler
- Make

---

## 🚀 Quick Start

### Basic Recording

1. **Add Tapestry** to your patch
2. **Connect audio** to the L and R inputs
3. **Click REC** (or send a gate to REC input) to start recording
4. **Click REC again** to stop recording

### Creating Markers

1. **Record some audio** first
2. **Click MARKER** while playing to mark the current position
3. **Use NEXT button** (or NEXT input) to advance to the next marker
4. **Adjust SELECT** knob to manually select markers

### Granular Processing

1. **Set Grain Size** to control grain length (1ms to full reel)
2. **Adjust Density** to control grain overlap (0-4 voices)
3. **Use Scan** to offset grain position within the current marker
4. **Connect CLK** input for time-stretched, rhythm-synced granulation

### Adding Effects with Expander

1. **Add Tapestry Expander** to the right of Tapestry
2. **Adjust effect parameters**:
   - Bit Crusher: BITS and RATE controls
   - Moog VCF: CUTOFF and RESO controls
3. **Set MIX** controls to blend dry and wet signals
4. **Use OUTPUT LEVEL** for final gain adjustment

For detailed tutorials, see the [Quick Start Guide](docs/QUICKSTART.md).

---

## 📚 Documentation

- **[Quick Start Guide](docs/QUICKSTART.md)** - Get up and running quickly
- **[API Reference](docs/API_REFERENCE.md)** - Complete technical documentation
- **[Advanced Usage](docs/ADVANCED_USAGE.md)** - In-depth techniques and workflows
- **[Examples](docs/examples/)** - Real-world patch examples
- **[FAQ](docs/FAQ.md)** - Common questions and solutions
- **[Contributing](docs/CONTRIBUTING.md)** - How to contribute to the project
- **[Changelog](docs/CHANGELOG.md)** - Version history and updates

---

## 🎛️ Module Overview

### Tapestry Main Module

#### Controls

| Control | Function | Range | CV Input |
|---------|----------|-------|----------|
| **MIX** | Recording crossfade/blend | 0-100% | 0-8V |
| **Grain Size** | Grain length | 1ms-2.9min | ±8V |
| **Speed** | Playback rate/direction | -26 to +12 semitones | ±4V |
| **Density** | Grain overlap/density | 0-4 voices | 0-5V |
| **Scan** | Grain position offset | 0-100% | 0-8V |
| **Select** | Manual marker selection | 0-299 | 0-5V |

#### Buttons

- **REC**: Start/stop recording (hold 1s to clear reel)
- **MARKER**: Create marker (hold 1s to delete current marker)
- **NEXT**: Advance to next marker
- **CLEAR**: Clear all markers (requires NEXT held)
- **COUNT**: Cycle marker count mode (4/8/16 markers per NEXT cycle)

#### Inputs/Outputs

- **Audio In**: L/R stereo inputs
- **Audio Out**: L/R stereo outputs
- **CV Inputs**: Full CV control over all parameters
- **Gate Inputs**: REC, MARKER, NEXT, PLAY, CLK
- **CV Outputs**: Envelope, End-of-Grain, End-of-Marker

### Tapestry Expander

#### Controls

| Control | Function | Range | CV Input |
|---------|----------|-------|----------|
| **Crush Bits** | Bit depth | 1-16 bits | CV |
| **Crush Rate** | Sample rate reduction | 0-100% | CV |
| **Crush Mix** | Dry/wet blend | 0-100% | CV |
| **Filter Cutoff** | Cutoff frequency | 20Hz-20kHz | CV |
| **Filter Reso** | Resonance | 0-100% | CV |
| **Filter Mix** | Dry/wet blend | 0-100% | CV |
| **Output Level** | Post-effects gain | 0-200% | - |

---

## 🔧 Technical Specifications

- **Sample Rate**: 48kHz internal processing
- **Bit Depth**: 32-bit float
- **Buffer Size**: Up to 8,352,000 frames (2.9 minutes stereo)
- **Maximum Markers**: 300 per reel
- **Grain Voices**: Up to 4 simultaneous
- **File Format**: WAV (32-bit float)
- **CPU Usage**: Optimized for real-time performance

---

## 💡 Use Cases

- **Sound Design**: Create complex textures and atmospheres
- **Live Performance**: Real-time audio manipulation and granulation
- **Composition**: Build evolving soundscapes and drones
- **Sampling**: Record and manipulate field recordings
- **Rhythmic Effects**: Clock-synced time stretching and stuttering
- **Experimental Music**: Explore musique concrète techniques

---

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines on:

- Reporting bugs
- Suggesting features
- Submitting pull requests
- Code style and standards
- Testing procedures

---

## 📄 License

This project is licensed under the **GPL-3.0-or-later** license. See the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- Inspired by the Make Noise Tapestry hardware module
- Built with the [VCV Rack](https://vcvrack.com/) SDK
- Uses the Moog VCF algorithm for analog filter emulation
- Community feedback and testing from VCV Rack users

---

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/shortwavlabs/tapestry/issues)
- **Email**: contact@shortwavlabs.com
- **Website**: [shortwavlabs.com](https://shortwavlabs.com)

---

## 🗺️ Roadmap

See our [project board](https://github.com/shortwavlabs/tapestry/projects) for upcoming features and improvements.

### Planned Features

- Additional filter types in expander
- MIDI control support
- Preset management system
- Advanced splice automation
- Real-time waveform analysis

---

**Made with ❤️ by [Shortwav Labs](https://shortwavlabs.com)**
