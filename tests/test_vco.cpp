#include "../src/dsp/oscillators/vco_model.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <fstream>

// Simple CSV writer
void writeCsv(const char* filename, const float* data, size_t numFrames, const char* header = "index,value") {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "%s\n", header);
    for (size_t i = 0; i < numFrames; ++i) {
        fprintf(f, "%zu,%.10f\n", i, data[i]);
    }
    fclose(f);
}

// Write CSV with multiple columns
void writeCsvMulti(const char* filename, const float* col1, const float* col2, size_t numFrames, 
                   const char* h1, const char* h2) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "%s,%s\n", h1, h2);
    for (size_t i = 0; i < numFrames; ++i) {
        fprintf(f, "%.10f,%.10f\n", col1[i], col2[i]);
    }
    fclose(f);
}

int main() {
    printf("Testing VCO Model...\n");
    
    lxx::dsp::VcoModel vco;
    
    // Test 1: Pitch to frequency mapping (matches Python ref) - NO DRIFT
    {
        printf("  Test 1: Pitch sweep...\n");
        vco.setDriftAmount(0.0f); // Disable drift for accuracy test
        const int numPitches = 8193; // 0 to 16384 step 2
        std::vector<float> pitches(numPitches);
        std::vector<float> freqs(numPitches);
        
        for (int i = 0; i < numPitches; ++i) {
            int pitch = i * 2;
            pitches[i] = pitch;
            freqs[i] = vco.pitchToFreq(pitch);
        }
        writeCsv("test_vco_pitch_sweep.csv", freqs.data(), numPitches);
        printf("    Done: %d points\n", numPitches);
    }
    
    // Test 2: MIDI note sweep (0-127)
    {
        printf("  Test 2: MIDI note sweep...\n");
        const int numNotes = 128;
        std::vector<float> notes(numNotes);
        std::vector<float> freqs(numNotes);
        std::vector<float> cents(numNotes);
        
        for (int i = 0; i < numNotes; ++i) {
            notes[i] = i;
            freqs[i] = vco.noteToFreq(i);
            // Compare to ideal equal temperament
            float ideal = 440.0f * std::pow(2.0f, (i - 69.0f) / 12.0f);
            cents[i] = 1200.0f * std::log2(freqs[i] / ideal);
        }
        writeCsvMulti("test_vco_midi_freq.csv", notes.data(), freqs.data(), numNotes, "note", "freq");
        writeCsv("test_vco_midi_cents.csv", cents.data(), numNotes);
        printf("    Done: 128 notes\n");
    }
    
    // Test 3: CV sweep (0-5V)
    {
        printf("  Test 3: CV sweep...\n");
        const int numCV = 501;
        std::vector<float> cvs(numCV);
        std::vector<float> freqs(numCV);
        
        for (int i = 0; i < numCV; ++i) {
            cvs[i] = i * 5.0f / (numCV - 1);
            freqs[i] = vco.cvToFreq(cvs[i]);
        }
        writeCsvMulti("test_vco_cv_sweep.csv", cvs.data(), freqs.data(), numCV, "cv", "freq");
        printf("    Done: %d points\n", numCV);
    }
    
    // Test 4: Temperature sweep at C3
    {
        printf("  Test 4: Temperature sweep...\n");
        // Enable drift for this test
        vco.setDriftAmount(1.0f);
        
        const int numTemps = 51;
        std::vector<float> tempsC(numTemps);
        std::vector<float> freqs(numTemps);
        std::vector<float> drifts(numTemps);
        
        for (int i = 0; i < numTemps; ++i) {
            float tempC = 15.0f + i * 25.0f / (numTemps - 1);
            tempsC[i] = tempC;
            vco.setTemperature(tempC + 273.15f);
            freqs[i] = vco.noteToFreq(60); // C3
            
            // Drift in cents relative to 25°C
            float baseFreq = 261.625f * std::pow(2.0f, (60 - 69) / 12.0f); // ideal C3
            drifts[i] = 1200.0f * std::log2(freqs[i] / baseFreq);
        }
        
        writeCsvMulti("test_vco_temp_sweep.csv", tempsC.data(), freqs.data(), numTemps, "temp_c", "freq");
        writeCsv("test_vco_temp_drift.csv", drifts.data(), numTemps);
        vco.setDriftAmount(0.3f); // Restore default
        vco.setTemperature(298.15f);
        printf("    Done: %d temperatures\n", numTemps);
    }
    
    // Test 5: Drift amount sweep
    {
        printf("  Test 5: Drift amount sweep...\n");
        vco.setTemperature(303.15f); // 30°C
        const int numAmounts = 101;
        std::vector<float> amounts(numAmounts);
        std::vector<float> freqs(numAmounts);
        
        for (int i = 0; i < numAmounts; ++i) {
            amounts[i] = i / 100.0f;
            vco.setDriftAmount(amounts[i]);
            freqs[i] = vco.noteToFreq(60);
        }
        writeCsvMulti("test_vco_drift_amount.csv", amounts.data(), freqs.data(), numAmounts, "amount", "freq");
        vco.setDriftAmount(0.3f);
        vco.setTemperature(298.15f);
        printf("    Done: %d amounts\n", numAmounts);
    }
    
    // Test 6: Pitch accuracy - cents deviation from equal temperament
    {
        printf("  Test 6: Pitch accuracy (cents)...\n");
        vco.setDriftAmount(0.0f); // Disable drift for accuracy test
        const int numPitches = 16385;
        std::vector<float> maxCentsDev;
        float maxDev = 0.0f;
        int maxDevPitch = 0;
        
        for (int pitch = 0; pitch < numPitches; ++pitch) {
            float freq = vco.pitchToFreq(pitch);
            // Ideal frequency for this pitch
            float semitones = (pitch - 7680.0f) / 128.0f;
            float idealFreq = 261.625f * std::pow(2.0f, semitones / 12.0f);
            float cents = 1200.0f * std::log2(freq / idealFreq);
            float absCents = std::abs(cents);
            if (absCents > maxDev) {
                maxDev = absCents;
                maxDevPitch = pitch;
            }
        }
        printf("    Max cents deviation: %.3f cents at pitch %d\n", maxDev, maxDevPitch);
        
        // Check across MIDI range
        for (int note = 0; note < 128; ++note) {
            float freq = vco.noteToFreq(note);
            float ideal = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
            float cents = 1200.0f * std::log2(freq / ideal);
            if (std::abs(cents) > 0.1f) {
                printf("    Note %d: %.3f cents deviation\n", note, cents);
            }
        }
    }
    
    // Test 7: Thermal drift cents at various pitches
    {
        printf("  Test 7: Thermal drift at various pitches...\n");
        vco.setDriftAmount(1.0f);
        vco.setTemperature(303.15f); // 30°C
        
        std::vector<float> pitches;
        std::vector<float> driftCents;
        
        for (int note = 24; note <= 96; note += 12) {
            int pitch = note * 128;
            pitches.push_back(note);
            float drift = vco.getThermalDriftCents(pitch);
            driftCents.push_back(drift);
            printf("    MIDI %d: %.3f cents drift\n", note, drift);
        }
        
        writeCsvMulti("test_vco_drift_by_pitch.csv", pitches.data(), driftCents.data(), 
                      pitches.size(), "midi_note", "drift_cents");
        
        vco.setDriftAmount(0.3f);
        vco.setTemperature(298.15f);
    }
    
    // Test 8: VCO max frequency
    {
        printf("  Test 8: VCO max frequency: %.1f Hz\n", vco.getMaxFreq());
    }
    
    printf("\nAll VCO tests complete. Output files in build directory.\n");
    return 0;
}