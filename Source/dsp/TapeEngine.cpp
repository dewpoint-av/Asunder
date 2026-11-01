#include "TapeEngine.h"

#include <algorithm>

namespace asunder
{
    static constexpr float kPi = 3.14159265358979323846f;

    void TapeEngine::Biquad::setLowShelf (float fs, float freq, float gainDb)
    {
        // RBJ low-shelf.
        const float A  = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * freq / fs;
        const float cw = std::cos (w0);
        const float sw = std::sin (w0);
        const float S  = 0.9f;                       // shelf slope
        const float alpha = sw * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
        const float twoSqrtAalpha = 2.0f * std::sqrt (A) * alpha;

        const float nb0 =        A * ((A + 1.0f) - (A - 1.0f) * cw + twoSqrtAalpha);
        const float nb1 =  2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
        const float nb2 =        A * ((A + 1.0f) - (A - 1.0f) * cw - twoSqrtAalpha);
        const float na0 =             (A + 1.0f) + (A - 1.0f) * cw + twoSqrtAalpha;
        const float na1 = -2.0f *    ((A - 1.0f) + (A + 1.0f) * cw);
        const float na2 =             (A + 1.0f) + (A - 1.0f) * cw - twoSqrtAalpha;

        this->b0 = nb0 / na0; this->b1 = nb1 / na0; this->b2 = nb2 / na0;
        this->a1 = na1 / na0; this->a2 = na2 / na0;
    }

    void TapeEngine::prepare (double maxSampleRate, int /*maxBlockSize*/)
    {
        maxRate = maxSampleRate;

        // Delay line big enough for the deepest wow + flutter excursion at the
        // highest rate (~6 ms either side of a 7 ms centre tap).
        const int len = static_cast<int> (std::ceil (0.03 * maxRate)) + 8;
        for (auto& c : ch)
        {
            c.delay.assign (static_cast<size_t> (len), 0.0f);
            c.writePos = 0;
        }

        reset();
        setSampleRate (maxSampleRate);
    }

    void TapeEngine::reset()
    {
        for (auto& c : ch)
        {
            c.bump.reset();
            c.toneZ = c.fastEnv = c.slowEnv = 0.0f;
            std::fill (c.delay.begin(), c.delay.end(), 0.0f);
            c.writePos = 0;
        }
        wowPhase = flutPhase = 0.0f;
        ch[0].rng = 0x1234567u;
        ch[1].rng = 0x89abcdefu;
    }

    void TapeEngine::setParams (const TapeParams& np)
    {
        if (np == p)
            return;
        const bool toneOrEq = (np.tone != p.tone) || (np.bump != p.bump);
        p = np;
        if (toneOrEq)
            setSampleRate (sampleRate);   // refresh EQ coeffs
        makeup = 1.0f + p.drive * 0.6f;   // gentle level compensation as drive rises
    }

    void TapeEngine::setSampleRate (double sr)
    {
        sampleRate = sr;
        const float fs = static_cast<float> (sr);

        // Tone: one-pole LP, cutoff 2 kHz (dark) .. 20 kHz (bright).
        const float toneFreq = 2000.0f * std::pow (10.0f, p.tone);   // 2k .. 20k
        toneCoef = std::exp (-2.0f * kPi * std::min (toneFreq, fs * 0.45f) / fs);

        // Low head-bump shelf around 90 Hz, up to +5 dB.
        const float bumpDb = p.bump * 5.0f;
        for (auto& c : ch)
            c.bump.setLowShelf (fs, 90.0f, bumpDb);

        // Transient detector ballistics.
        const auto coeff = [fs] (float ms) { return 1.0f - std::exp (-1.0f / (0.001f * ms * fs)); };
        aFast = coeff (0.5f);  rFast = coeff (30.0f);
        aSlow = coeff (12.0f); rSlow = coeff (160.0f);

        // Wow (~0.7 Hz) + flutter (~6 Hz) LFO increments.
        wowInc  = 2.0f * kPi * 0.7f / fs;
        flutInc = 2.0f * kPi * 6.0f / fs;
        baseDelay = static_cast<int> (0.007 * sr);   // 7 ms centre tap

        makeup = 1.0f + p.drive * 0.6f;
    }

    // Per-channel white noise via xorshift, scaled small (tape hiss).
    float TapeEngine::nextNoise (Channel& c) const
    {
        c.rng ^= c.rng << 13; c.rng ^= c.rng >> 17; c.rng ^= c.rng << 5;
        const float n = (static_cast<float> (c.rng) / 2147483648.0f) - 1.0f; // -1..1
        return n;
    }

