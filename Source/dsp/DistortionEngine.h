#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Distortion.h"

// Asunder — multiband distortion / waveshaping engine.
//
// The input is optionally split into up to 5 bands using cascaded 2nd-order
// Butterworth lowpasses (band i = low[i] - low[i-1], top band = x - low[last]),
// so the bands sum back to the input exactly. Each band runs the same waveshaper
// (drive + type) independently, then the bands are summed, DC-blocked (to remove
// the bias offset), tonally tilted, and mixed with the dry signal.

namespace asunder
{
    static constexpr int kMaxBands = 5;

    struct Params
    {
        std::array<float, kMaxBands> drive {};   // 0..1  per band
        std::array<float, kMaxBands> tone  {};   // 0 dark .. 1 bright, per band
        std::array<float, kMaxBands> bias  {};   // 0.5 symmetric, per band
        std::array<int,   kMaxBands> type  {};   // 0..5, per band
        float mix    = 1.0f;     // 0..1  global
        float output = 1.0f;     // 0..2  global
        int   bands  = 1;        // 1..5
        float xover0 = 300.0f;   // Hz
        float xover1 = 1000.0f;
        float xover2 = 3000.0f;
        float xover3 = 8000.0f;

        Params()
        {
            drive.fill (0.5f);
            tone.fill  (0.5f);
            bias.fill  (0.5f);
            type.fill  (2);
        }

        bool operator== (const Params& o) const
        {
            for (int i = 0; i < kMaxBands; ++i)
                if (drive[i] != o.drive[i] || tone[i]  != o.tone[i]
                    || bias[i] != o.bias[i] || type[i] != o.type[i])
                    return false;

            return mix    == o.mix
                && output == o.output
                && bands  == o.bands
                && xover0 == o.xover0
                && xover1 == o.xover1
                && xover2 == o.xover2
                && xover3 == o.xover3;
        }
    };

    class DistortionEngine
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void setParams (const Params& p);
        void process (const float* inL, const float* inR,
                      float* outL, float* outR, int numSamples);

    private:
        struct Biquad
        {
            float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
            float z1 = 0, z2 = 0;

            void reset() { z1 = z2 = 0.0f; }

            float process (float x)
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }

            void setLowpass (float fs, float freq)
            {
                const float w0    = 2.0f * 3.14159265358979323846f * freq / fs;
                const float cw    = std::cos (w0);
                const float alpha = std::sin (w0) * 0.5f;   // Q = 0.7071 (Butterworth)
                const float a0    = 1.0f + alpha;
                b0 = (1.0f - cw) * 0.5f / a0;
                b1 = (1.0f - cw) / a0;
                b2 = b0;
                a1 = (-2.0f * cw) / a0;
                a2 = (1.0f - alpha) / a0;
            }
        };

        struct Channel
        {
            std::array<Biquad, kMaxBands - 1> lp;       // crossover lowpasses
            std::array<float, kMaxBands> toneZ {};      // per-band one-pole lowpass state
            float dcX = 0.0f;                            // DC blocker prev input
            float dcY = 0.0f;                            // DC blocker prev output
        };

        void update (double sr);
        float processBand (float x, int band, int chIdx);

        double sampleRate = 48000.0;
        Params p;
        int activeBands = 1;
        std::array<float, kMaxBands> biasOffset {};
        std::array<float, kMaxBands> toneCoef {};
        std::array<float, kMaxBands - 1> xover {};

        Channel ch[2];
    };

} // namespace asunder
