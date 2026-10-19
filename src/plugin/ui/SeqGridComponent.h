#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/LXXUiPalette.h"

namespace lxx::ui {

class SeqGridComponent : public juce::Component {
public:
    void setActiveStep(int step);
    void setRecording(bool recording) { recording_ = recording; repaint(); }

private:
    void paint(juce::Graphics& g) override;

    int activeStep_ = -1;
    bool recording_ = false;
};

}  // namespace lxx::ui
