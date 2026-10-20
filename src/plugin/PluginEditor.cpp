#include "PluginEditor.h"

#include "parameters/ParameterIDs.h"
#include "ui/LXXUiPalette.h"

namespace lxx::ui {
void CommandConsole::setProcessor(lxx::LXXAudioProcessor* processor) noexcept
{
    processor_ = processor;
    apvts_ = processor != nullptr ? &processor->apvts() : nullptr;
    router_ = processor != nullptr ? &processor->commandRouter() : nullptr;
}
}  // namespace lxx::ui

namespace lxx {

using namespace params;
using namespace ui;

LXXAudioProcessorEditor::LXXAudioProcessorEditor(LXXAudioProcessor& processorRef)
    : juce::AudioProcessorEditor(processorRef), processor_(processorRef)
{
    setLookAndFeel(&lookAndFeel_);
    constrainer_.setSizeLimits(Layout::minWidth, Layout::minHeight, Layout::maxWidth, Layout::maxHeight);
    setConstrainer(&constrainer_);
    setResizable(true, true);
    setWantsKeyboardFocus(true);
    designSurface_.owner = this;
    addAndMakeVisible(designSurface_);

    uiPageBox_.addItemList({ "GUI", "GUI+CLI", "CLI" }, 1);
    guiSubBox_.addItemList({ "Main", "Advanced" }, 1);
    clockDivisionBox_.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 1);
    drumPageBox_.addItemList({ "Simple", "Extended" }, 1);
    sequencerModeBox_.addItemList({ "Legacy", "Grids", "Techno", "Tech Ext", "DNB", "DNB Ext" }, 1);
    lowCharacterBox_.addItemList({ "Auto", "Boom", "Tom" }, 1);

    for (auto* b : { &hostSyncToggle_, &midiOutToggle_, &runToggle_, &seqOnlyToggle_ }) {
        b->setClickingTogglesState(true);
        b->setColour(juce::ToggleButton::textColourId, Palette::accent());
        designSurface_.addAndMakeVisible(b);
    }
    for (auto* c : { &uiPageBox_, &guiSubBox_, &clockDivisionBox_, &drumPageBox_,
                     &sequencerModeBox_, &lowCharacterBox_ })
        designSurface_.addAndMakeVisible(c);

    for (auto* b : { &generateBtn_, &mutateBtn_, &randomizeKitBtn_ }) {
        b->setColour(juce::TextButton::textColourOffId, Palette::accent());
        b->setColour(juce::TextButton::textColourOnId, Palette::accent());
        designSurface_.addAndMakeVisible(b);
    }
    generateBtn_.onClick = [this] { runCli("generate"); };
    mutateBtn_.onClick = [this] { runCli("mutate 20"); };
    randomizeKitBtn_.onClick = [this] { runCli("randomize kit 40"); };

    locksLabel_.setColour(juce::Label::textColourId, Palette::mutedText());
    locksLabel_.setJustificationType(juce::Justification::centredLeft);
    designSurface_.addAndMakeVisible(locksLabel_);

    auto styleToggle = [](juce::ToggleButton& b) {
        b.setClickingTogglesState(true);
        b.setColour(juce::ToggleButton::textColourId, Palette::mutedText());
        b.setColour(juce::ToggleButton::tickColourId, Palette::accent());
    };
    for (auto* b : { &muteKick_, &muteSnare_, &muteCh_, &muteLow_, &muteMid_, &muteOh_,
                     &lockKick_, &lockSnare_, &lockCh_, &lockLow_, &lockMid_, &lockOh_ }) {
        styleToggle(*b);
        designSurface_.addAndMakeVisible(b);
    }

    for (auto* knob : {
             &drumsX_, &drumsY_, &bdDensity_, &sdDensity_, &hhDensity_,
             &bdTone_, &sdTone_, &hhTone_, &drumsBalance_, &drumsBandwidth_,
             &bdPitch_, &bdPitchDecay_, &bdPitchMod_, &bdAmpDecay_, &bdCrunch_,
             &sdPitch_, &sdPitchDecay_, &sdPitchMod_, &sdAmpDecay_, &sdSnappy_,
             &hhPitch_, &hhAmpDecay_, &gridsChaos_,
             &technoDensity_, &technoVariation_, &technoSyncopation_,
             &technoFill_, &technoSwing_, &technoSeed_,
             &ohDecay_, &ohTone_,
             &lowPitch_, &lowDecay_, &lowTone_,
             &midPitch_, &midDecay_, &midTone_,
             &volKick_, &volSnare_, &volCh_, &volLow_, &volMid_, &volOh_,
             &swing_, &tempo_, &volume_ })
        designSurface_.addAndMakeVisible(knob);

