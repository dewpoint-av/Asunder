# Asunder Distortion Plugin

A professional-grade audio distortion plugin built with JUCE, featuring a sleek dark mode interface with holographic visual effects.

## Features

### Audio Processing
- **6 Distortion Types**: Hard Clip, Soft Clip, Tube, Wave Fold, Bit Crush, Sample Rate Reduction
- **Drive Control**: 0-100% drive amount for intensity control
- **Mix Control**: 0-100% wet/dry mix
- **High-Quality DSP**: Professional-grade distortion algorithms

### User Interface
- **Dark Mode Design**: Sleek black interface with subtle grid patterns
- **Moog-Style Knobs**: Custom rotary controls with holographic rim lighting
- **GLSL Visualizer**: Real-time frequency spectrum visualization with holographic effects
- **Waveshape Visualizer**: Live input/output waveform comparison
- **Holographic Accents**: Subtle iridescent lighting effects throughout the interface

### Technical Specifications
- **Format**: VST3, Standalone
- **Platform**: Windows (x64)
- **Framework**: JUCE 6.x
- **C++ Standard**: C++17
- **Build System**: CMake

## Building

### Prerequisites
- Visual Studio 2022 with C++ development tools
- JUCE framework (automatically downloaded by CMake)
- CMake 3.15 or later

### Build Steps
1. Clone or download this project
2. Open Command Prompt in the project directory
3. Run `build.bat`
4. The plugin will be built in `build/bin/Release/`

### Manual Build
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Installation

After building, copy the VST3 plugin from `build/bin/Release/` to your DAW's VST3 plugin folder:
- **Default VST3 Location**: `C:\Program Files\Common Files\VST3\`
- **User VST3 Location**: `%USERPROFILE%\Documents\VST3 presets\`

## Usage

1. Load the plugin in your DAW
2. Use the **Drive** knob to control distortion intensity
3. Use the **Mix** knob to blend between dry and distorted signals
4. Select from 6 different distortion types using the dropdown
5. Watch the real-time visualizers respond to your settings

## Visual Effects

### Holographic Rim Lighting
- Subtle, dark mode-optimized iridescent effects
- Slowly changing color phases for a premium feel
- Multiple layered rings with varying transparency

### GLSL Visualizer
- 64-band frequency spectrum display
- Colors respond to drive and mix parameters
- Smooth animations with holographic gradients

### Waveshape Visualizer
- Real-time input vs output waveform comparison
- Grid overlay for precise measurement
- Holographic color coding for the output signal

## Customization

The interface is designed to be easily customizable:
- Colors and effects can be adjusted in the respective component files
- Animation speeds can be modified via timer frequencies
- Layout can be adjusted in the `PluginEditor::resized()` method

## License

This project is provided as-is for educational and development purposes.

## Credits

Built with the JUCE framework - the industry standard for audio plugin development.
