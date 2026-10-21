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
        float drive  = 0.5f;     // 0..1
        float tone   = 0.5f;     // 0 dark .. 1 bright
        float bias   = 0.5f;     // 0.5 symmetric
        float mix    = 1.0f;     // 0..1
        float output = 1.0f;     // 0..2
        int   type   = 2;        // 0..5
        int   bands  = 1;        // 1..5
        float xover0 = 300.0f;   // Hz
        float xover1 = 1000.0f;
        float xover2 = 3000.0f;
        float xover3 = 8000.0f;

        bool operator== (const Params& o) const
        {
            return drive  == o.drive
                && tone   == o.tone
                && bias   == o.bias
                && mix    == o.mix
                && output == o.output
                && type   == o.type
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
            std::array<Biquad, kMaxBands - 1> lp;   // crossover lowpasses
            float toneZ = 0.0f;                     // one-pole lowpass state
            float dcX   = 0.0f;                     // DC blocker prev input
            float dcY   = 0.0f;                     // DC blocker prev output
        };

        void update (double sr);

        double sampleRate = 48000.0;
        Params p;
        int activeBands = 1;
        float biasOffset = 0.0f;
        float toneCoef = 0.0f;
        std::array<float, kMaxBands - 1> xover {};

        Channel ch[2];
    };

} // namespace asunder