    auto fmtDb = [](double v) { return juce::String(v, 1) + " dB"; };
    for (auto* k : { &volKick_, &volSnare_, &volCh_, &volLow_, &volMid_, &volOh_ })
        k->slider().textFromValueFunction = fmtDb;

    designSurface_.addAndMakeVisible(seqGrid_);
    designSurface_.addAndMakeVisible(commandConsole_);
    commandConsole_.setProcessor(&processor_);

    seqOnlyToggle_.onClick = [this] { enforceSeqOnlyMidi(); };

    layoutReady_ = true;
    attachAllControls();
    setSize(Layout::designWidth, Layout::designHeight);
    refreshVisibility();
    startTimerHz(30);
}

LXXAudioProcessorEditor::~LXXAudioProcessorEditor()
{
    stopTimer();
    layoutReady_ = false;
    setLookAndFeel(nullptr);
}

void LXXAudioProcessorEditor::setSkinForTests(ui::SkinId) {}
void LXXAudioProcessorEditor::setGuiSizeForTests(ui::GuiSize) {}
ui::SkinId LXXAudioProcessorEditor::skinForTests() const noexcept { return ui::SkinId::defaultSkin; }

int LXXAudioProcessorEditor::uiMode() const { return uiPageBox_.getSelectedItemIndex(); }
bool LXXAudioProcessorEditor::isAdvanced() const { return guiSubBox_.getSelectedItemIndex() >= 1; }
int LXXAudioProcessorEditor::seqMode() const { return sequencerModeBox_.getSelectedItemIndex(); }
bool LXXAudioProcessorEditor::extendedSeq() const { return seqMode() == 3 || seqMode() == 5; }
bool LXXAudioProcessorEditor::legacyDensitiesActive() const { return seqMode() <= 1; }
bool LXXAudioProcessorEditor::mapXYActive() const { return seqMode() <= 1; }
bool LXXAudioProcessorEditor::technoMacrosActive() const { return seqMode() >= 2; }

void LXXAudioProcessorEditor::runCli(const juce::String& line)
{
    juce::String fb;
    processor_.commandRouter().tryExecute(line, processor_.apvts(), fb);
}

void LXXAudioProcessorEditor::enforceSeqOnlyMidi()
{
    if (seqOnlyToggle_.getToggleState())
        if (auto* p = processor_.apvts().getParameter(IDs::midiOut))
            if (p->getValue() < 0.5f) {
                p->beginChangeGesture();
                p->setValueNotifyingHost(1.0f);
                p->endChangeGesture();
            }
}

void LXXAudioProcessorEditor::attachKnob(LXXKnob& knob, const char* paramId)
{
    attachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.apvts(), paramId, knob.slider()));
}

