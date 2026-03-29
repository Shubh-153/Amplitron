#include "audio/effects/overdrive.h"

namespace GuitarAmp {

Overdrive::Overdrive() {
    params_ = {
        {"Drive",  1.5f,  1.0f, 10.0f, 1.5f, "x", "Amount of input gain pushing into soft-clipping. Increases harmonic saturation and sustain."},
        {"Tone",   0.7f,  0.0f,  1.0f, 0.7f, "", "Adjusts high-frequency content. Lower values are darker and smoother, higher values are brighter and more biting."},
        {"Level",  0.7f,  0.0f,  1.0f, 0.7f, "", "Master output volume of the pedal to compensate for the gain added by the Drive control."},
    };
}

void Overdrive::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    float drive = params_[0].value;
    float tone = params_[1].value;
    float level = params_[2].value;

    float lp_coeff = 0.05f + tone * 0.9f;
    float hp_coeff = 0.001f;

    for (int i = 0; i < num_samples; ++i) {
        float dry = buffer[i];

        // Asymmetric soft clipping (tube-like)
        float x = buffer[i] * drive;
        if (x > 0.0f) {
            x = 1.0f - std::exp(-x);
        } else {
            x = -1.0f + std::exp(x);
            x *= 0.8f; // asymmetry
        }

        // Tone: LP filter
        lp_state_ += lp_coeff * (x - lp_state_);
        x = lp_state_;

        // DC blocking HP filter
        hp_state_ += hp_coeff * (x - hp_state_);
        x -= hp_state_;

        x *= level;
        buffer[i] = dry * (1.0f - mix_) + x * mix_;
    }
}

void Overdrive::reset() {
    lp_state_ = 0.0f;
    hp_state_ = 0.0f;
}

} // namespace GuitarAmp
