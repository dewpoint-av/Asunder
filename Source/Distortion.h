#pragma once

#include <algorithm>
#include <cmath>

// Shared distortion transfer functions. Each maps an input sample (roughly
// -1..1) to an output in the same range; `drive` is 0..1. They are pure
// (stateless), so the DSP and the waveshape visualizer render the exact same
// curves. Sample-rate reduction is the one stateful process and lives in the
// processor; its static staircase shape is approximated here for display.

namespace asunder
{
    static constexpr int kNumDistortionTypes = 6;

    inline float driveGain (float drive)
    {
        return 1.0f + drive * 9.0f;   // 1x .. 10x
    }

    inline float hardClip (float x, float drive)
    {
        return std::clamp (x * driveGain (drive), -1.0f, 1.0f);
    }

    inline float softClip (float x, float drive)
    {
        const float d = x * driveGain (drive);
        return (2.0f / 3.14159265358979323846f) * std::atan (d);
    }

    inline float tube (float x, float drive)
    {
        return std::tanh (x * driveGain (drive));
    }

    inline float waveFold (float x, float drive)
    {
        float d = x * driveGain (drive);
        // Bounded triangle fold (a handful of iterations covers any audio range).
        for (int i = 0; i < 8 && (d > 1.0f || d < -1.0f); ++i)
            d = (d > 1.0f) ? 2.0f - d : -2.0f - d;
        return std::clamp (d, -1.0f, 1.0f);
    }

    inline float bitCrush (float x, float drive)
    {
        const float bits   = 16.0f - drive * 12.0f;        // 16 .. 4 bit
        const float levels = std::pow (2.0f, std::max (1.0f, bits));
        return std::round (x * levels) / levels;
    }

    // Sample-rate reduction, approximated as a staircase for the visualizer.
    inline float sampleReduceShape (float x, float drive)
    {
        const float steps = 4.0f + drive * 28.0f;          // 4 .. 32 hold levels
        return std::round ((x + 1.0f) * 0.5f * steps) / steps * 2.0f - 1.0f;
    }

    inline float transfer (float x, int type, float drive)
    {
        switch (type)
        {
            case 0:  return hardClip          (x, drive);
            case 1:  return softClip          (x, drive);
            case 2:  return tube              (x, drive);
            case 3:  return waveFold          (x, drive);
            case 4:  return bitCrush          (x, drive);
            case 5:  return sampleReduceShape (x, drive);
            default: return x;
        }
    }
}