void LXXAudioProcessorEditor::attachAllControls()
{
    attachments_.clear();
    muteLockAttachments_.clear();
    attachKnob(drumsX_, IDs::drumsX);
    attachKnob(drumsY_, IDs::drumsY);
    attachKnob(bdDensity_, IDs::bdDensity);
    attachKnob(sdDensity_, IDs::sdDensity);
    attachKnob(hhDensity_, IDs::hhDensity);
    attachKnob(bdTone_, IDs::bdTone);
    attachKnob(sdTone_, IDs::sdTone);
    attachKnob(hhTone_, IDs::hhTone);
    attachKnob(drumsBalance_, IDs::drumsBalance);
    attachKnob(drumsBandwidth_, IDs::drumsBandwidth);
    attachKnob(bdPitch_, IDs::bdPitch);
    attachKnob(bdPitchDecay_, IDs::bdPitchDecay);
    attachKnob(bdPitchMod_, IDs::bdPitchMod);
    attachKnob(bdAmpDecay_, IDs::bdAmpDecay);
    attachKnob(bdCrunch_, IDs::bdCrunch);
    attachKnob(sdPitch_, IDs::sdPitch);
    attachKnob(sdPitchDecay_, IDs::sdPitchDecay);
    attachKnob(sdPitchMod_, IDs::sdPitchMod);
    attachKnob(sdAmpDecay_, IDs::sdAmpDecay);
    attachKnob(sdSnappy_, IDs::sdSnappy);
    attachKnob(hhPitch_, IDs::hhPitch);
    attachKnob(hhAmpDecay_, IDs::hhAmpDecay);
    attachKnob(swing_, IDs::swing);
    attachKnob(tempo_, IDs::tempo);
    attachKnob(volume_, IDs::volume);
    attachKnob(gridsChaos_, IDs::gridsChaos);
    attachKnob(technoDensity_, IDs::technoDensity);
    attachKnob(technoVariation_, IDs::technoVariation);
    attachKnob(technoSyncopation_, IDs::technoSyncopation);
    attachKnob(technoFill_, IDs::technoFill);
    attachKnob(technoSwing_, IDs::technoSwing);
    attachKnob(technoSeed_, IDs::technoSeed);
    attachKnob(ohDecay_, IDs::ohDecay);
    attachKnob(ohTone_, IDs::ohTone);
    attachKnob(lowPitch_, IDs::lowPitch);
    attachKnob(lowDecay_, IDs::lowDecay);
    attachKnob(lowTone_, IDs::lowTone);
    attachKnob(midPitch_, IDs::midPitch);
    attachKnob(midDecay_, IDs::midDecay);
    attachKnob(midTone_, IDs::midTone);
    attachKnob(volKick_, IDs::volKick);
    attachKnob(volSnare_, IDs::volSnare);
    attachKnob(volCh_, IDs::volCh);
    attachKnob(volLow_, IDs::volLow);
    attachKnob(volMid_, IDs::volMid);
    attachKnob(volOh_, IDs::volOh);

    auto attachBtn = [this](juce::ToggleButton& b, const char* id) {
        muteLockAttachments_.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor_.apvts(), id, b));
    };
    attachBtn(muteKick_, IDs::muteKick);
    attachBtn(muteSnare_, IDs::muteSnare);
    attachBtn(muteCh_, IDs::muteCh);
    attachBtn(muteLow_, IDs::muteLow);
    attachBtn(muteMid_, IDs::muteMid);
    attachBtn(muteOh_, IDs::muteOh);
    attachBtn(lockKick_, IDs::lockKick);
    attachBtn(lockSnare_, IDs::lockSnare);
    attachBtn(lockCh_, IDs::lockCh);
    attachBtn(lockLow_, IDs::lockLow);
    attachBtn(lockMid_, IDs::lockMid);
    attachBtn(lockOh_, IDs::lockOh);

    hostSyncAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.apvts(), IDs::hostSync, hostSyncToggle_);
    midiOutAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.apvts(), IDs::midiOut, midiOutToggle_);
    runAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.apvts(), IDs::seqRun, runToggle_);
    seqOnlyAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.apvts(), IDs::seqOnly, seqOnlyToggle_);
    clockDivisionAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), IDs::clockDivision, clockDivisionBox_);
    drumPageAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), IDs::drumPage, drumPageBox_);
    sequencerModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), IDs::sequencerMode, sequencerModeBox_);
    uiPageAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), IDs::uiPage, uiPageBox_);
    guiSubAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), IDs::guiSubPage, guiSubBox_);
    lowCharacterAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), IDs::lowCharacter, lowCharacterBox_);
}

void LXXAudioProcessorEditor::placeKnob(LXXKnob& knob, int x, int y, int w, int h)
{
    knob.setBounds(x, y, w, h);
}

void LXXAudioProcessorEditor::placeKnob(LXXKnob& knob, juce::Rectangle<int> cell)
{
    const int s = juce::jmin(cell.getWidth(), cell.getHeight());
    knob.setBounds(cell.withSizeKeepingCentre(s, s));
}

void LXXAudioProcessorEditor::drawSectionFrame(juce::Graphics& g, juce::Rectangle<int> bounds,
                                                   const juce::String& title) const
{
    g.setColour(Palette::panel());
    g.fillRect(bounds.toFloat());
    g.setColour(Palette::edge());
    g.drawRect(bounds.toFloat(), 1.0f);
    if (title.isNotEmpty()) {
        auto header = bounds.removeFromTop(Layout::sectionTitleH);
        g.setColour(Palette::moduleHeaderBg());
        g.fillRect(header.toFloat());
        g.setColour(Palette::accent());
        g.setFont(lookAndFeel_.uiFont(11.0f));
        g.drawText(title, header.reduced(6, 0), juce::Justification::centredLeft, false);
    }
}

