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
            c.toneZ = 0.0f;
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

        // Bias: a small DC offset (-0.3 .. 0.3) drives asymmetric waveshaping
        // for even-harmonic richness; the DC blocker removes it afterwards.
        biasOffset = (p.bias - 0.5f) * 0.6f;

        // Tone: one-pole lowpass from 800 Hz (dark) to ~9.6 kHz (bright).
        const float toneFreq = 800.0f * std::pow (12.0f, p.tone);
        toneCoef = std::exp (-2.0f * 3.14159265358979323846f * toneFreq / fs);

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
        const int   type   = p.type;
        const float drive  = p.drive;
        const float mix    = p.mix;
        const float output = p.output;

        for (int s = 0; s < numSamples; ++s)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float dry = (c == 0) ? inL[s] : inR[s];
                const float x   = dry + biasOffset;

                // Split into bands.
                float low[kMaxBands - 1];
                for (int j = 0; j < nx; ++j)
                    low[j] = ch[c].lp[static_cast<size_t> (j)].process (x);

                float wet = 0.0f;
                if (nb == 1)
                {
                    wet = transfer (x, type, drive);
                }
                else
                {
                    wet  = transfer (low[0], type, drive);                    // low band
                    for (int j = 1; j < nx; ++j)
                        wet += transfer (low[j] - low[j - 1], type, drive);   // mid bands
                    wet += transfer (x - low[nx - 1], type, drive);           // high band
                }

                // DC blocker (removes the bias offset's DC).
                const float d = wet - ch[c].dcX + 0.995f * ch[c].dcY;
                ch[c].dcX = wet;
                ch[c].dcY = d;

                // Tone (one-pole lowpass).
                const float t = (1.0f - toneCoef) * d + toneCoef * ch[c].toneZ;
                ch[c].toneZ = t;

                const float out = std::clamp ((dry * (1.0f - mix) + t * mix) * output, -1.0f, 1.0f);
                if (c == 0) outL[s] = out; else outR[s] = out;
            }
        }
    }

} // namespace asunder
