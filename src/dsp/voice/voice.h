#pragma once

#include "patch.h"
#include "glide.h"
#include "note_stack.h"
#include "../oscillators/blep_oscillator.h"
#include "../oscillators/sub_oscillator.h"
#include "../oscillators/vco_model.h"
#include "../filters/oversampled_filter.h"
#include "../amplifiers/vca_as2164.h"
#include "../envelopes/adsr_envelope.h"
#include "../lfo/lfo.h"
#include "../modulation/cv_scaler.h"
#include <array>
#include <cstdint>
#include <vector>

namespace lxx {
namespace dsp {

/**
 * Monophonic Anushri voice: MIDI → pitch → VCO (+sub) → VCF → VCA.
 *
 * Note priority: LIFO stack (upstream anu/note_stack.h / voice_controller.cc).
 * Legato: per env_legato_mode (voice.cc NoteOn).
 * Unison: 1–4 detuned oscillator pairs summed before the shared filter.
 */
class Voice {
public:
    static constexpr int kMaxUnison = 4;

    void init(Sample sampleRate, size_t maxBlock = 2048);
    void reset();

    void setPatch(const VoicePatch& patch);
    const VoicePatch& getPatch() const { return patch_; }

    void noteOn(uint8_t note, uint8_t velocity, uint8_t slide = 0, uint8_t accent = 0);
    void noteOff(uint8_t note);
    void allSoundOff();
    void allNotesOff();

    void setPitchBend(Sample semitones);
    void setModWheel(Sample value01);
    void setAftertouch(Sample value01);

    bool isActive() const { return vcaEnv_.isActive(); }

    void processBlock(Sample* output, size_t numFrames);

    Sample getLastPeak() const { return lastPeak_; }
    Sample getFilterPeak() const { return filterPeak_; }

    Sample getFilterIc1() const { return filter_.getFilter().getBPState(); }
    Sample getFilterIc2() const { return filter_.getFilter().getLPState(); }
    Sample getFilterEffectiveSampleRate() const { return filter_.getEffectiveSampleRate(); }
    bool internalStateFinite() const;

    bool isIdle() const { return noteStack_.empty() && !vcaEnv_.isActive(); }
    uint8_t heldNotes() const { return noteStack_.size(); }
    EnvSegment getAmpSegment() const { return vcaEnv_.getSegment(); }
    Sample getAmpEnvelope() const { return vcaEnv_.getValue(); }
    Sample getVcaGain() const { return vca_.getGain(); }
    Sample lastOscillatorFrequencyHz() const { return lastOscFreqHz_; }
    Sample lastCutoffFrequencyHz() const { return lastCutoffHz_; }
    uint8_t lastMidiNote() const { return lastMidiNote_; }

private:
    struct UnisonOsc {
        BlePOscillator osc;
        SubOscillator sub;
        Sample detuneCents = 0.0f;
    };

    void updateEnvelopeParameters();
    void triggerNote(uint8_t note, uint8_t velocity, bool legato);
    void gateOn();
    void gateOff();

    int16_t noteToPitch(uint8_t note) const;
    int16_t computeDcoPitch(int16_t glidePitch, int16_t vibratoLfo) const;
    Sample pitchToFrequency(int16_t dcoPitch, Sample modEnv,
                            int16_t lfoSigned, Sample detuneCents) const;
    Sample computeCutoffHz(int16_t dcoPitch, Sample vcfEnv, int16_t lfo,
                           int16_t vibratoLfo, Sample velocity01) const;
    Sample computePulseWidth(Sample modEnv, uint16_t lfoUnsigned) const;
    void updateOscillatorFrequencies(int16_t dcoPitch, Sample modEnv, int16_t lfoSigned);

    Sample sampleRate_ = kSampleRate;
    VoicePatch patch_{};

    NoteStack<16> noteStack_;
    PitchGlide glide_;

    std::array<UnisonOsc, kMaxUnison> unison_{};
    VcoModel vcoModel_;
    OversampledSVF filter_;
    VcaAs2164 vca_;

    AdsrEnvelope vcfEnv_;
    VcaEnvelope vcaEnv_;
    ModEnvelope modEnv_;

    Lfo lfo_;
    Lfo vibratoLfo_;

    VcoCvScaler vcoScaler_;

    int16_t dcoPitch_ = 0;
    Sample pitchBendSemitones_ = 0.0f;
    Sample modWheel_ = 0.0f;
    Sample modWheel2_ = 0.0f;
    Sample aftertouch_ = 0.0f;
    Sample currentVelocity_ = 1.0f;
    uint8_t currentAccent_ = 0;

    std::vector<Sample> oscScratch_;
    std::vector<Sample> vcaScratch_;
    std::vector<Sample> lpScratch_;
    std::vector<Sample> bpScratch_;
    std::vector<Sample> hpScratch_;

    Sample lastPeak_ = 0.0f;
    Sample filterPeak_ = 0.0f;
    Sample lastOscFreqHz_ = 0.0f;
    Sample lastCutoffHz_ = 0.0f;
    uint8_t lastMidiNote_ = 0;
    bool pitchSourceValid_ = false;
    size_t maxBlock_ = 0;

    static constexpr Sample kOscDrive = 0.32f;
    static constexpr Sample kFilterInputGain = 0.55f;
    static constexpr Sample kSelfOscAtten = 0.45f;
    static constexpr Sample kMasterTrim = 0.75f;
};

} // namespace dsp
} // namespace lxx
