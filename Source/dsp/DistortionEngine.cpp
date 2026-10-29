#include "DistortionEngine.h"

namespace asunder
{
    void DistortionEngine::prepare (double sr, int /*maxBlockSize*/)
    {
        sampleRate = sr;
        reset();
        update (sr);
    }

    void DistortionEngine::reset()
    {
        for (auto& c : ch)
        {
            for (auto& f : c.lp)
                f.reset();
            c.toneZ.fill (0.0f);
            c.dcX = 0.0f;
            c.dcY = 0.0f;
        }
    }

    void DistortionEngine::setParams (const Params& np)
    {
        if (np == p)
            return;
        p = np;
        update (sampleRate);
    }

    void DistortionEngine::update (double sr)
    {
        const float fs = static_cast<float> (sr);

        activeBands = std::clamp (p.bands, 1, kMaxBands);

        // Per-band bias: a small DC offset (-0.3 .. 0.3) drives asymmetric
        // waveshaping for even-harmonic richness; the DC blocker removes it
        // afterwards.
        for (int b = 0; b < kMaxBands; ++b)
            biasOffset[static_cast<size_t> (b)] = (p.bias[static_cast<size_t> (b)] - 0.5f) * 0.6f;

        // Per-band tone: one-pole lowpass from 800 Hz (dark) to ~9.6 kHz (bright).
        for (int b = 0; b < kMaxBands; ++b)
        {
            const float toneFreq = 800.0f * std::pow (12.0f, p.tone[static_cast<size_t> (b)]);
            toneCoef[static_cast<size_t> (b)] = std::exp (-2.0f * 3.14159265358979323846f * toneFreq / fs);
        }

        // Sort the crossovers and configure the lowpasses.
        std::array<float, kMaxBands - 1> raw { p.xover0, p.xover1, p.xover2, p.xover3 };
        std::sort (raw.begin(), raw.end());

        for (int i = 0; i < kMaxBands - 1; ++i)
        {
            xover[static_cast<size_t> (i)] = raw[static_cast<size_t> (i)];
            for (auto& c : ch)
                c.lp[static_cast<size_t> (i)].setLowpass (fs, raw[static_cast<size_t> (i)]);
        }
    }

    void DistortionEngine::process (const float* inL, const float* inR,
                                    float* outL, float* outR, int numSamples)
    {
        const int   nb     = activeBands;
        const int   nx     = nb - 1;      // active crossovers
        const float mix    = p.mix;
        const float output = p.output;

        for (int s = 0; s < numSamples; ++s)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float dry = (c == 0) ? inL[s] : inR[s];

                // Split into bands (no bias yet — bias is applied per band).
                float low[kMaxBands - 1];
                for (int j = 0; j < nx; ++j)
                    low[j] = ch[c].lp[static_cast<size_t> (j)].process (dry);

                float wet = 0.0f;
                if (nb == 1)
                {
                    wet = processBand (dry, 0, c);
                }
                else
                {
                    wet  = processBand (low[0], 0, c);                       // low band
                    for (int j = 1; j < nx; ++j)
                        wet += processBand (low[j] - low[j - 1], j, c);      // mid bands
                    wet += processBand (dry - low[nx - 1], nx, c);           // high band
                }

                // DC blocker (removes the summed bias offsets' DC).
                const float d = wet - ch[c].dcX + 0.995f * ch[c].dcY;
                ch[c].dcX = wet;
                ch[c].dcY = d;

                const float out = std::clamp ((dry * (1.0f - mix) + d * mix) * output, -1.0f, 1.0f);
                if (c == 0) outL[s] = out; else outR[s] = out;
            }
        }
    }

    float DistortionEngine::processBand (float x, int band, int chIdx)
    {
        // Per-band bias offset, then the waveshaper, then the per-band tone.
        const float biased = x + biasOffset[static_cast<size_t> (band)];
        const float shaped = transfer (biased, p.type[static_cast<size_t> (band)],
                                       p.drive[static_cast<size_t> (band)]);

        const float coef = toneCoef[static_cast<size_t> (band)];
        const float t = (1.0f - coef) * shaped + coef * ch[chIdx].toneZ[static_cast<size_t> (band)];
        ch[chIdx].toneZ[static_cast<size_t> (band)] = t;
        return t;
    }

} // namespace asunder