void LXXAudioProcessorEditor::layoutKnobRow(juce::Rectangle<int> area, ui::LXXKnob* knobs[], int n, int knobSize)
{
    if (n <= 0) return;
    const int gap = Layout::gap;
    const int total = n * knobSize + (n - 1) * gap;
    int x = area.getX() + juce::jmax(0, (area.getWidth() - total) / 2);
    const int y = area.getY() + juce::jmax(0, (area.getHeight() - knobSize) / 2);
    for (int i = 0; i < n; ++i) {
        placeKnob(*knobs[i], x, y, knobSize, knobSize);
        knobs[i]->setVisible(true);
        x += knobSize + gap;
    }
}

void LXXAudioProcessorEditor::layoutAdvancedVoice(juce::Rectangle<int> area,
                                                      ui::LXXKnob* knobs[], int nKnobs,
                                                      ui::LXXKnob& vol, juce::ToggleButton& mute,
                                                      juce::ToggleButton& lock, juce::Component* extra)
{
    auto body = area.reduced(Layout::gap).withTrimmedTop(Layout::sectionTitleH);
    const int ks = juce::jlimit(48, Layout::knobStandard,
                                juce::jmin(body.getHeight() - 8, (body.getWidth() - 90) / juce::jmax(1, nKnobs + 1) - 4));
    int x = body.getX();
    const int y = body.getY() + 4;
    for (int i = 0; i < nKnobs; ++i) {
        placeKnob(*knobs[i], x, y, ks, ks);
        knobs[i]->setVisible(true);
        x += ks + 4;
    }
    placeKnob(vol, x, y, ks, ks);
    vol.setVisible(true);
    mute.setBounds(x + ks + 4, y + 4, 40, 22);
    mute.setVisible(true);
    lock.setButtonText("LOCK");
    lock.setBounds(x + ks + 4, y + 30, 52, 22);
    lock.setVisible(true);
    if (extra != nullptr) {
        extra->setBounds(body.getX(), body.getBottom() - 24, 100, 22);
        extra->setVisible(true);
    }
}

void LXXAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(Palette::background()); }

void LXXAudioProcessorEditor::DesignSurface::paint(juce::Graphics& g)
{
    if (owner == nullptr) return;
    const int mode = owner->uiMode();
    g.fillAll(Palette::background());
    if (mode >= 2)
        return;

    owner->drawSectionFrame(g, headerArea, {});
    g.setColour(Palette::accent());
    g.setFont(owner->lookAndFeel_.uiFont(20.0f));
    g.drawText("LXX", headerArea.getX() + 8, headerArea.getY() + 8, 100, 26,
               juce::Justification::centredLeft, false);
    g.setColour(Palette::mutedText());
    g.setFont(owner->lookAndFeel_.uiFont(10.0f));
    g.drawText("RIG-AUDIO", headerArea.getX() + 110, headerArea.getY() + 14, 120, 16,
               juce::Justification::centredLeft, false);

    if (owner->isAdvanced()) {
        owner->drawSectionFrame(g, advKick, "KICK  ·  C1=36");
        owner->drawSectionFrame(g, advLow, "BOOM / LOW TOM  ·  B0=35");
        owner->drawSectionFrame(g, advSnare, "SNARE  ·  D1=38");
        owner->drawSectionFrame(g, advPerc, "PERC  ·  D#1=39");
        owner->drawSectionFrame(g, advCh, "CLOSED HAT  ·  F#1=42");
        owner->drawSectionFrame(g, advOh, "OPEN HAT  ·  A#1=46");
        if (mode == 0)
            owner->drawSectionFrame(g, advTransport, "TRANSPORT / OUTPUT");
        if (!owner->extendedSeq() && owner->seqMode() != 2 && owner->seqMode() != 4) {
            g.setColour(Palette::mutedText());
            g.setFont(owner->lookAndFeel_.uiFont(10.0f));
            g.drawText("EXTENDED — manual / MIDI (seq not generating Boom/Perc/OH)",
                       advLow.getX() + 8, advLow.getY() + 2, advLow.getWidth() - 16, 14,
                       juce::Justification::centredLeft, false);
        }
    } else {
        owner->drawSectionFrame(g, soundArea, "SOUND");
        owner->drawSectionFrame(g, genArea, "GENERATOR");
        owner->drawSectionFrame(g, transportArea, "TRANSPORT");
        owner->drawSectionFrame(g, globalArea, "OUTPUT  ·  BITCRUSH / MASTER");
        owner->drawSectionFrame(g, locksArea, "LOCK");
        if (mode == 0)
            owner->drawSectionFrame(g, stepArea, "STEP");
    }
}

