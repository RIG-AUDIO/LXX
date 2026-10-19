#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include "ui/LXXUiPalette.h"

namespace lxx::ui {

enum class SkinId {
    defaultSkin,
    terminal,
    raytracing,
    raytracingPhoto
};

enum class GuiSize { small, medium, large };

struct GuiGeometry {
    static constexpr int designWidth = Layout::designWidth;
    static constexpr int designHeight = Layout::designHeight;

    static juce::Rectangle<int> editorBounds(GuiSize size);
    static float scaleFor(GuiSize size);
    static const char* stableId(GuiSize size);
};

class UiPreferences {
public:
    static SkinId loadSkin();
    static GuiSize loadGuiSize();
    static void save(SkinId skin, GuiSize size);

    static const char* skinStableId(SkinId skin);
    static const char* skinDisplayName(SkinId skin);

private:
    static std::unique_ptr<juce::PropertiesFile> open();
};

}  // namespace lxx::ui
