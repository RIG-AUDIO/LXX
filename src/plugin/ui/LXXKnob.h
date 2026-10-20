#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/LXXUiPalette.h"

namespace lxx::ui {

enum class KnobSize { standard, large };

class LXXKnob : public juce::Component {
 public:
    LXXKnob(const juce::String& label, KnobSize size = KnobSize::standard);

    void resized() override;
    void setAccentHot(bool hot);
    void setPlaceholder(bool placeholder);
    void setControlEnabled(bool enabled);
    void setExtendedStyle(bool extended);

    juce::Slider& slider() noexcept { return slider_; }
    const juce::Slider& slider() const noexcept { return slider_; }

 private:
    juce::Label label_;
    juce::Slider slider_;
    bool accentHot_ = false;
    bool extendedStyle_ = false;

    void refreshLabelColour();
};

}  // namespace lxx::ui
