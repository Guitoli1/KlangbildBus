#include "PluginEditor.h"

using namespace vintage;

//==============================================================================
BusAudioProcessorEditor::BusAudioProcessorEditor (BusAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    addKnob ("threshold", "THRESHOLD (DB)", true, juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "40|\xe2\x88\x92" "30|\xe2\x88\x92" "20|\xe2\x88\x92" "10|0")));
    addKnob ("makeup",    "MAKEUP (DB)",    true, "0|5|10|15|20");
    addKnob ("attack",    "ATTACK (MS)",  false, "0,1|0,3|1|3|10|30");
    addKnob ("release",   "RELEASE (S)",  false, "0,1|0,3|0,6|1,2|AUTO");
    addKnob ("ratio",     "RATIO",        false, "2|4|10");
    addKnob ("scfilter",  "SC-FILTER (HZ)", false, "AUS|60|90|150");
    addKnob ("mix",       "MIX",          false);

    setupSwitch (onSwitch, "on", "AN", "AUS", lampRed);
    meterSwitch.setButtonText ("x");
    meterSwitch.getProperties().set ("onText", "AUSGANG");
    meterSwitch.getProperties().set ("offText", "GR");
    meterSwitch.getProperties().set ("lamp", (int) lampAmber.getARGB());
    addAndMakeVisible (meterSwitch);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, "meter", meterSwitch));

    presetBox.getProperties().set ("plate", true);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    addAndMakeVisible (vu);

    setSize (1000, 620);
    startTimerHz (60);
}

BusAudioProcessorEditor::~BusAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

BusAudioProcessorEditor::Knob& BusAudioProcessorEditor::addKnob (const juce::String& paramID, const juce::String& name, bool big, const juce::String& steps)
{
    auto k = std::make_unique<Knob>();
    k->name = name;
    k->big = big;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k->slider.setMouseDragSensitivity (big ? 320 : (steps.isNotEmpty() ? 140 : 220));
    if (steps.isNotEmpty())
        k->slider.getProperties().set ("stepLabels", steps);
    addAndMakeVisible (k->slider);
    k->attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k->slider);
    if (auto* rp = proc.apvts.getParameter (paramID))
        k->slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void BusAudioProcessorEditor::setupSwitch (juce::ToggleButton& b, const juce::String& paramID,
                                            const juce::String& onText, const juce::String& offText, juce::Colour lamp)
{
    b.setButtonText ("x");
    b.getProperties().set ("onText", onText);
    b.getProperties().set ("offText", offText);
    b.getProperties().set ("lamp", (int) lamp.getARGB());
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, paramID, b));
}

void BusAudioProcessorEditor::timerCallback()
{
    constexpr float dt = 1.0f / 60.0f;
    const bool showOutput = meterSwitch.getToggleState();
    if (showOutput)
    {
        vu.setLabel ("AUSGANG");
        vu.update (proc.vuOut.load(), proc.peakOut.exchange (0.0f), dt);
    }
    else
    {
        // Pegelreduktion: Zeiger steht ohne Kompression auf 0 VU und wandert nach links
        proc.peakOut.store (0.0f);
        vu.setLabel ("GR");
        const float gr = proc.grDb.load();
        const float avg = juce::Decibels::decibelsToGain (-gr - 18.0f) / 1.1107f;
        vu.update (avg, 0.0f, dt);
    }

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
void BusAudioProcessorEditor::renderTextures()
{
    tolexImage = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), false);
    {
        juce::Image::BitmapData data (tolexImage, juce::Image::BitmapData::writeOnly);
        juce::Random rnd (7);
        for (int y = 0; y < tolexImage.getHeight(); ++y)
            for (int x = 0; x < tolexImage.getWidth(); ++x)
            {
                const int weave = ((x + y) % 4 == 0 || (x - y + 4096) % 4 == 0) ? -5 : 0;
                const int n = rnd.nextInt (13) - 6 + weave;
                data.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, 34 + n),
                                                         (juce::uint8) juce::jlimit (0, 255, 28 + n),
                                                         (juce::uint8) juce::jlimit (0, 255, 24 + n)));
            }
    }
    if (faceplate.isEmpty())
        return;
    faceplateImage = juce::Image (juce::Image::ARGB, faceplate.getWidth(), faceplate.getHeight(), true);
    juce::Graphics g (faceplateImage);
    const auto r = faceplateImage.getBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (cream.brighter (0.06f), 0, 0, creamDark, 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 6.0f);
    juce::Random rnd (3);
    for (int y = 0; y < faceplate.getHeight(); ++y)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colour (0xff6b5534)).withAlpha (0.015f + rnd.nextFloat() * 0.035f));
        g.drawHorizontalLine (y, 0.0f, r.getWidth());
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                             juce::Colour (0x30694a1c), 0, 0, true));
    g.fillRoundedRectangle (r, 6.0f);
}

