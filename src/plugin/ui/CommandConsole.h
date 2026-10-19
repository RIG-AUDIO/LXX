#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include <deque>

#include "ui/TerminalCommandRouter.h"

namespace lxx {
class LXXAudioProcessor;
}

namespace lxx::ui {

/**
 * DOS-like LXX command console: black background, phosphor green text,
 * same DIN product typeface as the graphical LXX UI.
 */
class CommandConsole : public juce::Component,
                       private juce::TextEditor::Listener {
public:
    CommandConsole()
    {
        din_ = juce::Typeface::createSystemTypefaceFor(
            BinaryData::din1451alt_ttf, static_cast<size_t>(BinaryData::din1451alt_ttfSize));

        historyView_.setMultiLine(true);
        historyView_.setReadOnly(true);
        historyView_.setScrollbarsShown(true);
        historyView_.setCaretVisible(false);
        historyView_.setFont(termFont(13.0f));
        historyView_.setColour(juce::TextEditor::backgroundColourId, kBg);
        historyView_.setColour(juce::TextEditor::textColourId, kGreenDim);
        historyView_.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        historyView_.setColour(juce::TextEditor::highlightColourId, kGreen.withAlpha(0.25f));
        historyView_.setText("LXX COMMAND CONSOLE — type help   Tab completes\n",
                             juce::dontSendNotification);
        addAndMakeVisible(historyView_);

        input_.setFont(termFont(14.0f));
        input_.setColour(juce::TextEditor::backgroundColourId, kBg);
        input_.setColour(juce::TextEditor::textColourId, kGreen);
        input_.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        input_.setColour(juce::CaretComponent::caretColourId, kGreen);
        input_.setTextToShowWhenEmpty("LXX>", kGreenDim);
        input_.addListener(this);
        input_.onTextChange = [this] { refreshCompletions(); };
        addAndMakeVisible(input_);
        setWantsKeyboardFocus(true);
    }

    ~CommandConsole() override { input_.removeListener(this); }

    void setProcessor(lxx::LXXAudioProcessor* processor) noexcept;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(kBg);
        g.setColour(kGreenDim);
        g.drawRect(getLocalBounds().toFloat().reduced(0.5f), 1.0f);

        auto header = getLocalBounds().removeFromTop(26).reduced(10, 4);
        g.setColour(kGreen);
        g.setFont(termFont(14.0f));
        g.drawText("LXX>", header, juce::Justification::centredLeft, false);
        g.setColour(kGreenDim);
        g.setFont(termFont(11.0f));
        g.drawText(statusLine_, header, juce::Justification::centredRight, false);

        if (completions_.size() > 0) {
            auto strip = completionBounds_;
            g.setColour(kBg);
            g.fillRect(strip);
            g.setColour(kGreenDim);
            g.drawRect(strip.toFloat(), 1.0f);
            g.setFont(termFont(12.0f));
            int x = strip.getX() + 8;
            for (int i = 0; i < completions_.size() && i < 8; ++i) {
                const bool sel = (i == completionIndex_);
                const auto w = completions_[i].length() * 8 + 12;
                if (sel) {
                    g.setColour(kGreen.withAlpha(0.2f));
                    g.fillRect(x - 2, strip.getY() + 2, w, strip.getHeight() - 4);
                }
                g.setColour(sel ? kGreen : kGreenDim);
                g.drawText(completions_[i], x, strip.getY(), w, strip.getHeight(),
                           juce::Justification::centredLeft, false);
                x += w;
            }
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8);
        r.removeFromTop(26);
        completionBounds_ = r.removeFromBottom(24);
        r.removeFromBottom(2);
        input_.setBounds(r.removeFromBottom(28));
        r.removeFromBottom(4);
        historyView_.setBounds(r);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (!input_.hasKeyboardFocus(true) && !hasKeyboardFocus(true))
            return false;

        if (completions_.size() > 0) {
            if (key == juce::KeyPress::upKey || (key == juce::KeyPress::tabKey && key.getModifiers().isShiftDown())) {
                completionIndex_ = (completionIndex_ + completions_.size() - 1) % completions_.size();
                repaint();
                return true;
            }
            if (key == juce::KeyPress::downKey || key == juce::KeyPress::tabKey) {
                if (key == juce::KeyPress::tabKey) {
                    applyCompletion(completions_[completionIndex_]);
                    return true;
                }
                completionIndex_ = (completionIndex_ + 1) % completions_.size();
                repaint();
                return true;
            }
            if (key == juce::KeyPress::escapeKey) {
                completions_.clear();
                completionIndex_ = 0;
                repaint();
                return true;
            }
        } else {
            if (key == juce::KeyPress::tabKey) {
                refreshCompletions();
                if (completions_.size() == 1)
                    applyCompletion(completions_[0]);
                return true;
            }
            if (key == juce::KeyPress::upKey) {
                if (history_.empty()) return true;
                historyIndex_ = juce::jlimit(0, (int) history_.size() - 1,
                    historyIndex_ < 0 ? (int) history_.size() - 1 : historyIndex_ - 1);
                input_.setText(history_[(size_t) historyIndex_], juce::dontSendNotification);
                return true;
            }
            if (key == juce::KeyPress::downKey) {
                if (historyIndex_ < 0) return true;
                ++historyIndex_;
                if (historyIndex_ >= (int) history_.size()) {
                    historyIndex_ = -1;
                    input_.clear();
                } else input_.setText(history_[(size_t) historyIndex_], juce::dontSendNotification);
                return true;
            }
            if (key == juce::KeyPress::escapeKey) {
                input_.clear();
                return true;
            }
        }
        return false;
    }

private:
    static constexpr juce::uint32 kBgArgb = 0xff000000;
    static constexpr juce::uint32 kGreenArgb = 0xff33ff66;
    static constexpr juce::uint32 kGreenDimArgb = 0xff1a9940;

