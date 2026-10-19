#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/LXXUiPalette.h"

namespace lxx::ui {

/**
 * Terminal skin scaffold.
 *
 * Layout architecture for the next CLI phase:
 *   header → module panels → command entry → status
 *
 * No command parser, no fake parameter values, no audio-rate timers.
 */
class TerminalView : public juce::Component {
public:
    TerminalView()
    {
        setInterceptsMouseClicks(true, true);
        command_.setText("> _", juce::dontSendNotification);
        command_.setColour(juce::Label::textColourId, juce::Colour(0xffc8e6d4));
        command_.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        command_.setJustificationType(juce::Justification::centredLeft);
        command_.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
        addAndMakeVisible(command_);
    }

    void setStatusLine(const juce::String& text)
    {
        status_ = text;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff07080a));
        auto bounds = getLocalBounds().reduced(14);

        g.setColour(juce::Colour(0xff1c2a32));
        g.drawRect(bounds.toFloat(), 1.0f);
        bounds.reduce(10, 8);

        auto header = bounds.removeFromTop(28);
        g.setColour(juce::Colour(0xff7dd3c0));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain));
        g.drawText("LXX", header, juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xff6f8794));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
        g.drawText("TERMINAL", header, juce::Justification::centredRight, false);

        auto ready = bounds.removeFromTop(20);
        g.setColour(juce::Colour(0xff8aa0b0));
        g.drawText("SYSTEM READY", ready, juce::Justification::centredLeft, false);

        bounds.removeFromTop(8);
        auto status = bounds.removeFromBottom(22);
        auto commandArea = bounds.removeFromBottom(32);
        bounds.removeFromBottom(8);

        auto top = bounds.removeFromTop(bounds.getHeight() / 2).reduced(0, 2);
        auto bottom = bounds.reduced(0, 2);
        auto osc = top.removeFromLeft(top.getWidth() / 2).reduced(2);
        auto filter = top.reduced(2);
        auto env = bottom.removeFromLeft(bottom.getWidth() / 2).reduced(2);
        auto seq = bottom.reduced(2);

        drawModule(g, osc, "KICK", "Bass drum tone / density via APVTS (parser deferred)");
        drawModule(g, filter, "SNARE", "Snare tone / density via APVTS (parser deferred)");
        drawModule(g, env, "HAT", "Hi-hat tone / density via APVTS (parser deferred)");
        drawModule(g, seq, "SEQ / SYNC", "Drum sequencer + host sync remain the DSP source of truth");

        g.setColour(juce::Colour(0xff1c2a32));
        g.drawRect(commandArea.toFloat(), 1.0f);

        g.setColour(juce::Colour(0xff6f8794));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        g.drawText(status_.isEmpty() ? "READY" : status_,
                   status, juce::Justification::centredLeft, false);
        g.drawText("CLI deferred", status, juce::Justification::centredRight, false);
    }

    void resized() override
    {
        auto commandArea = getLocalBounds().reduced(24).removeFromBottom(54).removeFromTop(28);
        command_.setBounds(commandArea.reduced(8, 2));
    }

private:
    static void drawModule(juce::Graphics& g, juce::Rectangle<int> r,
                           const juce::String& title, const juce::String& note)
    {
        g.setColour(juce::Colour(0xff10161c));
        g.fillRect(r);
        g.setColour(juce::Colour(0xff24323c));
        g.drawRect(r.toFloat(), 1.0f);
        auto header = r.removeFromTop(22).reduced(8, 0);
        g.setColour(juce::Colour(0xff7dd3c0));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
        g.drawText(title, header, juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xff6f8794));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        g.drawFittedText(note, r.reduced(8, 6), juce::Justification::topLeft, 3);
    }

    juce::Label command_;
    juce::String status_;
};

class SkinPlaceholderView : public juce::Component {
public:
    void setTitle(const juce::String& title) { title_ = title; repaint(); }
    void setBody(const juce::String& body) { body_ = body; repaint(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::background());
        auto bounds = getLocalBounds().reduced(40);
        g.setColour(Palette::panel());
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
        g.setColour(Palette::line());
        g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);
        g.setColour(Palette::cream());
        g.setFont(20.0f);
        g.drawText(title_, bounds.removeFromTop(48), juce::Justification::centred, false);
        g.setColour(Palette::mutedText());
        g.setFont(14.0f);
        g.drawFittedText(body_, bounds.reduced(16), juce::Justification::centred, 6);
    }

private:
    juce::String title_ { "Placeholder" };
    juce::String body_;
};

}  // namespace lxx::ui
