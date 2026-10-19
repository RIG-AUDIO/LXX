#!/usr/bin/env python3
"""
Analyze BLEP oscillator output for anti-aliasing quality.
"""

import numpy as np
import csv
import os
import sys

def load_csv(filename, skip_header=True, col=1):
    data = []
    with open(filename, 'r') as f:
        reader = csv.reader(f)
        if skip_header:
            next(reader, None)
        for row in reader:
            if len(row) > col:
                data.append(float(row[col]))
    return np.array(data)

def compute_fft(signal, sample_rate=48000):
    """Compute FFT and return frequencies and magnitudes in dB."""
    n = len(signal)
    # Window
    window = np.hanning(n)
    signal = signal * window
    # FFT
    fft = np.fft.rfft(signal)
    mag = np.abs(fft)
    mag_db = 20 * np.log10(mag + 1e-10)
    freqs = np.fft.rfftfreq(n, 1/sample_rate)
    return freqs, mag_db

def analyze_waveform(name, csv_file, fundamental_freq):
    print(f"\n--- {name} ---")
    signal = load_csv(csv_file)
    freqs, mag_db = compute_fft(signal)
    
    # Find fundamental bin
    fund_bin = np.argmin(np.abs(freqs - fundamental_freq))
    fund_mag = mag_db[fund_bin]
    print(f"  Fundamental ({fundamental_freq}Hz): bin {fund_bin} = {freqs[fund_bin]:.1f}Hz, mag = {fund_mag:.1f} dB")
    
    # Check harmonics
    print(f"  Harmonics:")
    for h in range(2, 10):
        h_freq = fundamental_freq * h
        if h_freq > 24000: break
        h_bin = np.argmin(np.abs(freqs - h_freq))
        h_mag = mag_db[h_bin]
        print(f"    H{h} ({h_freq}Hz): {h_mag:.1f} dB (rel: {h_mag - fund_mag:.1f} dB)")
    
    # Check for aliasing above fundamental
    # Energy in regions where there shouldn't be harmonics
    alias_mask = (freqs > fundamental_freq * 1.5) & (freqs < 24000)
    if np.any(alias_mask):
        max_alias = np.max(mag_db[alias_mask])
        alias_db = max_alias - fund_mag
        print(f"  Max alias above 1.5x fund: {alias_db:.1f} dB below fundamental")
        if alias_db < -60:
            print(f"    EXCELLENT: Aliasing < -60 dB")
        elif alias_db < -40:
            print(f"    GOOD: Aliasing < -40 dB")
        else:
            print(f"    WARNING: Aliasing > -40 dB")
    
    # Check Nyquist region
    nyquist_mask = (freqs > 20000) & (freqs < 24000)
    if np.any(nyquist_mask):
        nyquist_max = np.max(mag_db[nyquist_mask])
        nyquist_db = nyquist_max - fund_mag
        print(f"  Near Nyquist (20-24kHz): {nyquist_db:.1f} dB below fundamental")

def main():
    base = os.environ.get(
        "LXX_BLEP_CSV_DIR",
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "..", "build", "tests"),
    )
    if len(sys.argv) > 1:
        base = sys.argv[1]
    base = os.path.abspath(base)
    
    print("=" * 60)
    print("BLEP Oscillator Anti-Aliasing Analysis")
    print("=" * 60)
    
    # Triangle wave (authentic mode) at 440Hz
    analyze_waveform("Triangle (440Hz)", 
                     os.path.join(base, "test_blep_triangle.csv"), 
                     440.0)
    
    # Sawtooth at 440Hz
    analyze_waveform("Sawtooth (440Hz)", 
                     os.path.join(base, "test_blep_sawtooth.csv"), 
                     440.0)
    
    # Pulse 50% (square) at 440Hz
    analyze_waveform("Pulse 50% (440Hz)", 
                     os.path.join(base, "test_blep_pulse.csv"), 
                     440.0)
    
    # Pulse 25% at 440Hz
    analyze_waveform("Pulse 25% (440Hz)", 
                     os.path.join(base, "test_blep_pulse_25.csv"), 
                     440.0)
    
    # High frequency test (12kHz triangle)
    print("\n--- High Frequency Test (12kHz Triangle) ---")
    signal = load_csv(os.path.join(base, "test_blep_highfreq.csv"))
    freqs, mag_db = compute_fft(signal)
    fund_bin = np.argmin(np.abs(freqs - 12000))
    fund_mag = mag_db[fund_bin]
    print(f"  Fundamental (12kHz): {freqs[fund_bin]:.1f}Hz, mag = {fund_mag:.1f} dB")
    
    # Check for aliases
    alias_mask = (freqs > 18000) & (freqs < 24000)
    if np.any(alias_mask):
        max_alias = np.max(mag_db[alias_mask])
        alias_db = max_alias - fund_mag
        print(f"  Max alias above 18kHz: {alias_db:.1f} dB below fundamental")
        if alias_db < -60:
            print(f"    EXCELLENT: Aliasing < -60 dB")
        elif alias_db < -40:
            print(f"    GOOD: Aliasing < -40 dB")
        else:
            print(f"    WARNING: Aliasing > -40 dB")

if __name__ == '__main__':
    import os
    main()