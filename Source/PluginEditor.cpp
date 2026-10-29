#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "WaveshapeVisualizer.h"
#include "HoloLogo.h"

#include <cmath>
#include <functional>

namespace
{
    juce::String pctText  (float v) { return juce::String(v * 100.0f, 0) + " %"; }
    juce::String gainText (float v) { return juce::String(v, 2) + "x"; }
    juce::String hzText   (float v) { return juce::String(v, 0) + " Hz"; }

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

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(st, paramId, slider);
            stateRef.addParameterListener(param, this);
            refreshLabel();
        }

        ~Knob() override { stateRef.removeParameterListener(param, this); }

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

    // ---- a distortion-type selector button ---------------------------------
    class TypeButton : public juce::Component
    {
    public:
        TypeButton(const juce::String& label, int typeId, juce::AudioProcessorValueTreeState& st)
            : name(label), id(typeId), state(st) {}

        void paint(juce::Graphics& g) override
        {
            const bool sel = selected();
            auto r = getLocalBounds().toFloat().reduced(1.5f);

            if (sel)
            {
                g.setColour(AsunderLookAndFeel::accent.withAlpha(0.16f));
                g.fillRoundedRectangle(r, 7.0f);
                g.setColour(AsunderLookAndFeel::accent.withAlpha(0.7f));
                g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.5f);
            }
            else
            {
                g.setColour(AsunderLookAndFeel::panel);
                g.fillRoundedRectangle(r, 7.0f);
                g.setColour(AsunderLookAndFeel::border);
                g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.0f);
            }

            g.setFont(juce::FontOptions(10.0f, sel ? juce::Font::bold : juce::Font::plain));
            g.setColour(sel ? AsunderLookAndFeel::text : AsunderLookAndFeel::muted);
            g.drawText(name, r, juce::Justification::centred, false);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            if (auto* p = state.getParameter("type"))
                p->setValueNotifyingHost(static_cast<float>(id) / static_cast<float>(asunder::kNumDistortionTypes - 1));
        }

        bool selected() const
        {
            if (auto* p = state.getRawParameterValue("type"))
                return juce::roundToInt(p->load()) == id;
            return false;
        }

    private:
        juce::String name;
        int id;
        juce::AudioProcessorValueTreeState& state;
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
    }

    ~Impl() override
    {
        state.removeParameterListener("type", this);
    }

    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override
    {
        for (auto& b : typeButtons)
            b->repaint();
    }

    void attachTo(juce::Component& root)
    {
        root.addAndMakeVisible(*title);
        root.addAndMakeVisible(subtitle);
        root.addAndMakeVisible(brand);
        root.addAndMakeVisible(footer);
        root.addAndMakeVisible(*visualizer);
        root.addAndMakeVisible(bandsLabel);
        root.addAndMakeVisible(bandsCombo);
        for (auto& k : knobs)        root.addAndMakeVisible(*k);
        for (auto& k : xoverKnobs)   root.addAndMakeVisible(*k);
        for (auto& b : typeButtons)  root.addAndMakeVisible(*b);
    }

    void resized(juce::Rectangle<int> bounds)
    {
        auto b = bounds.reduced(16);

        footer.setBounds(b.removeFromBottom(18));

        // Multiband row: bands selector + crossover knobs.
        auto mb = b.removeFromBottom(112);
        auto bandsArea = mb.removeFromLeft(128);
        bandsLabel.setBounds(bandsArea.removeFromTop(16));
        bandsCombo.setBounds(bandsArea.reduced(6, 2).removeFromTop(28));

        const int ncx = static_cast<int>(xoverKnobs.size());
        const int cw = ncx > 0 ? mb.getWidth() / ncx : 0;
        for (int i = 0; i < ncx; ++i)
            xoverKnobs[static_cast<size_t>(i)]->setBounds(mb.removeFromLeft(cw).reduced(4, 2));

        // Header.
        auto header = b.removeFromTop(96);
        brand.setBounds(header.removeFromTop(18));
        title->setBounds(header.removeFromTop(44));
        subtitle.setBounds(header.removeFromTop(22));

        // Type buttons row.
        auto typeRow = b.removeFromTop(42);
        const int nt = static_cast<int>(typeButtons.size());
        const int bw = nt > 0 ? typeRow.getWidth() / nt : 0;
        for (int i = 0; i < nt; ++i)
            typeButtons[static_cast<size_t>(i)]->setBounds(typeRow.removeFromLeft(bw).reduced(3, 3));

        // Knobs (3 x 2 grid) on the right; waveshape visualizer on the left.
        auto knobsArea = b.removeFromRight(static_cast<int>(b.getWidth() * 0.55f));
        const int cols = 3, rows = 2, gap = 10;
        const int cw2 = (knobsArea.getWidth()  - gap * (cols - 1)) / cols;
        const int ch2 = (knobsArea.getHeight() - gap * (rows - 1)) / rows;
        int idx = 0;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols && idx < static_cast<int>(knobs.size()); ++c, ++idx)
                knobs[static_cast<size_t>(idx)]->setBounds(knobsArea.getX() + c * (cw2 + gap),
                                                           knobsArea.getY() + r * (ch2 + gap),
                                                           cw2, ch2);

        visualizer->setBounds(b.reduced(6));
    }

private:
    AsunderAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    std::unique_ptr<HoloTitle> title;
    juce::Label subtitle, brand, footer, bandsLabel;
    std::unique_ptr<WaveshapeVisualizer> visualizer;
    juce::ComboBox bandsCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandsAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Knob>> xoverKnobs;
    std::vector<std::unique_ptr<TypeButton>> typeButtons;

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

        visualizer = std::make_unique<WaveshapeVisualizer>(state);

        knobs.push_back(std::make_unique<Knob>(state, "drive",  "DRIVE",  pctText));
        knobs.push_back(std::make_unique<Knob>(state, "tone",   "TONE",   pctText));
        knobs.push_back(std::make_unique<Knob>(state, "bias",   "BIAS",   pctText));
        knobs.push_back(std::make_unique<Knob>(state, "mix",    "MIX",    pctText));
        knobs.push_back(std::make_unique<Knob>(state, "output", "OUTPUT", gainText));

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

        const juce::StringArray typeNames { "HARD", "SOFT", "TUBE", "FOLD", "CRUSH", "RATE" };
        for (int i = 0; i < asunder::kNumDistortionTypes; ++i)
            typeButtons.push_back(std::make_unique<TypeButton>(typeNames[i], i, state));

        state.addParameterListener("type", this);
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