void LXXAudioProcessorEditor::resized()
{
    if (!layoutReady_) return;
    designSurface_.setBounds(getLocalBounds());

    const int mode = uiMode();
    const bool cliOnly = mode >= 2;
    const bool hybrid = mode == 1;
    auto bounds = designSurface_.getLocalBounds().reduced(Layout::margin);

    if (cliOnly) {
        designSurface_.headerArea = bounds.removeFromTop(Layout::headerH);
        commandConsole_.setBounds(bounds);
        uiPageBox_.setBounds(designSurface_.headerArea.removeFromRight(120).reduced(0, 10));
        runToggle_.setBounds(designSurface_.headerArea.removeFromRight(80).reduced(4, 8));
        seqOnlyToggle_.setBounds(designSurface_.headerArea.removeFromRight(100).reduced(4, 8));
        sequencerModeBox_.setBounds(designSurface_.headerArea.removeFromRight(120).reduced(0, 10));
        refreshVisibility();
        return;
    }

    if (hybrid) {
        auto cliStrip = bounds.removeFromBottom(juce::jmax(140, bounds.getHeight() / 5));
        bounds.removeFromBottom(Layout::gap);
        commandConsole_.setBounds(cliStrip);
    } else {
        commandConsole_.setBounds(0, 0, 0, 0);
    }

    designSurface_.headerArea = bounds.removeFromTop(Layout::headerH);
    bounds.removeFromTop(Layout::gap);

    auto header = designSurface_.headerArea;
    uiPageBox_.setBounds(header.removeFromLeft(110).reduced(0, 10));
    header.removeFromLeft(Layout::gap);
    guiSubBox_.setBounds(header.removeFromLeft(100).reduced(0, 10));
    header.removeFromLeft(Layout::gap);
    runToggle_.setBounds(header.removeFromLeft(78).reduced(0, 8));
    seqOnlyToggle_.setBounds(header.removeFromLeft(100).reduced(0, 8));
    header.removeFromLeft(Layout::gap);
    sequencerModeBox_.setBounds(header.removeFromLeft(120).reduced(0, 10));

    if (isAdvanced()) {
        if (!hybrid) {
            designSurface_.advTransport = bounds.removeFromBottom(72);
            bounds.removeFromBottom(Layout::gap);
        } else {
            designSurface_.advTransport = {};
        }
        const int rowH = (bounds.getHeight() - 2 * Layout::gap) / 3;
        auto row0 = bounds.removeFromTop(rowH);
        bounds.removeFromTop(Layout::gap);
        auto row1 = bounds.removeFromTop(rowH);
        bounds.removeFromTop(Layout::gap);
        auto row2 = bounds;
        const int half = (row0.getWidth() - Layout::colGap) / 2;
        designSurface_.advKick = row0.removeFromLeft(half);
        row0.removeFromLeft(Layout::colGap);
        designSurface_.advLow = row0;
        designSurface_.advSnare = row1.removeFromLeft(half);
        row1.removeFromLeft(Layout::colGap);
        designSurface_.advPerc = row1;
        designSurface_.advCh = row2.removeFromLeft(half);
        row2.removeFromLeft(Layout::colGap);
        designSurface_.advOh = row2;
    } else {
        if (!hybrid) {
            designSurface_.stepArea = bounds.removeFromBottom(40);
            bounds.removeFromBottom(Layout::gap);
        } else {
            designSurface_.stepArea = {};
        }
        designSurface_.locksArea = bounds.removeFromBottom(36);
        bounds.removeFromBottom(Layout::gap);

        const int bottomH = juce::jmax(120, bounds.getHeight() * 28 / 100);
        auto bottom = bounds.removeFromBottom(bottomH);
        bounds.removeFromBottom(Layout::gap);
        const int transportW = bottom.getWidth() * 58 / 100;
        designSurface_.transportArea = bottom.removeFromLeft(transportW);
        bottom.removeFromLeft(Layout::colGap);
        designSurface_.globalArea = bottom;

        const int soundW = bounds.getWidth() * 60 / 100;
        designSurface_.soundArea = bounds.removeFromLeft(soundW);
        bounds.removeFromLeft(Layout::colGap);
        designSurface_.genArea = bounds;
    }

    refreshVisibility();
}

