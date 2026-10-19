#pragma once

#include <array>

#include "ui/LXXUiPalette.h"

namespace lxx::ui {

/** Physical sites extracted from anu.ai (656.67 x 419 pt mediabox). */
struct PanelSite {
    const char* id;
    float cx;
    float cy;
    float diameterPt;
    bool accentHot = false;
};

struct PanelLayout {
    static constexpr float kW = Layout::sourceWidth;
    static constexpr float kH = Layout::sourceHeight;

    static juce::Point<int> toEditor(float cx, float cy)
    {
        return { juce::roundToInt(cx * Layout::scaleX),
                 juce::roundToInt(cy * Layout::scaleY) };
    }

    static int knobDiameter(float diameterPt)
    {
        return juce::roundToInt(diameterPt * Layout::scaleX * 1.85f);
    }

    static juce::Rectangle<int> knobBounds(float cx, float cy, float diameterPt)
    {
        const int d = knobDiameter(diameterPt);
        const auto centre = toEditor(cx, cy);
        return { centre.x - d / 2, centre.y - d / 2 - Layout::knobLabelHeight, d, d + Layout::knobLabelHeight };
    }

    // Centers derived from PyMuPDF path rects (x0 + d/2, y0 + d/2).
    static constexpr std::array<PanelSite, 14> upperKnobs {{
        { "lin_fm", 78.23f, 96.77f, 22.68f },
        { "fm", 150.23f, 96.77f, 22.68f },
        { "vcf_fm", 78.23f, 168.77f, 22.68f },
        { "pw_mod", 150.23f, 168.77f, 22.68f },
        { "wave", 222.24f, 96.78f, 18.43f },
        { "sub_level", 222.24f, 168.78f, 18.43f },
        { "cutoff", 294.23f, 96.77f, 22.68f },
        { "resonance", 366.23f, 96.77f, 22.68f, true },
        { "vcf_env", 294.23f, 168.77f, 22.68f },
        { "vcf_lfo", 366.23f, 168.77f, 22.68f },
        { "mod", 495.84f, 96.78f, 18.43f },
        { "volume", 553.43f, 96.77f, 22.68f },
        { "vco_mod_bal", 438.23f, 96.77f, 22.68f },
        { "pw_mod_bal", 438.23f, 168.77f, 22.68f },
    }};

    static constexpr std::array<PanelSite, 10> softPotRowA {{
        { "pot_a0", 78.23f, 240.77f, 22.68f },
        { "pot_a1", 150.23f, 240.77f, 22.68f },
        { "pot_a2", 222.23f, 240.77f, 22.68f },
        { "pot_a3", 294.23f, 240.77f, 22.68f },
        { "pot_a4", 366.23f, 240.77f, 22.68f },
        { "pot_a5", 78.23f, 312.77f, 22.68f },
        { "pot_a6", 150.23f, 312.77f, 22.68f },
        { "pot_a7", 222.23f, 312.77f, 22.68f },
        { "pot_a8", 294.23f, 312.77f, 22.68f },
        { "pot_a9", 366.23f, 312.77f, 22.68f },
    }};

    static constexpr std::array<PanelSite, 4> transportKnobs {{
        { "tempo", 441.83f, 244.37f, 26.08f },
        { "run_stop", 492.23f, 244.37f, 26.08f },
        { "rec", 441.83f, 280.37f, 26.08f },
        { "hold", 492.23f, 280.37f, 26.08f },
    }};

    static juce::Rectangle<float> vcoSection() { return { 48.0f, 58.0f, 190.0f, 140.0f }; }
    static juce::Rectangle<float> mixerSection() { return { 198.0f, 58.0f, 70.0f, 140.0f }; }
    static juce::Rectangle<float> vcfSection() { return { 260.0f, 58.0f, 210.0f, 140.0f }; }
    static juce::Rectangle<float> outputSection() { return { 500.0f, 58.0f, 130.0f, 140.0f }; }
    static juce::Rectangle<float> potBankSection() { return { 48.0f, 205.0f, 420.0f, 150.0f }; }
    static juce::Rectangle<float> seqSection() { return { 410.0f, 205.0f, 210.0f, 190.0f }; }

    static juce::Rectangle<int> sectionToEditor(juce::Rectangle<float> section)
    {
        return { juce::roundToInt(section.getX() * Layout::scaleX),
                 juce::roundToInt(section.getY() * Layout::scaleY),
                 juce::roundToInt(section.getWidth() * Layout::scaleX),
                 juce::roundToInt(section.getHeight() * Layout::scaleY) };
    }
};

}  // namespace lxx::ui
