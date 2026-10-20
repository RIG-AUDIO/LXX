#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lxx::ui {

/**
 * DOS terminal colour language for the entire LXX UI
 * (GUI / GUI+CLI / CLI): near-black + phosphor green + DIN typography.
 */
struct Palette {
    static juce::Colour background() { return juce::Colour(0xff000000); }
    static juce::Colour panelDeep() { return juce::Colour(0xff050805); }
    static juce::Colour panel() { return juce::Colour(0xff0a100c); }
    static juce::Colour panelRaised() { return juce::Colour(0xff0f1812); }
    static juce::Colour edge() { return juce::Colour(0xff1a6633); }
    static juce::Colour line() { return juce::Colour(0xff143322); }
    static juce::Colour grid() { return juce::Colour(0xff1a4028); }
    static juce::Colour accent() { return juce::Colour(0xff33ff66); }
    static juce::Colour accentHot() { return juce::Colour(0xff66ff99); }
    static juce::Colour cream() { return juce::Colour(0xff33ff66); } // primary text (compat name)
    static juce::Colour mutedText() { return juce::Colour(0xff1a9940); }
    static juce::Colour disabled() { return juce::Colour(0xff0d331a); }
    static juce::Colour focus() { return juce::Colour(0xff55ff88); }
    static juce::Colour knobBody() { return juce::Colour(0xff080c09); }
    static juce::Colour knobInner() { return juce::Colour(0xff030503); }
    static juce::Colour knobRim() { return juce::Colour(0xff1a6633); }
    static juce::Colour knobActive() { return accent(); }
    static juce::Colour knobIndicator() { return accent(); }
    static juce::Colour moduleHeaderBg() { return juce::Colour(0xff0c1610); }
    static juce::Colour moduleHeaderText() { return accent(); }
    static juce::Colour authenticBadge() { return juce::Colour(0xff1a6633); }
    static juce::Colour extendedBadge() { return juce::Colour(0xff227744); }
    static juce::Colour placeholderOverlay() { return juce::Colour(0x99000000); }
};

/**
 * Helvetian / International Style layout constants.
 * Editor uses responsive column geometry — not a global AffineTransform scale.
 */
struct Layout {
    static constexpr int designWidth = 1040;
    static constexpr int analogHeight = 780;
    static constexpr int designHeight = 780;
    static constexpr int minWidth = 900;
    static constexpr int minHeight = 640;
    static constexpr int maxWidth = 1600;
    static constexpr int maxHeight = 1200;

    static constexpr float sourceWidth = 656.67f;
    static constexpr float sourceHeight = 419.0f;
    static constexpr float scaleX = designWidth / sourceWidth;
    static constexpr float scaleY = analogHeight / sourceHeight;

    static constexpr int margin = 12;
    static constexpr int gap = 8;
    static constexpr int colGap = 10;
    static constexpr int unit = 8;          // vertical rhythm
    static constexpr int headerH = 44;
    static constexpr int sectionTitleH = 18;
    static constexpr int columns = 12;

    static constexpr int knobStandard = 64;
    static constexpr int knobCompact = 56;
    static constexpr int knobLarge = 84;
    static constexpr int knobLabelHeight = 12;
    static constexpr int moduleRadius = 0; // Swiss: square modules
    static constexpr int moduleHeaderRow = 18;

    /** Column x within a 12-col grid inside `area`. */
    static juce::Rectangle<int> col(juce::Rectangle<int> area, int startCol, int span)
    {
        const int inner = area.getWidth() - (columns - 1) * colGap;
        const float cw = static_cast<float>(inner) / static_cast<float>(columns);
        const int x = area.getX() + juce::roundToInt(static_cast<float>(startCol) * (cw + static_cast<float>(colGap)));
        const int w = juce::jmax(1, juce::roundToInt(static_cast<float>(span) * cw + static_cast<float>(span - 1) * colGap));
        return { x, area.getY(), w, area.getHeight() };
    }
};

}  // namespace lxx::ui