void LXXAudioProcessorEditor::refreshVisibility()
{
    const int mode = uiMode();
    const bool cliOnly = mode >= 2;
    const bool hybrid = mode == 1;
    const bool adv = isAdvanced();
    const int seq = seqMode();
    const bool grids = seq == 1;
    const bool showExtVoices = extendedSeq() || adv;
    const bool densOk = legacyDensitiesActive();
    const bool mapOk = mapXYActive();
    const bool macrosOk = technoMacrosActive();

    commandConsole_.setVisible(cliOnly || hybrid);

    auto hideAll = [this] {
        for (auto* c : designSurface_.getChildren()) {
            if (dynamic_cast<LXXKnob*>(c) != nullptr
                || dynamic_cast<juce::TextButton*>(c) != nullptr)
                c->setVisible(false);
        }
        for (auto* b : { &muteKick_, &muteSnare_, &muteCh_, &muteLow_, &muteMid_, &muteOh_,
                         &lockKick_, &lockSnare_, &lockCh_, &lockLow_, &lockMid_, &lockOh_ })
            b->setVisible(false);
        locksLabel_.setVisible(false);
        lowCharacterBox_.setVisible(false);
        generateBtn_.setVisible(false);
        mutateBtn_.setVisible(false);
        randomizeKitBtn_.setVisible(false);
    };

    auto refreshLockColour = [](juce::ToggleButton& b) {
        b.setColour(juce::ToggleButton::textColourId,
                    b.getToggleState() ? Palette::accent() : Palette::mutedText());
    };
    for (auto* b : { &lockKick_, &lockSnare_, &lockCh_, &lockLow_, &lockMid_, &lockOh_ })
        refreshLockColour(*b);

    if (cliOnly) {
        hideAll();
        seqGrid_.setVisible(false);
        hostSyncToggle_.setVisible(false);
        midiOutToggle_.setVisible(false);
        clockDivisionBox_.setVisible(false);
        drumPageBox_.setVisible(false);
        guiSubBox_.setVisible(false);
        return;
    }

    guiSubBox_.setVisible(true);
    hideAll();
    seqGrid_.setVisible(!hybrid && !designSurface_.stepArea.isEmpty());
    hostSyncToggle_.setVisible(true);
    midiOutToggle_.setVisible(true);
    clockDivisionBox_.setVisible(true);

    const int ks = Layout::knobStandard;

    if (adv) {
        ui::LXXKnob* kickK[] = { &bdPitch_, &bdAmpDecay_, &bdTone_, &bdCrunch_ };
        layoutAdvancedVoice(designSurface_.advKick, kickK, 4, volKick_, muteKick_, lockKick_);

        ui::LXXKnob* lowK[] = { &lowPitch_, &lowDecay_, &lowTone_ };
        layoutAdvancedVoice(designSurface_.advLow, lowK, 3, volLow_, muteLow_, lockLow_, &lowCharacterBox_);

        ui::LXXKnob* snareK[] = { &sdPitch_, &sdAmpDecay_, &sdTone_, &sdSnappy_ };
        layoutAdvancedVoice(designSurface_.advSnare, snareK, 4, volSnare_, muteSnare_, lockSnare_);

        ui::LXXKnob* midK[] = { &midPitch_, &midDecay_, &midTone_ };
        layoutAdvancedVoice(designSurface_.advPerc, midK, 3, volMid_, muteMid_, lockMid_);

        ui::LXXKnob* chK[] = { &hhPitch_, &hhAmpDecay_, &hhTone_ };
        layoutAdvancedVoice(designSurface_.advCh, chK, 3, volCh_, muteCh_, lockCh_);

        ui::LXXKnob* ohK[] = { &ohTone_, &ohDecay_ };
        layoutAdvancedVoice(designSurface_.advOh, ohK, 2, volOh_, muteOh_, lockOh_);

        const float extAlpha = showExtVoices ? 1.0f : 0.55f;
        juce::Component* dim[] = {
            &lowPitch_, &lowDecay_, &lowTone_, &volLow_,
            &midPitch_, &midDecay_, &midTone_, &volMid_,
            &ohTone_, &ohDecay_, &volOh_, &lowCharacterBox_
        };
        for (auto* c : dim)
            c->setAlpha(extAlpha);

        if (!hybrid && !designSurface_.advTransport.isEmpty()) {
            auto t = designSurface_.advTransport.reduced(Layout::gap).withTrimmedTop(Layout::sectionTitleH);
            const int y = t.getY();
            int x = t.getX();
            placeKnob(tempo_, x, y, ks, ks); tempo_.setVisible(true); x += ks + 4;
            placeKnob(swing_, x, y, ks, ks); swing_.setVisible(true); x += ks + Layout::gap;
            hostSyncToggle_.setBounds(x, y + 18, 100, 24); x += 108;
            clockDivisionBox_.setBounds(x, y + 18, 90, 24); x += 98;
            midiOutToggle_.setBounds(x, y + 48, 100, 24); x += 108;
            placeKnob(drumsBandwidth_, x, y, ks, ks); drumsBandwidth_.setVisible(true);
            const int ms = Layout::knobLarge;
            placeKnob(volume_, t.getRight() - ms - 8, y, ms, ms); volume_.setVisible(true);
            seqGrid_.setBounds(t.getX() + 2 * (ks + 4), t.getBottom() - 22,
                               juce::jmax(120, t.getWidth() - ms - 2 * ks - 40), 22);
        } else if (hybrid) {
            placeKnob(tempo_, Layout::margin + 8, getHeight() - 200, ks, ks); tempo_.setVisible(true);
            placeKnob(drumsBandwidth_, Layout::margin + 80, getHeight() - 200, ks, ks);
            drumsBandwidth_.setVisible(true);
            placeKnob(volume_, getWidth() - Layout::knobLarge - Layout::margin - 8,
                      getHeight() - 210, Layout::knobLarge, Layout::knobLarge);
            volume_.setVisible(true);
        }
        drumPageBox_.setVisible(false);
    } else {
        // --- MAIN ---
        auto sound = designSurface_.soundArea.reduced(Layout::gap).withTrimmedTop(Layout::sectionTitleH);
        auto gen = designSurface_.genArea.reduced(Layout::gap).withTrimmedTop(Layout::sectionTitleH);
        auto transport = designSurface_.transportArea.reduced(Layout::gap).withTrimmedTop(Layout::sectionTitleH);
        auto global = designSurface_.globalArea.reduced(Layout::gap).withTrimmedTop(Layout::sectionTitleH);
        auto locks = designSurface_.locksArea.reduced(Layout::gap, 4).withTrimmedTop(2);

        // Core tones + balance
        {
            ui::LXXKnob* core[] = { &bdTone_, &sdTone_, &hhTone_, &drumsBalance_ };
            auto row = sound.removeFromTop(ks + 8);
            layoutKnobRow(row, core, 4, juce::jmin(ks, row.getHeight() - 4));
        }

        if (densOk) {
            ui::LXXKnob* dens[] = { &bdDensity_, &sdDensity_, &hhDensity_ };
            auto row = sound.removeFromTop(ks + 8);
            layoutKnobRow(row, dens, 3, juce::jmin(ks, row.getHeight() - 4));
        }

        // Extended focused Main controls
        if (extendedSeq()) {
            auto row = sound.removeFromTop(ks + 28);
            const int cellW = (row.getWidth() - 2 * Layout::gap) / 3;
            auto placePair = [&](juce::Rectangle<int> cell, ui::LXXKnob& a, ui::LXXKnob& b,
                                 juce::Component* extra) {
                const int s = juce::jmin(ks, cell.getHeight() - (extra != nullptr ? 24 : 0));
                placeKnob(a, cell.getX(), cell.getY(), s, s); a.setVisible(true);
                placeKnob(b, cell.getX() + s + 4, cell.getY(), s, s); b.setVisible(true);
                if (extra != nullptr) {
                    extra->setBounds(cell.getX(), cell.getY() + s + 2, juce::jmin(96, cell.getWidth()), 20);
                    extra->setVisible(true);
                }
            };
            placePair(row.removeFromLeft(cellW), volLow_, lowTone_, &lowCharacterBox_);
            row.removeFromLeft(Layout::gap);
            placePair(row.removeFromLeft(cellW), volMid_, midTone_, nullptr);
            row.removeFromLeft(Layout::gap);
            placePair(row, volOh_, ohDecay_, nullptr);
        }

        drumPageBox_.setBounds(sound.getX(), sound.getBottom() - 24, 100, 22);
        drumPageBox_.setVisible(true);

        // Generator
        if (grids) {
            ui::LXXKnob* g[] = { &drumsX_, &drumsY_, &gridsChaos_ };
            layoutKnobRow(gen.removeFromTop(ks + 8), g, 3, ks);
        }
        if (mapOk && seq == 0) {
            ui::LXXKnob* g[] = { &drumsX_, &drumsY_ };
            layoutKnobRow(gen.removeFromTop(ks + 8), g, 2, ks);
        }
        if (macrosOk) {
            ui::LXXKnob* g0[] = { &technoDensity_, &technoVariation_, &technoSyncopation_ };
            layoutKnobRow(gen.removeFromTop(ks + 8), g0, 3, ks);
            ui::LXXKnob* g1[] = { &technoFill_, &technoSwing_, &technoSeed_ };
            layoutKnobRow(gen.removeFromTop(ks + 8), g1, 3, ks);
        }

        generateBtn_.setBounds(gen.getX(), gen.getBottom() - 28, 90, 26); generateBtn_.setVisible(true);
        mutateBtn_.setBounds(gen.getX() + 96, gen.getBottom() - 28, 90, 26); mutateBtn_.setVisible(true);
        randomizeKitBtn_.setBounds(gen.getX() + 192, gen.getBottom() - 28, 90, 26); randomizeKitBtn_.setVisible(true);

        // Transport
        placeKnob(tempo_, transport.getX(), transport.getY(), ks, ks); tempo_.setVisible(true);
        placeKnob(swing_, transport.getX() + ks + Layout::gap, transport.getY(), ks, ks); swing_.setVisible(true);
        hostSyncToggle_.setBounds(transport.getX() + 2 * (ks + Layout::gap), transport.getY() + 18, 100, 24);
        clockDivisionBox_.setBounds(transport.getX() + 2 * (ks + Layout::gap) + 108, transport.getY() + 18, 90, 24);
        midiOutToggle_.setBounds(transport.getX() + 2 * (ks + Layout::gap), transport.getY() + 48, 100, 24);

        // Global output: BITCRUSH + MASTER
        {
            const int ms = juce::jmin(Layout::knobLarge, global.getHeight() - 4);
            placeKnob(drumsBandwidth_, global.getX() + 8, global.getY() + (global.getHeight() - ks) / 2, ks, ks);
            drumsBandwidth_.setVisible(true);
            placeKnob(volume_, global.getRight() - ms - 12, global.getY() + (global.getHeight() - ms) / 2, ms, ms);
            volume_.setVisible(true);
        }

        // Compact Main locks (same APVTS as Advanced/CLI)
        {
            locksLabel_.setBounds(locks.removeFromLeft(44));
            locksLabel_.setVisible(true);
            lockKick_.setButtonText("KICK");
            lockSnare_.setButtonText("SNARE");
            lockCh_.setButtonText("CH");
            lockLow_.setButtonText("LOW");
            lockMid_.setButtonText("MID");
            lockOh_.setButtonText("OH");
            const int lw = juce::jmax(48, (locks.getWidth() - 5 * 4) / 6);
            for (auto* b : { &lockKick_, &lockSnare_, &lockCh_, &lockLow_, &lockMid_, &lockOh_ }) {
                b->setBounds(locks.removeFromLeft(lw));
                locks.removeFromLeft(4);
                b->setVisible(true);
            }
        }

        if (!hybrid && !designSurface_.stepArea.isEmpty())
            seqGrid_.setBounds(designSurface_.stepArea.reduced(Layout::gap, 8));
    }

    tempo_.setControlEnabled(!hostSyncToggle_.getToggleState());
    clockDivisionBox_.setEnabled(hostSyncToggle_.getToggleState());
    designSurface_.repaint();
}

void LXXAudioProcessorEditor::timerCallback()
{
    const int step = static_cast<int>(processor_.sequencerGrooveStep() % 16);
    seqGrid_.setActiveStep(processor_.sequencerRunning() ? step : -1);
    enforceSeqOnlyMidi();
    runToggle_.setButtonText(runToggle_.getToggleState() ? "RUN ON" : "RUN OFF");
    refreshVisibility();
}

}  // namespace lxx