void BusAudioProcessorEditor::drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                                             juce::Font f, juce::Justification j)
{
    g.setFont (f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawFittedText (text, r.translated (0.0f, 1.0f).toNearestInt(), j, 2, 1.0f);
    g.setColour (ink);
    g.drawFittedText (text, r.toNearestInt(), j, 2, 1.0f);
}

void BusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (tolexImage, 0, 0);

    // Logo
    const auto scriptF = lnf.scriptFont (64.0f);
    const auto logoArea = juce::Rectangle<float> (40.0f, 8.0f, 400.0f, 84.0f);
    g.setFont (scriptF);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText ("Klangbild", logoArea.translated (2.0f, 3.0f), juce::Justification::centredLeft);
    g.setColour (logo);
    g.drawText ("Klangbild", logoArea, juce::Justification::centredLeft);
    const float sw = (float) juce::GlyphArrangement::getStringWidthInt (scriptF, "Klangbild");
    g.setColour (gold);
    g.setFont (lnf.labelFont (26.0f, 0.35f));
    g.drawText ("BUS", juce::Rectangle<float> (logoArea.getX() + sw + 14.0f, logoArea.getY() + 30.0f, 140.0f, 40.0f), juce::Justification::centredLeft);
    g.setColour (logo.withAlpha (0.55f));
    g.setFont (lnf.labelFont (12.0f, 0.3f));
    g.drawText (juce::CharPointer_UTF8 ("VCA-BUSKOMPRESSOR \xc2\xb7 SUMME \xc2\xb7 DRUMS \xc2\xb7 GRUPPEN"), juce::Rectangle<float> (44.0f, 90.0f, 420.0f, 16.0f), juce::Justification::centredLeft);

    const auto pa = presetArea.toFloat();
    g.setColour (logo.withAlpha (0.6f));
    g.setFont (lnf.labelFont (10.0f, 0.3f));
    g.drawText ("PROGRAMM", pa.withY (pa.getY() - 15.0f).withHeight (13.0f), juce::Justification::centredLeft);

    // Frontplatte
    const auto fp = faceplate.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (fp.expanded (4.0f).translated (0, 2.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (gold.brighter (0.3f), 0, fp.getY(), gold.darker (0.6f), 0, fp.getBottom(), false));
    g.fillRoundedRectangle (fp.expanded (3.0f), 8.0f);
    g.drawImageAt (faceplateImage, faceplate.getX(), faceplate.getY());
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (fp, 6.0f, 1.0f);
    for (auto [x, y, a] : { std::tuple { fp.getX() + 12.0f, fp.getY() + 12.0f, 0.4f }, std::tuple { fp.getRight() - 12.0f, fp.getY() + 12.0f, 1.3f },
                            std::tuple { fp.getX() + 12.0f, fp.getBottom() - 12.0f, 2.2f }, std::tuple { fp.getRight() - 12.0f, fp.getBottom() - 12.0f, 0.9f } })
        VintageLookAndFeel::drawScrew (g, { x, y }, 5.0f, a);

    // Rahmen um das große VU-Meter
    const auto vb = vuBezel.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (vb.translated (2.0f, 3.0f), 12.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2520), 0, vb.getY(), juce::Colour (0xff120f0c), 0, vb.getBottom(), false));
    g.fillRoundedRectangle (vb, 12.0f);

    // Trennlinie zwischen Haupt- und Nebenreglern
    for (int dy : { 0, 1 })
    {
        g.setColour (dy == 0 ? ink.withAlpha (0.4f) : juce::Colours::white.withAlpha (0.5f));
        g.drawHorizontalLine (dividerArea.getY() + dy, (float) dividerArea.getX(), (float) dividerArea.getRight());
    }

    for (auto& k : knobs)
    {
        const auto sb = k->slider.getBounds().toFloat();
        drawEngraved (g, k->name, { sb.getX() - 20.0f, sb.getBottom() - 4.0f, sb.getWidth() + 40.0f, k->big ? 24.0f : 18.0f },
                      lnf.labelFont (k->big ? 18.0f : 13.0f, k->big ? 0.2f : 0.14f), juce::Justification::centred);
    }

    const juce::String names[] = { "KOMPRESSOR", "ANZEIGE" };
    for (size_t i = 0; i < 2; ++i)
        drawEngraved (g, names[i], switchLabels[i].toFloat(), lnf.labelFont (12.0f, 0.14f), juce::Justification::centred);

    const auto tag = tagArea.toFloat();
    g.setColour (ink.withAlpha (0.85f));
    g.fillRoundedRectangle (tag, 3.0f);
    g.setColour (gold);
    g.setFont (lnf.labelFont (11.0f, 0.2f));
    g.drawText ("MODELL KB-4000", tag, juce::Justification::centred);
}

void BusAudioProcessorEditor::resized()
{
    const int W = getWidth();
    presetArea = { 44, 128, 260, 32 };
    presetBox.setBounds (presetArea);

    faceplate = { 24, 178, W - 48, 420 };
    const auto in = faceplate.reduced (30, 20);

    // obere Reihe: Threshold | VU | Makeup
    const int topH = 250;
    knobs[0]->slider.setBounds (in.getX(), in.getY() + 10, 250, 210);
    knobs[1]->slider.setBounds (in.getRight() - 250, in.getY() + 10, 250, 210);
    vuBezel = juce::Rectangle<int> (360, 232).withCentre ({ in.getCentreX(), in.getY() + 122 });
    vu.setBounds (vuBezel.reduced (10));
    dividerArea = { in.getX() + 10, in.getY() + topH + 6, in.getWidth() - 20, 2 };

    // untere Reihe: Attack, Release, Ratio, SC-Filter, Mix | Schalter
    const int rowY = in.getY() + topH + 14;
    for (int i = 2; i < 7; ++i)
        knobs[(size_t) i]->slider.setBounds (in.getX() + (i - 2) * 116, rowY, 116, 108);

    juce::ToggleButton* sws[] = { &onSwitch, &meterSwitch };
    const int swX0 = in.getX() + 5 * 116 + 16;
    for (int i = 0; i < 2; ++i)
    {
        const int x = swX0 + i * 140;
        switchLabels[(size_t) i] = { x, rowY + 8, 130, 16 };
        sws[i]->setBounds (x + 10, rowY + 30, 120, 40);
    }
    tagArea = { swX0 + 40, rowY + 84, 200, 24 };

    renderTextures();
}
