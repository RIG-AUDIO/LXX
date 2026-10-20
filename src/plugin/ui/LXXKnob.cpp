#include "ui/LXXKnob.h"

namespace lxx::ui {

LXXKnob::LXXKnob(const juce::String& label, KnobSize size)
{
    label_.setText(label, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label_);

    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
    slider_.setColour(juce::Slider::textBoxTextColourId, Palette::accent());
    slider_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider_.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider_.getProperties().set("lxxKnobSize",
                                size == KnobSize::large ? "large" : "standard");
    addAndMakeVisible(slider_);
}

void LXXKnob::resized()
{
    auto area = getLocalBounds();
    label_.setBounds(area.removeFromTop(Layout::knobLabelHeight));
    slider_.setBounds(area);
}

void LXXKnob::refreshLabelColour()
{
    if (extendedStyle_)
        label_.setColour(juce::Label::textColourId, Palette::focus());
    else if (accentHot_)
        label_.setColour(juce::Label::textColourId, Palette::accentHot());
    else
        label_.setColour(juce::Label::textColourId, Palette::accent());
}

void LXXKnob::setAccentHot(bool hot)
{
    accentHot_ = hot;
    refreshLabelColour();
}

void LXXKnob::setPlaceholder(bool placeholder)
{
    setAlpha(placeholder ? 0.45f : 1.0f);
    slider_.setEnabled(!placeholder);
}

void LXXKnob::setControlEnabled(bool enabled)
{
    setAlpha(enabled ? 1.0f : 0.4f);
    slider_.setEnabled(enabled);
}

void LXXKnob::setExtendedStyle(bool extended)
{
    extendedStyle_ = extended;
    slider_.getProperties().set("lxxExtendedKnob", extended ? "yes" : "no");
    refreshLabelColour();
}

}  // namespace lxx::ui
