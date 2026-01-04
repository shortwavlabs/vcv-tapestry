# Tapestry VCV Rack Plugin - AI Coding Agent Guide

## Project Overview

Tapestry is a granular microsound processor for VCV Rack 2.0+, inspired by the Make Noise Morphagene hardware. It consists of two C++ modules: **Tapestry** (main granular processor) and **TapestryExpander** (effects chain). Audio processing happens at 48kHz stereo with up to 2.9 minutes of recording buffer and 300 markers per reel.

## Architecture

### Module Structure
- **Tapestry** ([src/Tapestry.hpp](../src/Tapestry.hpp), [src/Tapestry.cpp](../src/Tapestry.cpp)): Main VCV Rack module inheriting `rack::Module`. Contains UI widget definitions and parameter mappings.
- **TapestryExpander** ([src/TapestryExpander.hpp](../src/TapestryExpander.hpp)): Right-side expander providing bit crusher and Moog VCF effects.
- **DSP Layer** ([src/dsp/](../src/dsp/)): Header-only C++ DSP processing, separated from VCV Rack API for portability and testing.

### DSP Components (in `src/dsp/`)
- `tapestry-core.h`: Core types, constants (`TapestryConfig`), and data structures (`SpliceMarker`, `GrainVoice`)
- `tapestry-buffer.h`: Stereo audio buffer management (2.9 min max, circular recording)
- `tapestry-splice.h`: Marker management (up to 300 markers with organize parameter)
- `tapestry-grain.h`: 4-voice granular synthesis engine with Hann windowing
- `tapestry-dsp.h`: Top-level DSP processor (`TapestryDSP` class) integrating all components
- `tapestry-effects.h`: Bit crusher and Moog VCF implementations

### Expander Communication
Uses VCV Rack's double-buffered expander protocol (`TapestryExpanderMessage` in [src/TapestryExpanderMessage.hpp](../src/TapestryExpanderMessage.hpp)):
- Tapestry sends audio to expander via `rightExpander.module` and `rightExpander.producerMessage`
- TapestryExpander receives from `leftExpander.module` and `leftExpander.consumerMessage`
- Processed audio flows back through the message struct (`processedL`, `processedR` fields)

## Development Workflow

### Build System
```bash
# Build plugin (requires VCV Rack SDK in dep/Rack-SDK)
RACK_DIR=./dep/Rack-SDK make dist  # Or use build.sh

# Test without VCV Rack
./run_tests.sh  # Compiles src/tests/test_tapestry.cpp standalone

# Clean build artifacts
./clean.sh
```

The Makefile uses VCV Rack's `plugin.mk` framework. All `.cpp` files in `src/` are auto-included via `$(wildcard src/*.cpp)`.

### Testing
Tests in [src/tests/test_tapestry.cpp](../src/tests/test_tapestry.cpp) use `SHORTWAV_DSP_RUN_TESTS` define. DSP headers are designed to compile standalone (no VCV Rack deps) for unit testing. Run `./run_tests.sh` for quick DSP validation without building the full plugin.

### File Organization
- `res/`: SVG panels, graphics, and UI assets
- `docs/`: User documentation (QUICKSTART.md, API_REFERENCE.md, examples/)
- `build/`: Compiler intermediates (*.d files)
- `plugin.json`: Module metadata, version, and module slugs

## Coding Conventions

### Terminology Mapping (CRITICAL for legal compliance)
See [docs/TERMINOLOGY_MAPPING.md](../docs/TERMINOLOGY_MAPPING.md). Code identifiers preserve original names (`VARI_SPEED_PARAM`, `GENE_SIZE_PARAM`, `MORPH_PARAM`, `SLIDE_PARAM`, `SPLICE_BUTTON`) but **all user-facing strings** use replacements:
- **Gene Size** → "Grain Size" in `configParam()` labels
- **Morph** → "Density"
- **Slide** → "Scan"
- **Splice** → "Marker"
- **Vari-Speed** → "Speed"
- **Sound On Sound** → "Mix"

When adding UI labels or documentation, always use the replacement terms.

### Parameter Ranges and CV Scaling
Constants in `TapestryConfig` ([src/dsp/tapestry-core.h](../src/dsp/tapestry-core.h)) define CV ranges:
- Unipolar CV: 0-8V (`kSosCvMax`), 0-5V (`kMorphCvMax`, `kOrganizeCvMax`)
- Bipolar CV: ±8V (`kGeneSizeCvMax`), ±4V (`kVariSpeedCvMax`)
- Gates trigger at 2.5V (`kGateTriggerThreshold`)

Parameters use 0.0-1.0 normalized ranges internally. CV inputs scale to these ranges in the module's `process()` method.

### Real-Time Safety
DSP code avoids allocations in audio thread:
- Buffers pre-allocated in constructors (`TapestryDSP::reset()`)
- Use stack allocation or fixed-size arrays in `process()` loops
- Atomic variables for cross-thread communication (e.g., recording state)

### Header-Only DSP Pattern
DSP headers (`.h` files) contain full implementations to allow compiler optimizations and standalone testing. They use `namespace ShortwavDSP` to avoid collisions.

## Key Integration Points

### Adding New Parameters
1. Add enum to `ParamIds` in [Tapestry.hpp](../src/Tapestry.hpp)
2. Call `configParam()` in module constructor ([Tapestry.cpp](../src/Tapestry.cpp))
3. Read parameter in `process()` method: `params[PARAM_ID].getValue()`
4. Add corresponding setter to `TapestryDSP` class if DSP processing needed
5. Update [docs/API_REFERENCE.md](../docs/API_REFERENCE.md) parameter table

### Expander Pattern Example
Tapestry checks for expander: `rightExpander.module && rightExpander.module->model == modelTapestryExpander`  
Then casts message: `TapestryExpanderMessage* msg = (TapestryExpanderMessage*)rightExpander.producerMessage;`  
TapestryExpander reads: `Module* left = leftExpander.module;` and `TapestryExpanderMessage* msg = (TapestryExpanderMessage*)leftExpander.consumerMessage;`

### Documentation Structure
- [QUICKSTART.md](../docs/QUICKSTART.md): User-focused tutorial with examples
- [API_REFERENCE.md](../docs/API_REFERENCE.md): Technical parameter tables and DSP specs
- [CONTRIBUTING.md](../docs/CONTRIBUTING.md): Development guidelines and PR process
- [TERMINOLOGY_MAPPING.md](../docs/TERMINOLOGY_MAPPING.md): Legal compliance reference

When updating parameters or features, update relevant docs in parallel.

## Common Patterns

### Smooth Parameter Changes
Use exponential smoothing in DSP: `current += coeff * (target - current)` where `coeff = 1 - exp(-1 / (sampleRate * timeMs * 0.001))`. See `SmoothParam` class in [TapestryExpander.hpp](../src/TapestryExpander.hpp).

### Granular Synthesis
4-voice grain engine uses Hann windowing (`0.5 * (1 - cos(2π * phase))`). Each grain has independent position, phase, and playback rate. See `GrainEngine` in [src/dsp/tapestry-grain.h](../src/dsp/tapestry-grain.h).

### Marker/Splice Navigation
Markers stored as `SpliceMarker` with `startFrame` and `endFrame`. `SpliceManager` handles insertion, deletion, and navigation. Organize parameter (0.0-1.0) maps to marker index for manual selection.

## Version and License
- Current version: 2.0.0 (update in [plugin.json](../plugin.json))
- License: GPL-3.0-or-later (see [LICENSE.md](../LICENSE.md))
- VCV Rack SDK: 2.0+ required (bundled in `dep/Rack-SDK/`)
