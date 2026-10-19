#include "ui/SeqGridComponent.h"

namespace lxx::ui {

void SeqGridComponent::setActiveStep(int step)
{
    if (activeStep_ != step) {
        activeStep_ = step;
        repaint();
    }
}

void SeqGridComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(4);
    constexpr int kSteps = 16;
    const int cellW = bounds.getWidth() / kSteps;
    const int cellH = bounds.getHeight();

    for (int i = 0; i < kSteps; ++i) {
        auto cell = juce::Rectangle<int>(bounds.getX() + i * cellW, bounds.getY(), cellW - 2, cellH);
        const bool active = i == activeStep_;
        g.setColour(active ? Palette::accentHot() : Palette::panel().brighter(0.12f));
        g.fillRoundedRectangle(cell.toFloat(), 3.0f);
        g.setColour(active ? Palette::cream() : Palette::line());
        g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.0f);
    }

    if (recording_) {
        g.setColour(Palette::accentHot().withAlpha(0.85f));
        g.setFont(11.0f);
        g.drawText("REC", bounds.removeFromTop(14), juce::Justification::centredRight, false);
    }
}

}  // namespace lxx::ui
