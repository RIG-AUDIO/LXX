juce_add_plugin(LXXPlugin
    COMPANY_NAME "RIG-AUDIO"
    BUNDLE_ID com.rigaudio.lxx
    PLUGIN_MANUFACTURER_CODE RgAu
    PLUGIN_CODE Lxx1
    FORMATS VST3
    PRODUCT_NAME "LXX"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
)

juce_generate_juce_header(LXXPlugin)

juce_add_binary_data(LXXFonts SOURCES
    resources/Fonts/din1451alt.ttf
)

target_sources(LXXPlugin
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        parameters/ParameterLayout.cpp
        ui/LXXLookAndFeel.cpp
        ui/LXXKnob.cpp
        ui/LXXSkin.cpp
        ui/TerminalCommandRouter.cpp
        ui/SeqGridComponent.cpp
)

target_include_directories(LXXPlugin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(LXXPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(LXXPlugin
    PRIVATE
        lxx_dsp
        LXXFonts
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        # juce::juce_recommended_lto_flags is deliberately NOT linked.  With
        # clang it adds -flto to the link line while the plugin's object set is
        # not consistently LTO, and the VST3 link then fails with unresolved
        # juce:: symbols (JucePluginFactory::createInstance, juce::Colour,
        # juce::AudioChannelSet::ambisonic, ...).  LTO changes no audio output
        # and buys nothing here, so the build stays homogeneous.
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(LXXPlugin PRIVATE /W4 /permissive-)
endif()

juce_add_console_app(LXXHostRuntimeTest PRODUCT_NAME "LXXHostRuntimeTest")
juce_generate_juce_header(LXXHostRuntimeTest)
target_sources(LXXHostRuntimeTest
    PRIVATE
        test_host_runtime.cpp
        PluginProcessor.cpp
        PluginEditor.cpp
        parameters/ParameterLayout.cpp
        ui/LXXLookAndFeel.cpp
        ui/LXXKnob.cpp
        ui/LXXSkin.cpp
        ui/TerminalCommandRouter.cpp
        ui/SeqGridComponent.cpp
)
target_include_directories(LXXHostRuntimeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(LXXHostRuntimeTest
    PRIVATE
        JucePlugin_Name="LXX"
        JucePlugin_Desc="LXX"
        JucePlugin_Manufacturer="RIG-AUDIO"
        JucePlugin_ManufacturerWebsite=""
        JucePlugin_ManufacturerEmail=""
        JucePlugin_ManufacturerCode=0x52674175
        JucePlugin_PluginCode=0x4c787831
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=1
        JucePlugin_IsMidiEffect=0
        JucePlugin_EditorRequiresKeyboardFocus=1
        JucePlugin_Version=1.0.0
        JucePlugin_VersionCode=0x10000
        JucePlugin_VersionString="1.0.0"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries(LXXHostRuntimeTest
    PRIVATE
        lxx_dsp
        LXXFonts
        juce::juce_audio_utils
        juce::juce_audio_processors
)
if(MSVC)
    target_compile_options(LXXHostRuntimeTest PRIVATE /W4 /permissive-)
endif()
