#include "WaveshapeVisualizer.h"

#include "Distortion.h"
#include "AsunderLookAndFeel.h"

namespace
{
    juce::String perBandId (int bandOneBased, const char* what)
    {
        return "b" + juce::String(bandOneBased) + what;
    }
}

WaveshapeVisualizer::WaveshapeVisualizer(juce::AudioProcessorValueTreeState& s)
    : state(s)
{
    // Listen to the selected band's type/drive (retarget on setBand).
    for (int b = 1; b <= 5; ++b)
    {
        state.addParameterListener(perBandId(b, "type"), this);
        state.addParameterListener(perBandId(b, "drive"), this);
    }
}

WaveshapeVisualizer::~WaveshapeVisualizer()
{
    for (int b = 1; b <= 5; ++b)
    {
        state.removeParameterListener(perBandId(b, "type"), this);
        state.removeParameterListener(perBandId(b, "drive"), this);
    }
}

void WaveshapeVisualizer::setBand(int bandOneBased)
{
    band = juce::jlimit(1, 5, bandOneBased);
    repaint();
}

juce::String WaveshapeVisualizer::typeParamId() const  { return perBandId(band, "type"); }
juce::String WaveshapeVisualizer::driveParamId() const { return perBandId(band, "drive"); }

float WaveshapeVisualizer::drive() const
{
    if (auto* p = state.getRawParameterValue(driveParamId()))
        return p->load();
    return 0.5f;
}

int WaveshapeVisualizer::type() const
{
    if (auto* p = state.getRawParameterValue(typeParamId()))
        return juce::roundToInt(p->load());
    return 2;
}

void WaveshapeVisualizer::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(8.0f);
    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float w  = r.getWidth();
    const float h  = r.getHeight();

    // Map input x in [-1,1] -> [left,right]; output y in [-1,1] -> [bottom,top].
    const auto toX = [&](float x) { return cx + x * (w * 0.5f); };
    const auto toY = [&](float y) { return cy - y * (h * 0.5f); };

    // Frame + crosshair.
    g.setColour(AsunderLookAndFeel::track);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(AsunderLookAndFeel::border);
    g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.5f);
    g.drawLine(r.getX(), cy, r.getRight(), cy, 1.0f);
    g.drawLine(cx, r.getY(), cx, r.getBottom(), 1.0f);

    // Dry reference line (y = x).
    juce::Path dry;
    dry.startNewSubPath(toX(-1.0f), toY(-1.0f));
    dry.lineTo(toX(1.0f), toY(1.0f));
    g.setColour(AsunderLookAndFeel::muted.withAlpha(0.55f));
    g.strokePath(dry, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Transfer curve.
    const int  N = 200;
    const int  t = type();
    const float d = drive();

    juce::Path curve;
    bool first = true;
    for (int i = 0; i <= N; ++i)
    {
        const float x = -1.0f + (2.0f * static_cast<float>(i)) / N;
        const float y = asunder::transfer(x, t, d);
        if (first) { curve.startNewSubPath(toX(x), toY(y)); first = false; }
        else       curve.lineTo(toX(x), toY(y));
    }
    g.setColour(AsunderLookAndFeel::accent.withAlpha(0.85f));
    g.strokePath(curve, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
