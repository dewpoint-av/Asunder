#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "WaveshapeVisualizer.h"
#include "TapeVisualizer.h"
#include "HoloLogo.h"

#include <cmath>
#include <functional>

namespace
{
    juce::String pctText  (float v) { return juce::String(v * 100.0f, 0) + " %"; }
    juce::String gainText (float v) { return juce::String(v, 2) + "x"; }
    juce::String hzText   (float v) { return juce::String(v, 0) + " Hz"; }

    juce::String perBandId (int bandOneBased, const char* what)
    {
        return "b" + juce::String(bandOneBased) + what;
    }

    // Per-tape-colour accent colours (also tint the visualizer).
    juce::Colour colourForTape (int c)
    {
        switch (c)
        {
            case 1:  return juce::Colour (0xffffcf6e);   // Tube  — gold
            case 2:  return juce::Colour (0xff5ec8ff);   // FET   — cyan
            case 3:  return juce::Colour (0xffff6ea8);   // Class A — magenta
            default: return juce::Colour (0xffff9e5e);   // Tape  — amber
        }
    }

    // ---- a labelled rotary knob bound to a parameter ------------------------
    class Knob : public juce::Component,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater
    {
    public:
        Knob(juce::AudioProcessorValueTreeState& st, const juce::String& paramId,
             const juce::String& name, std::function<juce::String(float)> textFnIn)
            : stateRef(st), param(paramId), textFn(textFnIn)
        {
            slider.setSliderStyle(juce::Slider::Rotary);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setRotaryParameters({ juce::MathConstants<float>::pi * 1.25f,
                                         juce::MathConstants<float>::pi * 2.75f, true });
            addAndMakeVisible(slider);

            nameLabel.setText(name, juce::dontSendNotification);
            nameLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
            nameLabel.setColour(juce::Label::textColourId, AsunderLookAndFeel::muted);
            nameLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(nameLabel);

            valueLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            valueLabel.setColour(juce::Label::textColourId, AsunderLookAndFeel::text);
            valueLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(valueLabel);

            attach();
        }

        ~Knob() override { stateRef.removeParameterListener(param, this); }

        void retarget(const juce::String& newParamId)
        {
            if (param == newParamId)
                return;
            stateRef.removeParameterListener(param, this);
            param = newParamId;
            attach();
        }

        void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }
        void handleAsyncUpdate() override { refreshLabel(); }

        void resized() override
        {
            auto b = getLocalBounds();
            nameLabel.setBounds(b.removeFromTop(18));
            valueLabel.setBounds(b.removeFromBottom(18));
            slider.setBounds(b);
        }

    private:
        void attach()
        {
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(stateRef, param, slider);
            stateRef.addParameterListener(param, this);
            refreshLabel();
        }

        void refreshLabel()
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(stateRef.getParameter(param)))
                if (textFn)
                    valueLabel.setText(textFn(p->getNormalisableRange().convertFrom0to1(p->getValue())),
                                       juce::dontSendNotification);
        }

        juce::AudioProcessorValueTreeState& stateRef;
        juce::String param;
        std::function<juce::String(float)> textFn;
        juce::Slider slider;
        juce::Label nameLabel, valueLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // ---- a generic choice-parameter button (distortion type / tape colour / mode) ----
    class ChoiceButton : public juce::Component
    {
    public:
        ChoiceButton(juce::AudioProcessorValueTreeState& st, juce::String paramId,
                     juce::String label, int index, juce::Colour accentColour,
                     float textSize = 10.0f)
            : state(st), param(std::move(paramId)), name(std::move(label)),
              id(index), accent(accentColour), fontSize(textSize) {}

        std::function<void()> onClick;

        void setAccent(juce::Colour c) { accent = c; repaint(); }
        void setParam(juce::String newParamId) { param = std::move(newParamId); repaint(); }

        void paint(juce::Graphics& g) override
        {
            const bool sel = selected();
            auto r = getLocalBounds().toFloat().reduced(1.5f);

            if (sel)
            {
                g.setColour(accent.withAlpha(0.18f));
                g.fillRoundedRectangle(r, 7.0f);
                g.setColour(accent.withAlpha(0.8f));
                g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.5f);
            }
            else
            {
                g.setColour(AsunderLookAndFeel::panel);
                g.fillRoundedRectangle(r, 7.0f);
                g.setColour(AsunderLookAndFeel::border);
                g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.0f);
            }

            g.setFont(juce::FontOptions(fontSize, sel ? juce::Font::bold : juce::Font::plain));
            g.setColour(sel ? AsunderLookAndFeel::text : AsunderLookAndFeel::muted);
            g.drawText(name, r, juce::Justification::centred, false);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(param)))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(id)));
            if (onClick) onClick();
        }

        bool selected() const
        {
            if (auto* p = state.getRawParameterValue(param))
                return juce::roundToInt(p->load()) == id;
            return false;
        }

    private:
        juce::AudioProcessorValueTreeState& state;
        juce::String param, name;
        int id;
        juce::Colour accent;
        float fontSize;
    };
}

