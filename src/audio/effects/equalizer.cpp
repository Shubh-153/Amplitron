#include "audio/effects/equalizer.h"

namespace GuitarAmp {

Equalizer::Equalizer() {
    params_ = {
        {"Bass",    0.0f, -12.0f, 12.0f, 0.0f, "dB", "Low shelf filter (200Hz). Boosts or cuts the heavy low-end thud and body of the tone."},
        {"Mid",     0.0f, -12.0f, 12.0f, 0.0f, "dB", "Bell filter (800Hz). Boosts for leads that cut through a mix, cuts for a hollow metal rhythm tone."},
        {"Treble",  0.0f, -12.0f, 12.0f, 0.0f, "dB", "High shelf filter (3kHz). Adjusts the bite, pick attack, and overall brightness."},
        {"Presence", 0.0f, -12.0f, 12.0f, 0.0f, "dB", "Ultra-high frequency contour. Adds 'air' and glassiness to the very top end."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void Equalizer::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    // Force recomputation on next process() call
    cached_bass_ = -999.0f;
    cached_mid_ = -999.0f;
    cached_treble_ = -999.0f;
    cached_presence_ = -999.0f;
    recompute_coefficients_if_dirty();
}

void Equalizer::compute_low_shelf(float freq, float gain_db, float q) {
    float A = std::pow(10.0f, gain_db / 40.0f);
    float w0 = TWO_PI * freq / sample_rate_;
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * q);
    float sqA = std::sqrt(A);

    float a0 = (A + 1) + (A - 1) * cos_w0 + 2 * sqA * alpha;
    low_shelf_.b0 = (A * ((A + 1) - (A - 1) * cos_w0 + 2 * sqA * alpha)) / a0;
    low_shelf_.b1 = (2 * A * ((A - 1) - (A + 1) * cos_w0)) / a0;
    low_shelf_.b2 = (A * ((A + 1) - (A - 1) * cos_w0 - 2 * sqA * alpha)) / a0;
    low_shelf_.a1 = (-2 * ((A - 1) + (A + 1) * cos_w0)) / a0;
    low_shelf_.a2 = ((A + 1) + (A - 1) * cos_w0 - 2 * sqA * alpha) / a0;
}

void Equalizer::compute_peaking(float freq, float gain_db, float q) {
    float A = std::pow(10.0f, gain_db / 40.0f);
    float w0 = TWO_PI * freq / sample_rate_;
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * q);

    float a0 = 1 + alpha / A;
    mid_peak_.b0 = (1 + alpha * A) / a0;
    mid_peak_.b1 = (-2 * cos_w0) / a0;
    mid_peak_.b2 = (1 - alpha * A) / a0;
    mid_peak_.a1 = (-2 * cos_w0) / a0;
    mid_peak_.a2 = (1 - alpha / A) / a0;
}

void Equalizer::compute_high_shelf(float freq, float gain_db, float q) {
    float A = std::pow(10.0f, gain_db / 40.0f);
    float w0 = TWO_PI * freq / sample_rate_;
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * q);
    float sqA = std::sqrt(A);

    float a0 = (A + 1) - (A - 1) * cos_w0 + 2 * sqA * alpha;
    high_shelf_.b0 = (A * ((A + 1) + (A - 1) * cos_w0 + 2 * sqA * alpha)) / a0;
    high_shelf_.b1 = (-2 * A * ((A - 1) + (A + 1) * cos_w0)) / a0;
    high_shelf_.b2 = (A * ((A + 1) + (A - 1) * cos_w0 - 2 * sqA * alpha)) / a0;
    high_shelf_.a1 = (2 * ((A - 1) - (A + 1) * cos_w0)) / a0;
    high_shelf_.a2 = ((A + 1) - (A - 1) * cos_w0 - 2 * sqA * alpha) / a0;
}

void Equalizer::recompute_coefficients_if_dirty() {
    float bass = params_[0].value;
    float mid = params_[1].value;
    float treble = params_[2].value;
    float presence = params_.size() > 3 ? params_[3].value : 0.0f;

    if (bass != cached_bass_ || mid != cached_mid_ ||
        treble != cached_treble_ || presence != cached_presence_) {
        compute_low_shelf(200.0f, bass, 0.7f);
        compute_peaking(800.0f, mid, 1.0f);
        compute_high_shelf(3000.0f, treble, 0.7f);
        cached_bass_ = bass;
        cached_mid_ = mid;
        cached_treble_ = treble;
        cached_presence_ = presence;
    }
}

void Equalizer::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    // Only recompute biquad coefficients when parameters actually changed
    recompute_coefficients_if_dirty();

    for (int i = 0; i < num_samples; ++i) {
        float x = buffer[i];
        x = low_shelf_.process(x);
        x = mid_peak_.process(x);
        x = high_shelf_.process(x);
        buffer[i] = x;
    }
}

void Equalizer::reset() {
    low_shelf_.reset();
    mid_peak_.reset();
    high_shelf_.reset();
}

} // namespace GuitarAmp
