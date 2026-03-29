#include "audio/effects/noise_gate.h"

namespace GuitarAmp {

NoiseGate::NoiseGate() {
    params_ = {
        {"Threshold", -55.0f, -80.0f, 0.0f, -55.0f, "dB", "Signal level below which the gate closes and mutes the audio. Set just above background noise level."},
        {"Attack",     0.5f,   0.1f,  10.0f, 0.5f,  "ms", "How quickly the gate opens when the signal exceeds the threshold. Fast attack preserves pick transients."},
        {"Release",   50.0f,   5.0f, 500.0f, 50.0f,  "ms", "How quickly the gate closes after the signal falls below threshold. Longer release preserves sustained notes."},
    };
    update_coefficients();
}

void NoiseGate::update_coefficients() {
    float attack_ms = params_[1].value;
    float release_ms = params_[2].value;
    attack_coeff_ = std::exp(-1.0f / (sample_rate_ * attack_ms * 0.001f));
    release_coeff_ = std::exp(-1.0f / (sample_rate_ * release_ms * 0.001f));
}

void NoiseGate::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    float threshold = db_to_linear(params_[0].value);
    update_coefficients();

    for (int i = 0; i < num_samples; ++i) {
        float abs_sample = std::fabs(buffer[i]);
        float coeff = (abs_sample > envelope_) ? attack_coeff_ : release_coeff_;
        envelope_ = coeff * envelope_ + (1.0f - coeff) * abs_sample;

        float target_gain = (envelope_ > threshold) ? 1.0f : 0.0f;
        gain_ += (target_gain - gain_) * 0.01f;
        buffer[i] *= gain_;
    }
}

void NoiseGate::reset() {
    envelope_ = 0.0f;
    gain_ = 0.0f;
}

} // namespace GuitarAmp
