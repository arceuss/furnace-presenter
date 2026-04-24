#pragma once

namespace fp {

class HighPassIIR {
    float alpha;
    float delta;
    float prev_in;
    float prev_out;

public:
    HighPassIIR() = default;
    HighPassIIR(float sample_rate, float cutoff_frequency);
    void consume(float input);
    float output() const;
};

} // namespace fp
