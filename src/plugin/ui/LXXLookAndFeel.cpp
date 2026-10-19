#include "ui/LXXLookAndFeel.h"

#include <BinaryData.h>

namespace lxx::ui {

LXXLookAndFeel::LXXLookAndFeel()
{
    dinTypeface_ = juce::Typeface::createSystemTypefaceFor(
        BinaryData::din1451alt_ttf, static_cast<size_t>(BinaryData::din1451alt_ttfSize));
    setColour(juce::ResizableWindow::backgroundColourId, Palette::background());
    setColour(juce::Label::textColourId, Palette::cream());
    setColour(juce::Slider::textBoxTextColourId, Palette::cream());
    setColour(juce::Slider::textBoxBackgroundColourId, Palette::panelDeep());
    setColour(juce::Slider::textBoxOutlineColourId, Palette::line());
    setColour(juce::TextButton::buttonColourId, Palette::panelRaised());
    setColour(juce::TextButton::textColourOffId, Palette::mutedText());
    setColour(juce::TextButton::textColourOnId, Palette::cream());
    setColour(juce::ComboBox::backgroundColourId, Palette::panelDeep());
    setColour(juce::ComboBox::outlineColourId, Palette::edge());
    setColour(juce::ComboBox::textColourId, Palette::cream());
    setColour(juce::ComboBox::arrowColourId, Palette::accent());
    setColour(juce::ToggleButton::textColourId, Palette::cream());
    setColour(juce::ToggleButton::tickColourId, Palette::accent());
    setColour(juce::ToggleButton::tickDisabledColourId, Palette::disabled());
    setColour(juce::PopupMenu::backgroundColourId, Palette::panelRaised());
    setColour(juce::PopupMenu::textColourId, Palette::cream());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::accent().withAlpha(0.35f));
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::cream());
}

juce::Font LXXLookAndFeel::regularFont(float height) const
{
    if (dinTypeface_ != nullptr)
        return juce::Font(dinTypeface_).withHeight(height);
    return juce::Font(juce::Font::getDefaultSansSerifFontName(), height, juce::Font::plain);
}

void LXXLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                         int x,
                                         int y,
                                         int width,
                                         int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height))
                      .reduced(2.0f);
    const bool large = slider.getProperties().getWithDefault("lxxKnobSize", "standard") == "large";
    const float requestedDiameter = static_cast<float>(large ? Layout::knobLarge : Layout::knobStandard);
    const float diameter = juce::jmin(requestedDiameter,
                                      juce::jmin(bounds.getWidth(), bounds.getHeight()));
    bounds = bounds.withSizeKeepingCentre(diameter, diameter);

    const float radius = diameter * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                        0.0f, rotaryStartAngle, rotaryEndAngle, true);
    const float stroke = large ? 3.2f : 2.6f;
    g.setColour(Palette::knobRim().withMultipliedAlpha(0.42f));
    g.strokePath(track, juce::PathStrokeType(stroke));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                           0.0f, rotaryStartAngle, angle, true);
    const bool extendedKnob = slider.getProperties().getWithDefault("lxxExtendedKnob", "no") == "yes";
    const auto activeColour = ! slider.isEnabled() ? Palette::disabled()
        : extendedKnob ? Palette::focus()
        : Palette::knobActive();
    g.setColour(activeColour);
    g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    const auto body = bounds.reduced(diameter * 0.18f);
    g.setColour(Palette::knobBody());
    g.fillEllipse(body);
    g.setColour(Palette::knobInner());
    g.fillEllipse(body.reduced(diameter * 0.08f));
    g.setColour(Palette::edge().withMultipliedAlpha(0.9f));
    g.drawEllipse(body, 1.1f);

    juce::Path tick;
    const float pointerWidth = large ? 2.8f : 2.3f;
    tick.addRoundedRectangle(-pointerWidth * 0.5f, -radius * 0.70f,
                             pointerWidth, radius * 0.36f, pointerWidth * 0.5f);
    g.setColour(Palette::knobIndicator());
    g.fillPath(tick, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void LXXLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(backgroundColour);
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = button.getToggleState() ? Palette::accent().withAlpha(0.28f)
                                        : Palette::panelRaised();
    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.08f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.05f);

    g.setColour(fill);
    g.fillRect(bounds);
    g.setColour(button.getToggleState() ? Palette::accent() : Palette::edge());
    g.drawRect(bounds, 1.0f);
}

void LXXLookAndFeel::drawComboBox(juce::Graphics& g,
                                     int width,
                                     int height,
                                     bool isButtonDown,
                                     int buttonX,
                                     int buttonY,
                                     int buttonW,
                                     int buttonH,
                                     juce::ComboBox& box)
{
    juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f,
                                         static_cast<float>(width) - 1.0f,
                                         static_cast<float>(height) - 1.0f);
    auto fill = Palette::panelDeep();
    if (isButtonDown)
        fill = fill.brighter(0.10f);
    else if (box.isMouseOver())
        fill = Palette::accent().withAlpha(0.12f).overlaidWith(fill);
    g.setColour(fill);
    g.fillRect(bounds);
    g.setColour(box.hasKeyboardFocus(true) ? Palette::focus() : Palette::edge());
    g.drawRect(bounds, box.hasKeyboardFocus(true) ? 2.0f : 1.0f);
}

void LXXLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(box.getLocalBounds().reduced(9, 1).withTrimmedRight(18));
    label.setJustificationType(juce::Justification::centredLeft);
}

juce::Font LXXLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return regularFont(12.0f);
}

juce::Font LXXLookAndFeel::getLabelFont(juce::Label&)
{
    return regularFont(12.0f);
}

juce::Font LXXLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return regularFont(static_cast<float>(buttonHeight) * 0.52f);
}

}  // namespace lxx::ui
