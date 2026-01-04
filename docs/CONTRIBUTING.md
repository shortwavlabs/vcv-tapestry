# Contributing to Tapestry

Thank you for your interest in contributing to Tapestry! This document provides guidelines and instructions for contributing to the project.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [How Can I Contribute?](#how-can-i-contribute)
3. [Development Setup](#development-setup)
4. [Coding Standards](#coding-standards)
5. [Testing Guidelines](#testing-guidelines)
6. [Submission Process](#submission-process)
7. [Documentation](#documentation)
8. [Community](#community)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for all contributors, regardless of experience level, background, or identity.

### Expected Behavior

- **Be respectful**: Treat all contributors with respect and consideration
- **Be constructive**: Provide helpful feedback and suggestions
- **Be collaborative**: Work together to improve the project
- **Be patient**: Remember that everyone is learning

### Unacceptable Behavior

- Harassment, discrimination, or offensive comments
- Personal attacks or trolling
- Publishing others' private information
- Any conduct that could reasonably be considered inappropriate

### Reporting

If you experience or witness unacceptable behavior, please contact: contact@shortwavlabs.com

---

## How Can I Contribute?

### Reporting Bugs

**Before submitting a bug report**:
1. Check the [FAQ](FAQ.md) for common issues
2. Search [existing issues](https://github.com/shortwavlabs/tapestry/issues) to avoid duplicates
3. Test with the latest version of Tapestry
4. Test in a minimal patch to isolate the issue

**When submitting a bug report, include**:
- **VCV Rack version** (e.g., 2.5.1)
- **Tapestry version** (e.g., 2.0.0)
- **Operating system** (e.g., macOS 14.2, Windows 11, Ubuntu 22.04)
- **Steps to reproduce** (numbered list)
- **Expected behavior** (what should happen)
- **Actual behavior** (what actually happens)
- **Patch file** (if applicable, attach .vcv file)
- **Screenshots/recordings** (if relevant)

**Bug report template**:
```markdown
**VCV Rack Version**: 2.5.1
**Tapestry Version**: 2.0.0
**OS**: macOS 14.2 (Apple M1)

**Description**:
Brief description of the issue.

**Steps to Reproduce**:
1. Open VCV Rack
2. Add Tapestry module
3. Connect audio input
4. Click REC button
5. ...

**Expected Behavior**:
Recording should start and the button should turn red.

**Actual Behavior**:
Button does not light up and no audio is recorded.

**Additional Context**:
- Happens with both mono and stereo inputs
- Does not occur in version 1.5.0
- Patch file attached
```

---

### Suggesting Features

**Before submitting a feature request**:
1. Check the [Roadmap](#roadmap) to see if it's already planned
2. Search existing issues to avoid duplicates
3. Consider if it aligns with the project's goals

**When submitting a feature request, include**:
- **Use case**: Why is this feature needed?
- **Proposed solution**: How would it work?
- **Alternatives considered**: Other approaches you've thought about
- **Examples**: Similar features in other plugins/software
- **Mockups/diagrams**: Visual representations if applicable

**Feature request template**:
```markdown
**Feature Name**: Advanced Marker Interpolation

**Use Case**:
When navigating between markers with Select, I want smooth crossfading 
between segments to avoid abrupt transitions.

**Proposed Solution**:
Add a "Marker Crossfade" parameter (0-100ms) that crossfades between 
outgoing and incoming splices.

**Alternatives Considered**:
- External crossfader module (adds complexity)
- Always-on crossfade (removes option for abrupt cuts)

**Examples**:
- Ableton Live's "Warp" crossfade
- Native Instruments Kontakt's "Voice Crossfade"

**Additional Context**:
This would be especially useful for melodic content and pad textures.
```

---

### Contributing Code

We welcome code contributions! Here's how to get started.

#### Types of Contributions

- **Bug fixes**: Fix reported or discovered bugs
- **New features**: Implement planned or approved features
- **Optimizations**: Improve performance or memory usage
- **Refactoring**: Improve code structure and maintainability
- **Tests**: Add or improve test coverage
- **Documentation**: Improve code comments and docs

---

## Development Setup

### Prerequisites

- **VCV Rack SDK** 2.0 or later
- **C++17** compatible compiler:
  - macOS: Xcode Command Line Tools
  - Windows: MSVC 2019+ or MinGW-w64
  - Linux: GCC 7+ or Clang 6+
- **Make** build system
- **Git** for version control

### Setting Up Your Environment

#### 1. Clone the Repository

```bash
git clone https://github.com/shortwavlabs/tapestry.git
cd tapestry
```

#### 2. Set Up VCV Rack SDK

**Option A: Download SDK**
```bash
# Download VCV Rack SDK from https://vcvrack.com/
# Extract to a location, e.g., ~/Rack-SDK

# Set environment variable (add to ~/.bashrc or ~/.zshrc)
export RACK_DIR=~/Rack-SDK
```

**Option B: Use Rack-SDK Submodule**
```bash
# The project includes SDK as a submodule
git submodule update --init --recursive
export RACK_DIR=./dep/Rack-SDK
```

#### 3. Build the Plugin

```bash
# Clean build
make clean

# Build
make

# Install to VCV Rack plugins directory
make install
```

#### 4. Run VCV Rack

```bash
# Launch VCV Rack (adjust path as needed)
/Applications/VCV\ Rack\ 2.app/Contents/MacOS/Rack  # macOS
Rack.exe  # Windows
./Rack    # Linux
```

---

### Development Workflow

#### 1. Create a Branch

```bash
# Create and switch to a new branch
git checkout -b feature/my-new-feature

# Or for bug fixes
git checkout -b fix/issue-123
```

**Branch naming conventions**:
- `feature/description` - New features
- `fix/issue-number` - Bug fixes
- `refactor/description` - Code refactoring
- `docs/description` - Documentation updates
- `test/description` - Test additions/improvements

#### 2. Make Changes

Edit source files in the `src/` directory:
- `Tapestry.cpp` / `Tapestry.hpp` - Main module
- `TapestryExpander.cpp` / `TapestryExpander.hpp` - Expander module
- `dsp/*.h` - DSP core classes

#### 3. Build and Test

```bash
# Build
make

# Install
make install

# Test in VCV Rack
# (No automated tests yet, manual testing required)
```

#### 4. Commit Changes

```bash
# Stage changes
git add src/Tapestry.cpp

# Commit with descriptive message
git commit -m "Fix: Resolve audio glitch when switching splices (closes #123)"
```

**Commit message format**:
```
Type: Brief description (closes #issue-number)

Detailed explanation of what changed and why.
- Bullet points for multiple changes
- Reference related issues

Technical details if needed (algorithm changes, performance impacts, etc.)
```

**Commit types**:
- `Fix:` - Bug fixes
- `Feature:` - New features
- `Refactor:` - Code restructuring
- `Docs:` - Documentation changes
- `Test:` - Test additions/changes
- `Perf:` - Performance improvements
- `Style:` - Code style changes (formatting, etc.)

#### 5. Push to GitHub

```bash
# Push your branch
git push origin feature/my-new-feature
```

#### 6. Create Pull Request

1. Go to [GitHub repository](https://github.com/shortwavlabs/tapestry)
2. Click "Pull Requests" → "New Pull Request"
3. Select your branch
4. Fill out the PR template (see [Submission Process](#submission-process))
5. Submit PR

---

## Coding Standards

### C++ Style Guide

#### General Principles

- **Clarity over cleverness**: Write readable, maintainable code
- **Consistency**: Follow existing code style
- **Performance**: Optimize for real-time audio (no allocations in process loop)
- **Safety**: Avoid undefined behavior and memory leaks

#### Naming Conventions

```cpp
// Classes: PascalCase
class GrainEngine { };
class TapestryDSP { };

// Functions/methods: camelCase
void processAudio();
float calculateEnvelope();

// Variables: camelCase
float grainSize;
int currentSplice;

// Member variables: camelCase with trailing underscore
float sampleRate_;
bool isRecording_;

// Constants: kPascalCase
static constexpr float kMaxGrainSize = 8352000.0f;

// Enums: PascalCase
enum class WaveformColor {
  Red,
  Amber,
  Green
};

// Enum values: PascalCase
WaveformColor::BabyBlue
```

#### File Organization

```cpp
#pragma once  // Use pragma once, not include guards

#include "plugin.hpp"  // VCV Rack headers first
#include <vector>      // Standard library headers
#include "dsp/tapestry-core.h"  // Project headers

// Constants
static constexpr float kSampleRate = 48000.0f;

// Forward declarations
struct ReelDisplay;

// Class declaration
class TapestryDSP {
public:
  // Public methods
  void process();
  
private:
  // Private members
  float sampleRate_;
};
```

#### Formatting

**Indentation**: 2 spaces (no tabs)

**Braces**: Opening brace on same line
```cpp
if (condition) {
  doSomething();
} else {
  doSomethingElse();
}
```

**Line length**: Maximum 100 characters

**Comments**: Use `//` for single-line, `/* */` for multi-line
```cpp
// Single-line comment

/*
 * Multi-line comment
 * describing complex behavior
 */
```

---

### Real-Time Safety

**Critical**: No allocations in `process()` method!

**Forbidden in process()**:
```cpp
// ❌ NO
std::vector<float> buffer(size);  // Allocation
new float[size];                  // Allocation
malloc(size);                     // Allocation
std::string str = "text";         // Allocation
```

**Allowed in process()**:
```cpp
// ✅ YES
float buffer[256];                // Stack allocation (small)
float value = param.getValue();   // No allocation
output.setVoltage(value);         // No allocation
```

**Pattern for resizable data**:
```cpp
class MyModule : Module {
  std::vector<float> buffer_;  // Member variable
  
  void onReset() override {
    buffer_.resize(8352000);   // Allocate here (UI thread)
  }
  
  void process() {
    buffer_[index] = value;    // Access here (audio thread)
  }
};
```

---

### DSP Best Practices

#### Sample Rate Handling

```cpp
void setSampleRate(float sampleRate) {
  sampleRate_ = sampleRate;
  // Recalculate coefficients
  updateFilterCoefficients();
}
```

#### Avoid Denormals

```cpp
// Add small epsilon to prevent denormals
float value = input + 1e-18f;
```

#### Use SIMD When Possible

```cpp
// VCV Rack provides SIMD helpers
#include <simd/functions.hpp>
float_4 values = simd::sin(phases);
```

---

## Testing Guidelines

### Manual Testing

Since Tapestry doesn't have automated tests yet, thorough manual testing is essential.

**Test checklist for code changes**:

#### Basic Functionality
- [ ] Module loads without errors
- [ ] All controls respond to input
- [ ] Audio passes through correctly
- [ ] Recording works (REC button)
- [ ] Playback works (PLAY gate)
- [ ] Splices create/delete correctly

#### Parameter Testing
- [ ] Grain Size: Full range (0-100%)
- [ ] Density: Full range (0-100%)
- [ ] Speed: Full range (-100% to +100%)
- [ ] Scan: Full range (0-100%)
- [ ] Select: Full range (0 to N-1)
- [ ] Mix: Full range (0-100%)

#### CV Modulation
- [ ] All CV inputs respond correctly
- [ ] Attenuverters work as expected
- [ ] No zipper noise or glitches
- [ ] Voltage ranges are accurate

#### Edge Cases
- [ ] Empty buffer (no recording)
- [ ] Full buffer (2.9 minutes)
- [ ] Zero splices
- [ ] Maximum splices (300)
- [ ] Extreme parameter values
- [ ] Rapid parameter changes
- [ ] High CPU load scenarios

#### Expander Testing (if applicable)
- [ ] Expander connects correctly
- [ ] CONNECTED light illuminates
- [ ] Audio routes through expander
- [ ] Bit crusher operates correctly
- [ ] Filter operates correctly
- [ ] Mix controls work properly

#### File I/O
- [ ] Save reel to WAV file
- [ ] Load reel from WAV file
- [ ] JSON serialization (save patch)
- [ ] JSON deserialization (load patch)
- [ ] Marker positions persist correctly

#### Performance
- [ ] No audio dropouts
- [ ] CPU usage acceptable (<15% with 4 voices)
- [ ] Memory usage stable
- [ ] No memory leaks

---

### Regression Testing

When fixing bugs, always:
1. Reproduce the bug first
2. Fix the bug
3. Verify the fix
4. Test that nothing else broke

---

### Test Patches

Create test patches that demonstrate:
- The bug being fixed
- The feature being added
- Edge cases being handled

Include test patches in your PR description.

---

## Submission Process

### Pull Request Template

```markdown
## Description
Brief description of changes.

## Type of Change
- [ ] Bug fix (non-breaking change fixing an issue)
- [ ] New feature (non-breaking change adding functionality)
- [ ] Breaking change (fix or feature causing existing functionality to change)
- [ ] Documentation update

## Related Issues
Closes #123
Related to #456

## Motivation and Context
Why is this change needed? What problem does it solve?

## How Has This Been Tested?
Describe testing methodology:
- [ ] Manual testing in VCV Rack 2.5.1
- [ ] Tested on macOS 14.2
- [ ] Tested all parameter ranges
- [ ] Tested with expander
- [ ] Tested file I/O
- [ ] Tested CPU usage

## Screenshots / Videos
(If applicable)

## Checklist
- [ ] My code follows the project's style guidelines
- [ ] I have commented my code, particularly in complex areas
- [ ] I have updated documentation (if needed)
- [ ] My changes generate no new warnings
- [ ] I have tested all affected functionality
- [ ] I have checked for performance regressions
```

---

### Review Process

1. **Automated checks**: CI/CD runs (if configured)
2. **Code review**: Maintainer reviews code for:
   - Correctness
   - Style compliance
   - Performance
   - Documentation
3. **Testing**: Maintainer tests changes in VCV Rack
4. **Feedback**: Maintainer provides comments/requests changes
5. **Revision**: Contributor addresses feedback
6. **Approval**: Maintainer approves PR
7. **Merge**: PR merged into main branch

**Timeline**: Expect initial feedback within 1-2 weeks.

---

### After Your PR is Merged

- Your contribution will be included in the next release
- You'll be credited in the release notes
- Consider joining the [community](#community) for ongoing involvement

---

## Documentation

### Code Documentation

**Comment guidelines**:

```cpp
/**
 * Processes one sample of granular output.
 * 
 * @param buffer Audio buffer to read from
 * @param splice Current splice boundaries
 * @param position Playback position (updated by reference)
 * @param outL Left output sample (written by reference)
 * @param outR Right output sample (written by reference)
 * 
 * @note This function is called in the audio thread (real-time safe)
 */
void process(const TapestryBuffer& buffer, 
             const SpliceMarker& splice,
             double& position,
             float& outL,
             float& outR);
```

**When to comment**:
- Complex algorithms
- Non-obvious design decisions
- Performance-critical sections
- Public API functions
- Tricky edge cases

**When NOT to comment**:
```cpp
// ❌ Bad (obvious)
i++;  // Increment i

// ✅ Good (explains why)
i++;  // Skip the first sample to avoid click
```

---

### User Documentation

When adding features, update:
- `README.md` - If it's a major feature
- `docs/QUICKSTART.md` - If beginners need to know
- `docs/ADVANCED_USAGE.md` - If it's an advanced technique
- `docs/API_REFERENCE.md` - If it's a new API
- `docs/CHANGELOG.md` - Always update changelog

---

## Community

### Communication Channels

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and ideas
- **Email**: contact@shortwavlabs.com

### Getting Help

If you need help:
1. Check the [FAQ](FAQ.md)
2. Search existing issues/discussions
3. Ask in GitHub Discussions
4. Email the maintainer

---

## Roadmap

### Short Term (Next Release)

- [ ] Automated unit tests for DSP classes
- [ ] CI/CD pipeline (GitHub Actions)
- [ ] Additional waveform colors
- [ ] Performance profiling tools

### Medium Term (Next Few Releases)

- [ ] Additional filter types (high-pass, band-pass)
- [ ] Preset management system
- [ ] MIDI control support
- [ ] Multi-sample accurate clock sync

### Long Term (Future)

- [ ] Multi-channel expander (4+ channels)
- [ ] Advanced splice automation (curves, randomization)
- [ ] Real-time waveform analysis
- [ ] Network sync capabilities

---

## Recognition

### Contributors

All contributors are recognized in:
- Release notes
- CHANGELOG.md
- About dialog in the plugin (planned)

### Significant Contributions

Major contributions may result in:
- Co-authorship credit
- Dedicated acknowledgment in documentation
- Invitation to join the core team

---

## License

By contributing to Tapestry, you agree that your contributions will be licensed under the **GPL-3.0-or-later** license.

See [LICENSE](../LICENSE) for full details.

---

## Questions?

If you have questions about contributing, please:
- Open a GitHub Discussion
- Email: contact@shortwavlabs.com

**We appreciate your interest in improving Tapestry!** 🎵

---

*Last Updated: January 4, 2026*
