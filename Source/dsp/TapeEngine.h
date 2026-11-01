#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Asunder — analog tape / harmonic saturation engine.
//
// A full-band, JUCE-free saturator voiced after the Softube Harmonics Analog
// Saturation Processor and UAD Oxide Tape: four selectable "colors" (harmonic
// characters), gentle program-dependent saturation, a tape-style playback EQ
// (low head-bump + HF roll-off), optional wow/flutter and hiss, and a
// transient-preserving "punch" stage that keeps drum attacks clean and punchy
// while the body is softly saturated.
//
// It runs at whatever sample rate it is told (the processor drives it inside an
// oversampled block, so the nonlinearity is anti-aliased). Buffers are sized
// once for the maximum rate in prepare(); switching the oversampling factor only
// recomputes coefficients via setSampleRate() and never allocates.

namespace asunder
{
    static constexpr int kNumTapeColors = 4;

    enum TapeColorId { kTape = 0, kTube = 1, kFET = 2, kClassA = 3 };

    struct TapeParams
    {
        int   color  = kTape;   // 0..3
        float drive  = 0.35f;   // 0..1  saturation amount
        float bias   = 0.5f;    // 0..1  0.5 = symmetric; off-centre adds even harmonics
        float tone   = 0.6f;    // 0..1  HF roll-off: 0 dark .. 1 bright
        float bump   = 0.3f;    // 0..1  low-frequency head bump
        float punch  = 0.5f;    // 0..1  transient preservation (keep attacks clean)
        float wow    = 0.0f;    // 0..1  wow & flutter depth
        float noise  = 0.0f;    // 0..1  tape hiss
        float mix    = 1.0f;    // 0..1  dry/wet
        float output = 1.0f;    // 0..2  make-up / output gain

        bool operator== (const TapeParams& o) const
        {
            return color == o.color && drive == o.drive && bias == o.bias
                && tone == o.tone && bump == o.bump && punch == o.punch
                && wow == o.wow && noise == o.noise && mix == o.mix
                && output == o.output;
        }
        bool operator!= (const TapeParams& o) const { return ! (*this == o); }
    };

    class TapeEngine
    {
    public:
        // Allocate for the maximum rate the processor may run us at (= base * max
        // oversampling factor). Call once from prepareToPlay.
        void prepare (double maxSampleRate, int maxBlockSize);

        // Point the engine at the current (possibly oversampled) rate. Recomputes
        // coefficients only — never allocates — so it is safe to call per block.
        void setSampleRate (double sampleRate);

        void reset();
        void setParams (const TapeParams& p);

        // In-place safe (out may alias in). numSamples at the current rate.
        void process (const float* inL, const float* inR,
                      float* outL, float* outR, int numSamples);

    private:
        // --- biquad (transposed Direct Form II) ------------------------------
        struct Biquad
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
            float z1 = 0, z2 = 0;
            void reset() { z1 = z2 = 0.0f; }
            float process (float x)
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void setLowShelf (float fs, float freq, float gainDb);
        };

        struct Channel
        {
            Biquad bump;              // low head-bump shelf
            float  toneZ = 0.0f;      // one-pole HF roll-off state
            float  fastEnv = 0.0f;    // transient detector (fast)
            float  slowEnv = 0.0f;    // transient detector (slow)
            std::vector<float> delay; // wow/flutter delay line
            int    writePos = 0;
            uint32_t rng = 0x1234567u;
        };

        float saturate (float x, float effDrive) const;
        float nextNoise (Channel& c) const;

        double sampleRate = 48000.0;
        double maxRate     = 48000.0 * 8.0;
        TapeParams p;

        // Cached, rate-dependent coefficients.
        float toneCoef   = 0.0f;   // one-pole LP coefficient
        float aFast = 0, rFast = 0, aSlow = 0, rSlow = 0;  // detector coeffs
        float wowInc = 0.0f, flutInc = 0.0f;               // LFO increments (rad/sample)
        float wowPhase = 0.0f, flutPhase = 0.0f;
        int   baseDelay = 0;       // centre tap for modulated delay (samples)
        float makeup = 1.0f;

        Channel ch[2];
    };

} // namespace asunder