// ===========================================================================
class AsunderAudioProcessorEditor::Impl : private juce::AudioProcessorValueTreeState::Listener,
                                          private juce::AsyncUpdater
{
public:
    explicit Impl(AsunderAudioProcessor& p) : processor(p), state(p.getState())
    {
        build();
        applyBand(currentBand());
        updateMode(currentMode());
        updateAccent();
    }

    ~Impl() override
    {
        state.removeParameterListener("band", this);
        state.removeParameterListener("mode", this);
        state.removeParameterListener("tapeColor", this);
        for (int b = 1; b <= asunder::kMaxBands; ++b)
            state.removeParameterListener(perBandId(b, "type"), this);
    }

    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override
    {
        const int m = currentMode();
        if (m != shownMode)
            updateMode(m);

        updateAccent();

        const int b = currentBand();
        if (b != bandKnobsBand)
            applyBand(b);
        else
            for (auto& k : typeButtons) k->repaint();

        for (auto& c : colorButtons) c->repaint();
    }

    void attachTo(juce::Component& root)
    {
        root.addAndMakeVisible(*title);
        root.addAndMakeVisible(subtitle);
        root.addAndMakeVisible(brand);
        root.addAndMakeVisible(footer);
        root.addAndMakeVisible(*modeDist);
        root.addAndMakeVisible(*modeTape);
        root.addAndMakeVisible(qualityLabel);
        root.addAndMakeVisible(qualityCombo);

        // Distortion page.
        root.addAndMakeVisible(*visualizer);
        root.addAndMakeVisible(bandLabel);
        root.addAndMakeVisible(bandCombo);
        root.addAndMakeVisible(bandsLabel);
        root.addAndMakeVisible(bandsCombo);
        for (auto& k : bandKnobs)   root.addAndMakeVisible(*k);
        for (auto& k : globalKnobs) root.addAndMakeVisible(*k);
        for (auto& k : xoverKnobs)  root.addAndMakeVisible(*k);
        for (auto& b : typeButtons) root.addAndMakeVisible(*b);

        // Tape page.
        root.addAndMakeVisible(*tapeVis);
        for (auto& b : colorButtons) root.addAndMakeVisible(*b);
        for (auto& k : tapeKnobs)    root.addAndMakeVisible(*k);
    }

    void resized(juce::Rectangle<int> bounds)
    {
        auto b = bounds.reduced(16);
        footer.setBounds(b.removeFromBottom(18));

        // Header.
        auto header = b.removeFromTop(84);
        brand.setBounds(header.removeFromTop(18));
        title->setBounds(header.removeFromTop(44));
        subtitle.setBounds(header.removeFromTop(22));

        // Mode row: DISTORTION | TAPE on the left, Quality on the right.
        auto modeRow = b.removeFromTop(40);
        auto qArea = modeRow.removeFromRight(190);
        qualityLabel.setBounds(qArea.removeFromLeft(60).withSizeKeepingCentre(60, 18));
        qualityCombo.setBounds(qArea.reduced(2, 5));
        auto toggles = modeRow.removeFromLeft(260);
        modeDist->setBounds(toggles.removeFromLeft(128).reduced(3));
        modeTape->setBounds(toggles.removeFromLeft(128).reduced(3));

        b.removeFromTop(6);

        if (shownMode == 1) layoutTape(b);
        else                layoutDistortion(b);
    }

private:
    AsunderAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    std::unique_ptr<HoloTitle> title;
    juce::Label subtitle, brand, footer, bandLabel, bandsLabel, qualityLabel;

    // Mode + quality.
    std::unique_ptr<ChoiceButton> modeDist, modeTape;
    juce::ComboBox qualityCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;

    // Distortion page.
    std::unique_ptr<WaveshapeVisualizer> visualizer;
    juce::ComboBox bandCombo, bandsCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandAttachment, bandsAttachment;
    std::vector<std::unique_ptr<Knob>> bandKnobs;
    std::vector<std::unique_ptr<Knob>> globalKnobs;
    std::vector<std::unique_ptr<Knob>> xoverKnobs;
    std::vector<std::unique_ptr<ChoiceButton>> typeButtons;

    // Tape page.
    std::unique_ptr<TapeVisualizer> tapeVis;
    std::vector<std::unique_ptr<ChoiceButton>> colorButtons;
    std::vector<std::unique_ptr<Knob>> tapeKnobs;   // drive,bias,tone,bump,punch,wow,hiss,mix,output

    int bandKnobsBand = 1;
    int shownMode = -1;

    int currentBand() const
    {
        if (auto* p = state.getRawParameterValue("band"))
            return juce::jlimit(1, asunder::kMaxBands, juce::roundToInt(p->load()));
        return 1;
    }

    int currentMode() const
    {
        if (auto* p = state.getRawParameterValue("mode"))
            return juce::jlimit(0, 1, juce::roundToInt(p->load()));
        return 0;
    }

    int currentColor() const
    {
        if (auto* p = state.getRawParameterValue("tapeColor"))
            return juce::jlimit(0, asunder::kNumTapeColors - 1, juce::roundToInt(p->load()));
        return 0;
    }

    void applyBand(int band)
    {
        bandKnobsBand = band;
        bandKnobs[0]->retarget(perBandId(band, "drive"));
        bandKnobs[1]->retarget(perBandId(band, "tone"));
        bandKnobs[2]->retarget(perBandId(band, "bias"));
        for (auto& t : typeButtons)
            t->setParam(perBandId(band, "type"));
        visualizer->setBand(band);
    }

    void updateAccent()
    {
        const auto col = colourForTape(currentColor());
        if (tapeVis) tapeVis->setAccentColour(col);
        for (auto& b : colorButtons) b->repaint();
    }

    void setPageVisible(bool tape)
    {
        // Distortion page.
        visualizer->setVisible(! tape);
        bandLabel.setVisible(! tape);  bandCombo.setVisible(! tape);
        bandsLabel.setVisible(! tape); bandsCombo.setVisible(! tape);
        for (auto& k : bandKnobs)   k->setVisible(! tape);
        for (auto& k : globalKnobs) k->setVisible(! tape);
        for (auto& k : xoverKnobs)  k->setVisible(! tape);
        for (auto& b : typeButtons) b->setVisible(! tape);

        // Tape page.
        tapeVis->setVisible(tape);
        for (auto& b : colorButtons) b->setVisible(tape);
        for (auto& k : tapeKnobs)    k->setVisible(tape);
    }

    void updateMode(int mode)
    {
        shownMode = mode;
        subtitle.setText(mode == 1 ? "ANALOG TAPE SATURATION" : "MULTIBAND DISTORTION",
                         juce::dontSendNotification);
        setPageVisible(mode == 1);
        if (auto* parent = title->getParentComponent())
            parent->resized();
        modeDist->repaint();
        modeTape->repaint();
    }

    // ---- distortion layout (original) -------------------------------------
    void layoutDistortion(juce::Rectangle<int> b)
    {
        auto mb = b.removeFromBottom(112);
        auto bandsArea = mb.removeFromLeft(128);
        bandsLabel.setBounds(bandsArea.removeFromTop(16));
        bandsCombo.setBounds(bandsArea.reduced(6, 2).removeFromTop(28));

        const int ncx = static_cast<int>(xoverKnobs.size());
        const int cw = ncx > 0 ? mb.getWidth() / ncx : 0;
        for (int i = 0; i < ncx; ++i)
            xoverKnobs[static_cast<size_t>(i)]->setBounds(mb.removeFromLeft(cw).reduced(4, 2));

        auto bandRow = b.removeFromTop(66);
        auto bandArea = bandRow.removeFromLeft(120);
        bandLabel.setBounds(bandArea.removeFromTop(16));
        bandCombo.setBounds(bandArea.reduced(6, 2).removeFromTop(28));

        const int nt = static_cast<int>(typeButtons.size());
        const int bw = nt > 0 ? bandRow.getWidth() / nt : 0;
        for (int i = 0; i < nt; ++i)
            typeButtons[static_cast<size_t>(i)]->setBounds(bandRow.removeFromLeft(bw).reduced(3, 3));

        auto knobsArea = b.removeFromRight(static_cast<int>(b.getWidth() * 0.62f));
        const int nk = static_cast<int>(bandKnobs.size() + globalKnobs.size());
        const int kw = nk > 0 ? knobsArea.getWidth() / nk : 0;
        for (auto& k : bandKnobs)   k->setBounds(knobsArea.removeFromLeft(kw).reduced(4, 2));
        for (auto& k : globalKnobs) k->setBounds(knobsArea.removeFromLeft(kw).reduced(4, 2));

        visualizer->setBounds(b.reduced(6));
    }

    // ---- tape layout ------------------------------------------------------
    void layoutTape(juce::Rectangle<int> b)
    {
        // Colour selector row.
        auto colorRow = b.removeFromTop(40);
        const int nc = static_cast<int>(colorButtons.size());
        const int cwi = nc > 0 ? colorRow.getWidth() / nc : 0;
        for (int i = 0; i < nc; ++i)
            colorButtons[static_cast<size_t>(i)]->setBounds(colorRow.removeFromLeft(cwi).reduced(4, 3));

        b.removeFromTop(6);

        // Visualizer left, knob grid right.
        auto knobsArea = b.removeFromRight(static_cast<int>(b.getWidth() * 0.60f));
        tapeVis->setBounds(b.reduced(6));

        const int n = static_cast<int>(tapeKnobs.size());   // 9
        const int cols = 3;
        const int rows = (n + cols - 1) / cols;
        const int cwid = knobsArea.getWidth() / cols;
        const int chei = knobsArea.getHeight() / rows;
        for (int i = 0; i < n; ++i)
        {
            const int r = i / cols;
            const int c = i % cols;
            juce::Rectangle<int> cell (knobsArea.getX() + c * cwid,
                                       knobsArea.getY() + r * chei, cwid, chei);
            tapeKnobs[static_cast<size_t>(i)]->setBounds(cell.reduced(4, 2));
        }
    }

    void build()
    {
        title = std::make_unique<HoloTitle>("ASUNDER", 40.0f, true);

        subtitle.setText("MULTIBAND DISTORTION", juce::dontSendNotification);
        subtitle.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        subtitle.setColour(juce::Label::textColourId, AsunderLookAndFeel::muted);
        subtitle.setJustificationType(juce::Justification::centred);

        brand.setText("DEWPOINT AV", juce::dontSendNotification);
        brand.setFont(monumentFont(13.0f, false));
        brand.setColour(juce::Label::textColourId, AsunderLookAndFeel::accent);
        brand.setJustificationType(juce::Justification::centred);

        footer.setText(juce::String("DEWPOINT AV  -  v") + juce::String(JucePlugin_VersionString), juce::dontSendNotification);
        footer.setFont(monumentFont(11.0f, false));
        footer.setColour(juce::Label::textColourId, AsunderLookAndFeel::muted);
        footer.setJustificationType(juce::Justification::centred);

        // Mode toggle.
        modeDist = std::make_unique<ChoiceButton>(state, "mode", "DISTORTION", 0, AsunderLookAndFeel::accent, 12.0f);
        modeTape = std::make_unique<ChoiceButton>(state, "mode", "TAPE",       1, AsunderLookAndFeel::accent, 12.0f);

        // Quality selector.
        qualityLabel.setText("QUALITY", juce::dontSendNotification);
        qualityLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        qualityLabel.setColour(juce::Label::textColourId, AsunderLookAndFeel::muted);
        qualityLabel.setJustificationType(juce::Justification::centredRight);
        qualityCombo.addItemList({ "Draft 1x", "Standard 2x", "High 4x", "Ultra 8x" }, 1);
        qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "quality", qualityCombo);

        // ---- Distortion page --------------------------------------------
        visualizer = std::make_unique<WaveshapeVisualizer>(state);
        visualizer->setBand(1);

        bandKnobs.push_back(std::make_unique<Knob>(state, perBandId(1, "drive"), "DRIVE", pctText));
        bandKnobs.push_back(std::make_unique<Knob>(state, perBandId(1, "tone"),  "TONE",  pctText));
        bandKnobs.push_back(std::make_unique<Knob>(state, perBandId(1, "bias"),  "BIAS",  pctText));
        globalKnobs.push_back(std::make_unique<Knob>(state, "mix",    "MIX",    pctText));
        globalKnobs.push_back(std::make_unique<Knob>(state, "output", "OUTPUT", gainText));

        bandLabel.setText("EDIT BAND", juce::dontSendNotification);
        bandLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        bandLabel.setColour(juce::Label::textColourId, AsunderLookAndFeel::muted);
        bandLabel.setJustificationType(juce::Justification::centred);

        for (int i = 1; i <= asunder::kMaxBands; ++i)
            bandCombo.addItem("BAND " + juce::String(i), i);
        bandCombo.setSelectedId(1);
        bandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "band", bandCombo);

        bandsLabel.setText("BANDS", juce::dontSendNotification);
        bandsLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        bandsLabel.setColour(juce::Label::textColourId, AsunderLookAndFeel::muted);
        bandsLabel.setJustificationType(juce::Justification::centred);

        for (int i = 1; i <= asunder::kMaxBands; ++i)
            bandsCombo.addItem(juce::String(i), i);
        bandsCombo.setSelectedId(1);
        bandsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "bands", bandsCombo);

        xoverKnobs.push_back(std::make_unique<Knob>(state, "xover1", "X-OVER 1", hzText));
        xoverKnobs.push_back(std::make_unique<Knob>(state, "xover2", "X-OVER 2", hzText));
        xoverKnobs.push_back(std::make_unique<Knob>(state, "xover3", "X-OVER 3", hzText));
        xoverKnobs.push_back(std::make_unique<Knob>(state, "xover4", "X-OVER 4", hzText));

        {
            const juce::StringArray typeNames { "HARD", "SOFT", "TUBE", "FOLD", "CRUSH", "RATE" };
            for (int i = 0; i < asunder::kNumDistortionTypes; ++i)
            {
                auto tb = std::make_unique<ChoiceButton>(state, perBandId(currentBand(), "type"),
                                                         typeNames[i], i, AsunderLookAndFeel::accent);
                typeButtons.push_back(std::move(tb));
            }
        }

        // ---- Tape page ---------------------------------------------------
        tapeVis = std::make_unique<TapeVisualizer>(processor);

        {
            const juce::StringArray colorNames { "TAPE", "TUBE", "FET", "CLASS A" };
            for (int i = 0; i < asunder::kNumTapeColors; ++i)
                colorButtons.push_back(std::make_unique<ChoiceButton>(
                    state, "tapeColor", colorNames[i], i, colourForTape(i), 11.0f));
        }

        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeDrive",  "DRIVE",  pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeBias",   "BIAS",   pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeTone",   "TONE",   pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeBump",   "BUMP",   pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapePunch",  "PUNCH",  pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeWow",    "WOW",    pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeNoise",  "HISS",   pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeMix",    "MIX",    pctText));
        tapeKnobs.push_back(std::make_unique<Knob>(state, "tapeOutput", "OUTPUT", gainText));

        // Live updates.
        state.addParameterListener("band", this);
        state.addParameterListener("mode", this);
        state.addParameterListener("tapeColor", this);
        for (int b = 1; b <= asunder::kMaxBands; ++b)
            state.addParameterListener(perBandId(b, "type"), this);
    }
};

// ===========================================================================
AsunderAudioProcessorEditor::AsunderAudioProcessorEditor(AsunderAudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    setLookAndFeel(&lookAndFeel);

    impl = std::make_unique<Impl>(p);
    impl->attachTo(*this);

    setResizable(true, true);
    setResizeLimits(860, 560, 1400, 1000);
    setSize(940, 660);
}

AsunderAudioProcessorEditor::~AsunderAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void AsunderAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(AsunderLookAndFeel::background);
}

void AsunderAudioProcessorEditor::resized()
{
    impl->resized(getLocalBounds());
}
