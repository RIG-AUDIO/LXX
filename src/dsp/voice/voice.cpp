#include "voice.h"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <vector>

namespace lxx {
namespace dsp {

namespace {

// Firmware U8U8Mul(note, 128) — pitch in 1/128 semitone units
int16_t noteToPitchUnits(uint8_t note) {
    return static_cast<int16_t>(note) * 128;
}

Sample centsToRatio(Sample cents) {
    return std::pow(2.0f, cents / 1200.0f);
}

// Firmware S8U8MulShift8 style: (signed8 * unsigned8) >> 8
Sample mulS8U8Shift8(int8_t a, uint8_t b) {
    return static_cast<Sample>(a) * static_cast<Sample>(b) / 256.0f;
}

Sample mulU16U8Shift8(Sample a, Sample b) {
    return a * b / 256.0f;
}

} // namespace

void Voice::init(Sample sampleRate, size_t maxBlock) {
    sampleRate_ = (sampleRate > 1.0f && std::isfinite(sampleRate)) ? sampleRate : kSampleRate;
    maxBlock_ = maxBlock > 0 ? maxBlock : 2048;
    glide_.init(sampleRate_);
    noteStack_.init();

    for (auto& u : unison_) {
        u.osc.setSampleRate(sampleRate_);
        u.sub.setSampleRate(sampleRate_);
    }

    filter_.setSampleRate(sampleRate_);
    filter_.prepareBuffers(maxBlock_);
    filter_.setAuthenticMode(true);
    filter_.setUseAnuResonance(true);

    vcfEnv_.init(sampleRate_);
    vcaEnv_.init(sampleRate_);
    modEnv_.init(sampleRate_);
    lfo_.init(sampleRate_);
    vibratoLfo_.init(sampleRate_);

    oscScratch_.assign(maxBlock_, 0.0f);
    vcaScratch_.assign(maxBlock_, 0.0f);
    lpScratch_.assign(maxBlock_, 0.0f);
    bpScratch_.assign(maxBlock_, 0.0f);
    hpScratch_.assign(maxBlock_, 0.0f);

    reset();
}

void Voice::reset() {
    noteStack_.clear();
    glide_.snapTo(0);
    dcoPitch_ = 0;
    lastOscFreqHz_ = 0.0f;
    lastMidiNote_ = 0;
    pitchSourceValid_ = false;
    pitchBendSemitones_ = 0.0f;
    modWheel_ = 0.0f;
    modWheel2_ = 0.0f;
    aftertouch_ = 0.0f;
    currentVelocity_ = 1.0f;
    currentAccent_ = 0;
    lastPeak_ = 0.0f;
    filterPeak_ = 0.0f;

    for (auto& u : unison_) {
        u.osc.reset();
        u.sub.reset();
    }
    filter_.reset();
    vcaEnv_.kill();
    vcfEnv_.kill();
    modEnv_.kill();
    lfo_.reset();
    vibratoLfo_.reset();
}

void Voice::setPatch(const VoicePatch& patch) {
    if (patch_ == patch)
        return;

    const bool envChanged =
        patch.envAttack != patch_.envAttack
        || patch.envDecay != patch_.envDecay
        || patch.envSustain != patch_.envSustain
        || patch.envRelease != patch_.envRelease
        || patch.envVcaMorph != patch_.envVcaMorph;
    const bool lfoChanged =
        patch.lfoShape != patch_.lfoShape
        || patch.lfoRateHz != patch_.lfoRateHz
        || patch.vibratoRateHz != patch_.vibratoRateHz;
    const bool modeChanged = patch.authenticMode != patch_.authenticMode
        || patch.filterMode != patch_.filterMode;
    const bool glideChanged = patch.glideTimeSec != patch_.glideTimeSec;

    patch_ = patch;

    if (envChanged)
        updateEnvelopeParameters();
    if (lfoChanged) {
        lfo_.setShape(patch_.lfoShape);
        lfo_.setFrequency(patch_.lfoRateHz);
        vibratoLfo_.setFrequency(patch_.vibratoRateHz);
    }
    if (modeChanged) {
        filter_.setAuthenticMode(patch_.authenticMode);
        filter_.getFilter().setFilterMode(patch_.filterMode);
        vca_.setSoftClipEnabled(!patch_.authenticMode);
    }
    if (glideChanged)
        glide_.setGlideTime(patch_.glideTimeSec);
}

void Voice::updateEnvelopeParameters() {
    vcfEnv_.setTimes(patch_.envAttack, patch_.envDecay, patch_.envSustain, patch_.envRelease);

    modEnv_.setTimes(patch_.envAttack, patch_.envDecay);

    vcaEnv_.setMorph(patch_.envVcaMorph);
    vcaEnv_.setBaseTimes(patch_.envAttack, patch_.envDecay, patch_.envSustain, patch_.envRelease);
}

int16_t Voice::noteToPitch(uint8_t note) const {
    return noteToPitchUnits(note);
}

void Voice::gateOn() {
    vcaEnv_.noteOn();
    vcfEnv_.noteOn();
    modEnv_.noteOn();
}

void Voice::gateOff() {
    vcaEnv_.noteOff();
    vcfEnv_.noteOff();
    modEnv_.noteOff();
}

void Voice::triggerNote(uint8_t note, uint8_t velocity, bool legato) {
    // voice.cc NoteOn: if (!legato || !patch_.env_legato_mode) GateOn();
    if (!legato || !patch_.envLegatoMode) {
        gateOn();
    }

    const int16_t target = noteToPitch(note);
    lastMidiNote_ = note;

    // Firmware: if (!pitch_source_ || (!legato && env_legato_mode)) snap.
    // Do not treat MIDI 0 (pitch units 0) as "no source" after a real note.
    if (!pitchSourceValid_ || patch_.glideTimeSec <= 1.0e-4f
        || (!legato && patch_.envLegatoMode)) {
        glide_.snapTo(target);
    } else {
        glide_.glideTo(target);
    }
    pitchSourceValid_ = true;

    Sample vel = static_cast<Sample>(velocity + currentAccent_) / 127.0f;
    currentVelocity_ = clamp(vel, 0.0f, 1.0f);
}

void Voice::noteOn(uint8_t note, uint8_t velocity, uint8_t slide, uint8_t accent) {
    if (velocity == 0) {
        noteOff(note);
        return;
    }

    currentAccent_ = accent;

    noteStack_.noteOn(note, velocity);
    const bool legato = noteStack_.size() != 1;

    // Apply slide time offset (voice.cc: slide_time = kbd_glide + slide)
    Sample glideTime = patch_.glideTimeSec;
    if (slide > 0) {
        glideTime = clamp(glideTime + static_cast<Sample>(slide) / 255.0f * kGlideMaxTime,
                          0.0f, kGlideMaxTime);
    }
    glide_.setGlideTime(glideTime);

    triggerNote(note, velocity, legato);
}

void Voice::noteOff(uint8_t note) {
    // voice_controller.cc: LIFO retrigger on top-note release
    if (noteStack_.empty()) {
        return;
    }

    const uint8_t topNote = noteStack_.mostRecentNote();
    noteStack_.noteOff(note);

    if (noteStack_.empty()) {
        gateOff();
        return;
    }

    if (topNote == note) {
        triggerNote(noteStack_.mostRecentNote(),
                    noteStack_.mostRecentVelocity(),
                    true);
    }
}

void Voice::allSoundOff() {
    noteStack_.clear();
    pitchSourceValid_ = false;
    vcaEnv_.kill();
    vcfEnv_.kill();
    modEnv_.kill();
    glide_.snapTo(0);
}

void Voice::allNotesOff() {
    noteStack_.clear();
    pitchSourceValid_ = false;
    gateOff();
}

void Voice::setPitchBend(Sample semitones) {
    pitchBendSemitones_ = semitones;
}

void Voice::setModWheel(Sample value01) {
    modWheel_ = clamp(value01, 0.0f, 1.0f);
}

void Voice::setAftertouch(Sample value01) {
    aftertouch_ = clamp(value01, 0.0f, 1.0f);
}

int16_t Voice::computeDcoPitch(int16_t glidePitch, int16_t vibratoLfo) const {
    int32_t p = glidePitch;
    p += static_cast<int32_t>(pitchBendSemitones_ * 128.0f);
    p += static_cast<int32_t>(patch_.oscRange * 6.0f * 256.0f);
    p += static_cast<int32_t>(patch_.oscDcoFine * 128.0f / 50.0f);

    Sample modWheelPitch = 0.0f;
    if (patch_.vibratoDest < 0.5f) {
        modWheelPitch = modWheel_;
    } else {
        modWheelPitch = modWheel_ * (1.0f - patch_.vibratoDest) * 2.0f;
    }
    p += static_cast<int32_t>(mulS8U8Shift8(static_cast<int8_t>(vibratoLfo >> 8),
                                            static_cast<uint8_t>(modWheelPitch * 255.0f)));
    return static_cast<int16_t>(p);
}

Sample Voice::pitchToFrequency(int16_t dcoPitch, Sample modEnv,
                                int16_t lfoSigned, Sample detuneCents) const {
    int32_t p = dcoPitch;
    p += static_cast<int32_t>(patch_.oscDetune * 128.0f / 100.0f);
    p += static_cast<int32_t>(patch_.oscFine * 128.0f / 50.0f);
    p += static_cast<int32_t>(mulU16U8Shift8(modEnv, patch_.pitchEnvAmt * 255.0f) * 16.0f);
    p += static_cast<int32_t>((static_cast<Sample>(lfoSigned) / 256.0f)
                              * patch_.pitchLfoAmt * 16.0f);
    p += static_cast<int32_t>(detuneCents * 128.0f / 100.0f);
    return vcoModel_.pitchToFreq(static_cast<int16_t>(p));
}

Sample Voice::computeCutoffHz(int16_t dcoPitch, Sample vcfEnv, int16_t lfo,
                               int16_t vibratoLfo, Sample velocity01) const {
    // Panel cutoff is musical Hz (log-scaled parameter). Analog Anushri
    // tracking is 0.5 V/oct at firmware tracking=128; env/LFO are unipolar /
    // bipolar offsets in octaves. Do not round-trip Hz through the inverted
    // DAC shortcut — that made the LPF feel backwards.
    const Sample baseHz = clamp(patch_.filterCutoffHz, 20.0f, 20000.0f);
    const Sample noteSemis = (static_cast<Sample>(dcoPitch) - 60.0f * 128.0f) / 128.0f;
    const Sample trackOct = noteSemis / 12.0f * patch_.filterKbdTrack * 0.5f;

    Sample envAmt = patch_.filterEnvAmt + static_cast<Sample>(currentAccent_) * 0.5f / 255.0f;
    envAmt += velocity01 * patch_.velToFilter;
    envAmt = clamp(envAmt, 0.0f, 1.0f);
    const Sample envOct = clamp(vcfEnv, 0.0f, 1.0f) * envAmt * 4.0f;

    const Sample lfoOct = (static_cast<Sample>(lfo) / 32768.0f) * patch_.filterLfoAmt * 2.0f;

    Sample growl = modWheel_;
    if (patch_.vibratoDest >= 0.5f)
        growl = modWheel_ * patch_.vibratoDest * 2.0f;
    growl = clamp(growl + modWheel2_ + patch_.filterModAmt, 0.0f, 1.0f);
    const Sample growlOct = (static_cast<Sample>(vibratoLfo) / 32768.0f) * growl * 0.5f;

    const Sample hz = baseHz * std::pow(2.0f, trackOct + envOct + lfoOct + growlOct);
    return clamp(hz, 20.0f, 20000.0f);
}

Sample Voice::computePulseWidth(Sample modEnv, uint16_t lfoUnsigned) const {
    Sample pw = 0.0f;
    pw += mulU16U8Shift8(modEnv, patch_.pwEnvAmt * 255.0f) * 0.25f;
    pw += mulU16U8Shift8(static_cast<Sample>(lfoUnsigned), patch_.pwLfoAmt * 255.0f) * 0.25f;
    pw = pw * 0.25f;
    return clamp(0.5f + pw / 4096.0f, 0.05f, 0.95f);
}

void Voice::updateOscillatorFrequencies(int16_t dcoPitch, Sample modEnv, int16_t lfoSigned) {
    const int count = std::clamp(patch_.unisonCount, 1, kMaxUnison);
    for (int i = 0; i < kMaxUnison; ++i) {
        if (i >= count) {
            continue;
        }
        const Sample detune = unison_[static_cast<size_t>(i)].detuneCents;
        const Sample freq = pitchToFrequency(dcoPitch, modEnv, lfoSigned, detune);
        lastOscFreqHz_ = freq;
        unison_[static_cast<size_t>(i)].osc.setFrequency(freq);
        // Analog sub-oscillator is a 1-octave divider.
        unison_[static_cast<size_t>(i)].sub.setFrequency(freq * 0.5f);
    }
}

void Voice::processBlock(Sample* output, size_t numFrames) {
    if (output == nullptr || numFrames == 0)
        return;

    if (oscScratch_.empty() || maxBlock_ == 0) {
        std::fill(output, output + numFrames, 0.0f);
        return;
    }

    if (numFrames > maxBlock_) {
        size_t offset = 0;
        while (offset < numFrames) {
            const size_t chunk = std::min(numFrames - offset, maxBlock_);
            if (chunk == 0)
                break;
            processBlock(output + offset, chunk);
            offset += chunk;
        }
        return;
    }

    // Firmware: closed VCA (envelope DEAD, no held keys) is digital silence.
    // Oscillators still free-run in analog hardware, but vca_cv==0 mutes output.
    if (isIdle()) {
        std::fill(output, output + numFrames, 0.0f);
        vca_.setFromFirmware(0.0f, currentVelocity_, patch_.velToAmp, patch_.volume * 255.0f);
        return;
    }

    std::fill(oscScratch_.begin(), oscScratch_.begin() + static_cast<std::ptrdiff_t>(numFrames), 0.0f);

    constexpr size_t kSubBlock = 32;
    const int count = std::clamp(patch_.unisonCount, 1, kMaxUnison);
    const Sample spread = patch_.unisonDetuneCents;
    for (int i = 0; i < count; ++i) {
        Sample t = (count == 1) ? 0.0f
                  : (static_cast<Sample>(i) / static_cast<Sample>(count - 1)) * 2.0f - 1.0f;
        unison_[static_cast<size_t>(i)].detuneCents = t * spread;
    }

    const Waveform wf = patch_.authenticMode ? Waveform::TRIANGLE : Waveform::SAWTOOTH;

    for (size_t offset = 0; offset < numFrames; offset += kSubBlock) {
        const size_t block = std::min(kSubBlock, numFrames - offset);
        Sample blockCutoff = patch_.filterCutoffHz;

        for (size_t i = 0; i < block; ++i) {
            const size_t idx = offset + i;
            const int16_t glidePitch = glide_.process();
            const int16_t vibratoLfo = vibratoLfo_.processBipolar();
            dcoPitch_ = computeDcoPitch(glidePitch, vibratoLfo);

            const uint16_t lfoUnsigned = lfo_.process();
            const int16_t lfoSigned = static_cast<int16_t>(lfoUnsigned) - 32768;
            const Sample modEnvVal = modEnv_.process();
            const Sample vcfEnvVal = vcfEnv_.process();
            vcaScratch_[idx] = vcaEnv_.process();

            updateOscillatorFrequencies(dcoPitch_, modEnvVal, lfoSigned);

            const Sample pw = computePulseWidth(modEnvVal, lfoUnsigned);

            Sample oscSum = 0.0f;
            for (int u = 0; u < count; ++u) {
                auto& voice = unison_[static_cast<size_t>(u)];
                voice.osc.setWaveform(wf);
                if (!patch_.authenticMode) {
                    voice.osc.setPulseWidth(pw);
                }
                oscSum += voice.osc.process() * patch_.waveLevel;
                oscSum += voice.sub.process() * patch_.subLevel;
            }
            oscScratch_[idx] = oscSum * kOscDrive / static_cast<Sample>(count) * kFilterInputGain;

            blockCutoff = computeCutoffHz(dcoPitch_, vcfEnvVal, lfoSigned,
                                          vibratoLfo, currentVelocity_);
        }

        lastCutoffHz_ = blockCutoff;
        filter_.setCutoffFreq(clamp(blockCutoff, 20.0f, 20000.0f));
        filter_.setResonance(clamp(patch_.filterResonance, 0.0f, 1.0f));
        filter_.processBlock(oscScratch_.data() + offset,
                             lpScratch_.data() + offset,
                             bpScratch_.data() + offset,
                             hpScratch_.data() + offset,
                             block);

        for (size_t i = 0; i < block; ++i) {
            const size_t idx = offset + i;
            Sample filtered = 0.0f;
            switch (patch_.filterMode) {
                case SVFOutput::LOWPASS:  filtered = lpScratch_[idx]; break;
                case SVFOutput::BANDPASS: filtered = bpScratch_[idx]; break;
                case SVFOutput::HIGHPASS: filtered = hpScratch_[idx]; break;
            }

            filterPeak_ = std::max(filterPeak_, std::abs(filtered));

            if (patch_.filterResonance > 0.92f && std::abs(oscScratch_[idx]) < 1e-5f) {
                filtered *= kSelfOscAtten;
            }

            vca_.setFromFirmware(vcaScratch_[idx],
                                 currentVelocity_,
                                 patch_.velToAmp,
                                 patch_.volume * 255.0f);

            Sample out = 0.0f;
            if (vca_.getGain() != 0.0f)
                out = vca_.processSample(filtered) * kMasterTrim;
            output[idx] = out;
            lastPeak_ = std::max(lastPeak_, std::abs(out));
        }
    }
}

bool Voice::internalStateFinite() const {
    const Sample ic1 = filter_.getFilter().getBPState();
    const Sample ic2 = filter_.getFilter().getLPState();
    const Sample hp = filter_.getFilter().getHPState();
    return std::isfinite(ic1) && std::isfinite(ic2) && std::isfinite(hp)
        && std::isfinite(lastPeak_) && std::isfinite(filterPeak_);
}

} // namespace dsp
} // namespace lxx
