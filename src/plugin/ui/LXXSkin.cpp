#include "ui/LXXSkin.h"

namespace lxx::ui {

juce::Rectangle<int> GuiGeometry::editorBounds(GuiSize size)
{
    switch (size) {
        case GuiSize::small: return { 0, 0, 790, 608 };
        case GuiSize::large: return { 0, 0, 1234, 950 };
        case GuiSize::medium: break;
    }
    return { 0, 0, designWidth, designHeight };
}

float GuiGeometry::scaleFor(GuiSize size)
{
    const auto bounds = editorBounds(size);
    return static_cast<float>(bounds.getWidth()) / static_cast<float>(designWidth);
}

const char* GuiGeometry::stableId(GuiSize size)
{
    switch (size) {
        case GuiSize::small: return "small";
        case GuiSize::large: return "large";
        case GuiSize::medium: break;
    }
    return "medium";
}

const char* UiPreferences::skinStableId(SkinId skin)
{
    switch (skin) {
        case SkinId::terminal: return "terminal";
        case SkinId::raytracing: return "raytracing";
        case SkinId::raytracingPhoto: return "raytracing_photo";
        case SkinId::defaultSkin: break;
    }
    return "default";
}

const char* UiPreferences::skinDisplayName(SkinId skin)
{
    switch (skin) {
        case SkinId::terminal: return "Terminal";
        case SkinId::raytracing: return "Raytracing";
        case SkinId::raytracingPhoto: return "Raytracing — Photo Mode";
        case SkinId::defaultSkin: break;
    }
    return "Default";
}

std::unique_ptr<juce::PropertiesFile> UiPreferences::open()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "LXX";
    options.filenameSuffix = ".settings";
    options.folderName = "RIG-AUDIO/LXX";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    return std::make_unique<juce::PropertiesFile>(options);
}

SkinId UiPreferences::loadSkin()
{
    const auto properties = open();
    const auto value = properties->getValue("skin", "default");
    if (value == "terminal") return SkinId::terminal;
    if (value == "raytracing") return SkinId::raytracing;
    if (value == "raytracing_photo") return SkinId::raytracingPhoto;
    return SkinId::defaultSkin;
}

GuiSize UiPreferences::loadGuiSize()
{
    const auto properties = open();
    const auto value = properties->getValue("gui_size", "medium");
    if (value == "small") return GuiSize::small;
    if (value == "large") return GuiSize::large;
    return GuiSize::medium;
}

void UiPreferences::save(SkinId skin, GuiSize size)
{
    auto properties = open();
    properties->setValue("skin", skinStableId(skin));
    properties->setValue("gui_size", GuiGeometry::stableId(size));
    properties->saveIfNeeded();
}

}  // namespace lxx::ui
