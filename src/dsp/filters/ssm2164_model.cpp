#include "ssm2164_model.h"

namespace lxx {
namespace dsp {

SSM2164Model::SSM2164Model(const SSM2164Calibration& cal) : cal_(cal), circuit_() {
    updateDerived();
}

void SSM2164Model::setCalibration(const SSM2164Calibration& cal) {
    cal_ = cal;
    updateDerived();
}

void SSM2164Model::setTemperature(Sample tempKelvin) {
    cal_.temperature = tempKelvin;
    updateDerived();
}

void SSM2164Model::updateDerived() {
    kt_cutoff_ = cal_.kBoltzmann * cal_.temperature;
    kt_resonance_ = cal_.kBoltzmann * cal_.resonanceTemp;
}

Sample SSM2164Model::cutoffGain(Sample cv) const {
    // VCF cutoff law: gain = exp(-cv / 10.4 / (kT))
    // cv is the voltage at the SSM2164 control pin
    // Positive cv reduces gain (exponential attenuation)
    return std::exp(-cv / (cal_.cutoffDenom * kt_cutoff_));
}

Sample SSM2164Model::resonanceGain(Sample cv) const {
    // 60 mV/decade: gain = exp(-cv / 60.0 / (kT))
    return std::exp(-cv / (cal_.resonanceDenom * kt_resonance_));
}

Sample SSM2164Model::anuResonanceGain(Sample value) const {
    // 60 mV/decade: gain = 0.5 / (0.5 + value^2 * 10)
    return 0.5f / (0.5f + value * value * 10.0f);
}

Sample SSM2164Model::computeCutoffFreq(Sample cv_dac, Sample cv_input, Sample cv_pot) const {
    // VCF cutoff law f_vcf()
    Sample i1 = cv_dac / circuit_.r_dac + cv_input / circuit_.r_input + cv_pot / circuit_.r_pot - 4.096f / circuit_.r_ref;
    Sample v1 = -i1 * circuit_.r_fb;
    
    Sample f_max = 1.0f / (2.0f * kPi * circuit_.r_vcf * circuit_.c_vcf);
    return f_max * cutoffGain(v1);
}

Sample SSM2164Model::getMaxFreq() const {
    return 1.0f / (2.0f * kPi * circuit_.r_vcf * circuit_.c_vcf);
}

void SSM2164Model::setCircuitParams(const CircuitParams& params) {
    circuit_ = params;
}

} // namespace dsp
} // namespace lxx