    // Colour-dependent saturation. x is pre-gained input, effDrive is the
    // transient-adjusted drive (0..1). Output ~[-1,1], harmonic flavour per colour.
    float TapeEngine::saturate (float x, float effDrive) const
    {
        const float g = 1.0f + effDrive * 9.0f;      // 1x .. 10x
        const float b = (p.bias - 0.5f);             // -0.5 .. 0.5

        switch (p.color)
        {
            case kTube:   // triode: asymmetric, strong 2nd harmonic
            {
                const float xb = x * g + b * 0.9f + 0.12f;
                const float s  = std::tanh (xb);
                const float y  = s + 0.22f * (s * s - 0.33f);   // even-harmonic lift
                return y;
            }
            case kFET:    // tighter odd-harmonic drive, punchy
            {
                const float xb = x * g + b * 0.4f;
                const float a  = std::fabs (xb);
                return xb / std::pow (1.0f + std::pow (a, 2.5f), 1.0f / 2.5f);
            }
            case kClassA: // strongly asymmetric low-order grit
            {
                const float xb = x * g + b * 0.7f + 0.2f;
                return (xb >= 0.0f) ? std::tanh (xb)
                                    : 0.75f * std::tanh (xb * 1.3f);
            }
            case kTape:
            default:      // warm symmetric tape with a touch of even harmonic
            {
                const float xb = x * g + b * 0.6f;
                const float s  = std::tanh (xb);
                return s - 0.12f * (s * s - 0.5f);
            }
        }
    }

    void TapeEngine::process (const float* inL, const float* inR,
                              float* outL, float* outR, int numSamples)
    {
        const float mix    = p.mix;
        const float output = p.output * makeup;
        const float wowDepthSamples = p.wow * 0.006f * static_cast<float> (sampleRate); // up to 6 ms
        const float hiss   = p.noise * 0.02f;   // gentle
        const float punch  = p.punch;

        for (int s = 0; s < numSamples; ++s)
        {
            const float dryL = inL[s];
            const float dryR = inR[s];

            // --- linked transient detection (max of |L|,|R|) -----------------
            const float det = std::max (std::fabs (dryL), std::fabs (dryR));
            float& fe = ch[0].fastEnv;   // share one linked detector (ch[0] holds it)
            float& se = ch[0].slowEnv;
            fe += (det - fe) * (det > fe ? aFast : rFast);
            se += (det - se) * (det > se ? aSlow : rSlow);
            const float trans = std::clamp ((fe - se) * 3.0f, 0.0f, 1.0f);

            const float effDrive = p.drive * (1.0f - 0.8f * punch * trans);
            const float cleanAmt = 0.9f * punch * trans;   // crossfade attacks to clean

            // --- wow/flutter LFO (shared) ------------------------------------
            wowPhase  += wowInc;  if (wowPhase  > 2.0f * kPi) wowPhase  -= 2.0f * kPi;
            flutPhase += flutInc; if (flutPhase > 2.0f * kPi) flutPhase -= 2.0f * kPi;
            const float modSamples = wowDepthSamples * (0.85f * std::sin (wowPhase)
                                                        + 0.15f * std::sin (flutPhase));

            for (int c = 0; c < 2; ++c)
            {
                Channel& chan = ch[c];
                const float dry = (c == 0) ? dryL : dryR;

                // Wow/flutter: write to delay, read back at modulated fractional tap.
                float in = dry;
                if (wowDepthSamples > 0.0f && ! chan.delay.empty())
                {
                    const int sz = static_cast<int> (chan.delay.size());
                    chan.delay[static_cast<size_t> (chan.writePos)] = dry;
                    float readPos = static_cast<float> (chan.writePos)
                                    - static_cast<float> (baseDelay) + modSamples;
                    while (readPos < 0.0f)               readPos += static_cast<float> (sz);
                    while (readPos >= static_cast<float> (sz)) readPos -= static_cast<float> (sz);
                    const int   i0 = static_cast<int> (readPos);
                    const int   i1 = (i0 + 1) % sz;
                    const float fr = readPos - static_cast<float> (i0);
                    in = chan.delay[static_cast<size_t> (i0)] * (1.0f - fr)
                       + chan.delay[static_cast<size_t> (i1)] * fr;
                    chan.writePos = (chan.writePos + 1) % sz;
                }

                // Saturate, then keep transients clean (crossfade to pre-sat).
                float shaped = saturate (in, effDrive);
                shaped = shaped * (1.0f - cleanAmt) + in * cleanAmt;

                // Tape hiss.
                if (hiss > 0.0f)
                    shaped += nextNoise (chan) * hiss;

                // Playback EQ: low head-bump then HF roll-off.
                shaped = chan.bump.process (shaped);
                chan.toneZ = (1.0f - toneCoef) * shaped + toneCoef * chan.toneZ;
                shaped = chan.toneZ;

                const float wet = shaped * output;
                const float out = std::clamp (dry * (1.0f - mix) + wet * mix, -1.0f, 1.0f);
                if (c == 0) outL[s] = out; else outR[s] = out;
            }
        }
    }

} // namespace asunder
