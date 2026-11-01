#include "TapeVisualizer.h"

#include "AsunderLookAndFeel.h"

#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    float levelToDb(float lin) { return 20.0f * std::log10 (juce::jmax (1.0e-4f, lin)); }
}

TapeVisualizer::TapeVisualizer(AsunderAudioProcessor& p) : processor(p)
{
    setOpaque (true);
    waveLen = juce::jmin ((int) wave.size(), processor.getWaveformCapacity());
    startTimerHz (30);
}

TapeVisualizer::~TapeVisualizer() { stopTimer(); }

void TapeVisualizer::timerCallback()
{
    reelPhase += 0.08f;
    if (reelPhase > 2.0f * kPi) reelPhase -= 2.0f * kPi;
    waveLen = processor.copyWaveform (wave.data(), (int) wave.size());
    repaint();
}

void TapeVisualizer::drawVU(juce::Graphics& g, juce::Rectangle<float> r, float level, const juce::String& label)
{
    // Panel.
    g.setColour (AsunderLookAndFeel::track);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (AsunderLookAndFeel::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);

    const float cx = r.getCentreX();
    const float cy = r.getBottom() - r.getHeight() * 0.28f;
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.52f;

    // Scale arc (-40..+3 dB mapped to +/- 52 deg).
    const float a0 = juce::degreesToRadians (-52.0f);
    const float a1 = juce::degreesToRadians (52.0f);

    juce::Path arc;
    arc.addCentredArc (cx, cy, radius, radius, 0.0f, a0, a1, true);
    g.setColour (AsunderLookAndFeel::muted.withAlpha (0.6f));
    g.strokePath (arc, juce::PathStrokeType (1.4f));

    // Ticks.
    for (int i = 0; i <= 6; ++i)
    {
        const float t   = (float) i / 6.0f;
        const float ang = a0 + t * (a1 - a0);
        const float r1  = radius - 4.0f;
        const float r2  = radius + 1.0f;
        g.setColour ((t > 0.72f ? juce::Colour (0xffff5a5a) : AsunderLookAndFeel::muted).withAlpha (0.8f));
        g.drawLine (cx + r1 * std::sin (ang), cy - r1 * std::cos (ang),
                    cx + r2 * std::sin (ang), cy - r2 * std::cos (ang), 1.2f);
    }

    // Needle: map dB [-36..+3] to the arc.
    const float db = levelToDb (level);
    const float tn = juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 39.0f);
    const float ang = a0 + tn * (a1 - a0);
    g.setColour (accent.withAlpha (0.35f));
    g.drawLine (cx, cy, cx + radius * std::sin (ang), cy - radius * std::cos (ang), 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.drawLine (cx, cy, cx + (radius - 2.0f) * std::sin (ang), cy - (radius - 2.0f) * std::cos (ang), 1.6f);
    g.setColour (accent);
    g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);

    // Label.
    g.setColour (AsunderLookAndFeel::muted);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (label + "  VU", r.removeFromTop (16.0f), juce::Justification::centred, false);
}

void TapeVisualizer::drawReel(juce::Graphics& g, float cx, float cy, float radius, float phase)
{
    g.setColour (AsunderLookAndFeel::border);
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.4f);
    g.setColour (accent.withAlpha (0.5f));
    g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    for (int i = 0; i < 3; ++i)
    {
        const float a = phase + (float) i * (2.0f * kPi / 3.0f);
        g.setColour (AsunderLookAndFeel::muted.withAlpha (0.55f));
        g.drawLine (cx, cy, cx + radius * 0.85f * std::sin (a), cy - radius * 0.85f * std::cos (a), 1.4f);
    }
}

void TapeVisualizer::drawWaveform(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (AsunderLookAndFeel::track);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (AsunderLookAndFeel::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);

    const float cy = r.getCentreY();
    g.setColour (AsunderLookAndFeel::muted.withAlpha (0.4f));
    g.drawLine (r.getX() + 6.0f, cy, r.getRight() - 6.0f, cy, 1.0f);

    if (waveLen < 2)
        return;

    const float x0 = r.getX() + 6.0f;
    const float w  = r.getWidth() - 12.0f;
    const float halfH = r.getHeight() * 0.42f;

    juce::Path p;
    bool first = true;
    for (int i = 0; i < waveLen; ++i)
    {
        const float x = x0 + w * ((float) i / (float) (waveLen - 1));
        const float y = cy - juce::jlimit (-1.0f, 1.0f, wave[(size_t) i]) * halfH;
        if (first) { p.startNewSubPath (x, y); first = false; }
        else       p.lineTo (x, y);
    }
    g.setColour (accent.withAlpha (0.9f));
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void TapeVisualizer::paint(juce::Graphics& g)
{
    g.fillAll (AsunderLookAndFeel::background);

    auto r = getLocalBounds().toFloat().reduced (8.0f);

    // Top half: two VU meters flanking a pair of reels.
    auto top = r.removeFromTop (r.getHeight() * 0.56f);
    r.removeFromTop (8.0f);

    const float vuW = top.getWidth() * 0.34f;
    drawVU (g, top.removeFromLeft (vuW).reduced (2.0f), processor.getInLevel(),  "IN");
    auto centre = top.removeFromLeft (top.getWidth() - vuW);
    drawVU (g, top.reduced (2.0f), processor.getOutLevel(), "OUT");

    // Reels between the meters.
    const float reelR = juce::jmin (centre.getWidth(), centre.getHeight()) * 0.26f;
    const float ry = centre.getCentreY();
    drawReel (g, centre.getCentreX() - reelR * 1.4f, ry, reelR,  reelPhase);
    drawReel (g, centre.getCentreX() + reelR * 1.4f, ry, reelR, -reelPhase * 0.6f);
    // Tape path line between reels.
    g.setColour (AsunderLookAndFeel::muted.withAlpha (0.4f));
    g.drawLine (centre.getCentreX() - reelR * 1.4f, ry - reelR,
                centre.getCentreX() + reelR * 1.4f, ry - reelR, 1.2f);

    drawWaveform (g, r);
}