    const juce::Colour kBg { kBgArgb };
    const juce::Colour kGreen { kGreenArgb };
    const juce::Colour kGreenDim { kGreenDimArgb };

    juce::Font termFont(float size) const
    {
        if (din_ != nullptr)
            return juce::Font(din_).withHeight(size);
        return juce::Font(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain);
    }

    void refreshCompletions()
    {
        if (router_ == nullptr) { completions_.clear(); return; }
        completions_ = router_->complete(input_.getText(), input_.getText().endsWithChar(' '));
        completionIndex_ = 0;
        repaint();
    }

    void applyCompletion(const juce::String& token)
    {
        auto text = input_.getText();
        const int lastSpace = text.lastIndexOfChar(' ');
        if (lastSpace >= 0)
            text = text.substring(0, lastSpace + 1) + token + " ";
        else
            text = token + " ";
        input_.setText(text, juce::dontSendNotification);
        input_.moveCaretToEnd();
        refreshCompletions();
    }

    void textEditorReturnKeyPressed(juce::TextEditor&) override
    {
        if (apvts_ == nullptr) return;
        const auto line = input_.getText().trim();
        input_.clear();
        completions_.clear();
        historyIndex_ = -1;
        if (line.isEmpty()) return;
        history_.push_back(line);
        if (history_.size() > 64) history_.pop_front();
        appendLine("LXX> " + line);
        juce::String feedback;
        if (router_ != nullptr)
            router_->tryExecute(line, *apvts_, feedback);
        if (feedback.isNotEmpty()) {
            for (auto& part : juce::StringArray::fromLines(feedback))
                appendLine(part);
            statusLine_ = feedback.upToFirstOccurrenceOf("\n", false, false);
        }
        repaint();
    }

    void textEditorEscapeKeyPressed(juce::TextEditor&) override
    {
        completions_.clear();
        input_.clear();
        repaint();
    }

    void appendLine(const juce::String& line)
    {
        historyView_.moveCaretToEnd();
        historyView_.insertTextAtCaret(line + "\n");
        historyView_.moveCaretToEnd();
    }

    juce::Typeface::Ptr din_;
    lxx::LXXAudioProcessor* processor_ = nullptr;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    TerminalCommandRouter* router_ = nullptr;
    juce::TextEditor historyView_;
    juce::TextEditor input_;
    std::deque<juce::String> history_;
    juce::StringArray completions_;
    int completionIndex_ = 0;
    int historyIndex_ = -1;
    juce::Rectangle<int> completionBounds_;
    juce::String statusLine_ { "READY" };
};

}  // namespace lxx::ui
