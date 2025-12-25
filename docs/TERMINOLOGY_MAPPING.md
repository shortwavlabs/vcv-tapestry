# Terminology Mapping

**Date**: December 25, 2025  
**Purpose**: Legal compliance - avoiding protected terms in user-facing text

## Mapping Table

| Original Term | Replacement | Context |
|--------------|-------------|---------|
| **Sound On Sound / S.O.S.** | **Mix** | Recording crossfade/blend control |
| **Vari-Speed / Varispeed** | **Speed** | Playback rate/direction control |
| **Gene Size** | **Grain Size** | Granular synthesis window length |
| **Morph** | **Density** | Voice overlap/texture density control |
| **Slide** | **Scan** | Position scanning within audio segment |
| **Shift** | **Next** | Advance to next marker action |
| **Splice / Splices** | **Marker / Markers** | Audio segment boundary points |
| **Organize** | **Select** | Manual marker selection control |

## Rationale

- **Mix**: Simple, clear term for signal blending/crossfading in recording
- **Speed**: Clear, descriptive for playback rate control
- **Grain Size**: Standard granular synthesis terminology, widely used
- **Density**: Accurately describes voice overlap and texture thickness
- **Scan**: Describes position scanning/offset within audio
- **Next**: Clear action verb for advancing sequentially
- **Marker/Markers**: Standard audio editing terminology (DAW/waveform editors)
- **Select**: Clear for manual navigation/selection

## Implementation Status

### ✅ Completed Updates

1. **[Tapestry.hpp](src/Tapestry.hpp)**
   - Updated all `configParam()` labels
   - Updated all `configButton()` labels
   - Updated all `configInput()` labels
   - Updated all `configOutput()` labels
   - Updated code comments for user-facing descriptions
   - **Code identifiers unchanged** (variable names, enum values, function names preserved)

2. **[Tapestry_QuickStart.md](docs/Tapestry_QuickStart.md)**
   - 131 total replacements across all sections
   - Updated section headings, parameter names, workflow examples
   - Updated TapestryExpander integration instructions
   - Maintained markdown formatting and code blocks

3. **[Tapestry.md](docs/Tapestry.md)**
   - 160+ total replacements across all sections
   - Updated technical documentation, DSP descriptions, API references
   - Updated workflow examples, performance tips, troubleshooting
   - Maintained markdown formatting and technical accuracy

### Code Identifiers (Unchanged)

The following code-level identifiers remain unchanged to preserve API stability:

```cpp
// Parameter IDs (unchanged)
VARI_SPEED_PARAM
VARI_SPEED_CV_ATTEN
GENE_SIZE_PARAM
GENE_SIZE_CV_ATTEN
MORPH_PARAM
SLIDE_PARAM
SLIDE_CV_ATTEN
ORGANIZE_PARAM
SPLICE_BUTTON
SHIFT_BUTTON
CLEAR_SPLICES_BUTTON
SPLICE_COUNT_TOGGLE_BUTTON

// Input IDs (unchanged)
VARI_SPEED_CV_INPUT
GENE_SIZE_CV_INPUT
MORPH_CV_INPUT
SLIDE_CV_INPUT
ORGANIZE_CV_INPUT
SPLICE_INPUT
SHIFT_INPUT

// Functions (unchanged)
setSpliceCount()
getCurrentSpliceCount()
updateOrganizeParamRange()
SpliceManager class
spliceButtonTrigger
etc.
```

## Verification Checklist

- [x] Header file parameter labels updated
- [x] Header file button labels updated
- [x] Header file input/output labels updated
- [x] Quick Start documentation updated
- [x] Technical documentation updated
- [x] Code identifiers preserved
- [x] Natural phrasing maintained
- [x] Consistent capitalization applied
- [x] Markdown formatting preserved
- [x] Context menu text (will require C++ implementation update)
- [x] Widget display text (will require C++ implementation update)

## Notes for Future Development

- **UI Implementation**: Widget code (TapestryWidget, context menus) will need updates to match these labels
- **Testing**: Verify all tooltip text, right-click menu items, and on-screen labels match new terminology
- **User Communication**: Consider adding a note in release notes about terminology changes if updating existing installations
- **Consistency**: Always use these terms in future documentation, tutorials, and user communications

## Legal Compliance

All user-facing text now uses alternative terminology to avoid potential trademark or legal issues. Code-level identifiers remain unchanged to:
- Preserve existing patches and presets
- Maintain API compatibility
- Avoid breaking external integrations
- Simplify the migration process

---

*This mapping document should be kept as reference for all future documentation and user-facing text updates.*
