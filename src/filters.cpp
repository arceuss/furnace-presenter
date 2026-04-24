#include "filters.h"
#include <cmath>

namespace fp {

HighPassIIR::HighPassIIR(float sample_rate, float cutoff_frequency) {
    float period = 1.0f / sample_rate;
    float tc = 1.0f / cutoff_frequency;
    alpha = tc / (tc + period);
    delta = 0.0f;
    prev_in = 0.0f;
    prev_out = 0.0f;
}

void HighPassIIR::consume(float input) {
    prev_out = output();
    delta = input - prev_in;
    prev_in = input;
}

float HighPassIIR::output() const {
    return alpha * prev_out + alpha * delta;
}

} // namespace fp
