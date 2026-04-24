#pragma once

#include "config.h"
#include "filters.h"
#include "font.h"
#include <cstdint>
#include <string>
#include <vector>
#include <deque>

namespace fp {

struct ChannelState {
    float volume = 0.0f;
    float amplitude = 0.0f;
    double frequency = 0.0;
    size_t timbre = 0;
    float balance = 0.5f;
    bool edge = false;
};

struct OscilloscopeState {
    std::deque<float> amplitudes;
    std::deque<bool> edges;
    OscilloscopeState() = default;
    void consume(const ChannelState& state);
};

struct SliceState {
    Color color{0,0,0,0};
    float index = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    size_t frame = 0;
};

struct PianoRollState {
    std::deque<SliceState> slices;
    float samples_per_frame = 0.0f;
    float taken_samples = 0.0f;
    float starting_octave = 0.0f;
    std::vector<float> volume_buf;
    size_t frame_count = 0;
    bool pending_edge = false;

    PianoRollState() = default;
    PianoRollState(float sample_rate, float scroll_speed, float starting_octave);
    void consume(const ChannelState& state, const ChannelSettings* settings);
};

class Visualizer {
    size_t channels = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t sample_rate = 0;
    PianoRollConfig config;

    std::vector<ChannelState> channel_last_states;
    std::vector<HighPassIIR> channel_filters;
    std::vector<OscilloscopeState> oscilloscope_states;
    std::vector<PianoRollState> piano_roll_states;
    float piano_view_offset = -1.0f;

    Font font;

    std::vector<uint8_t> canvas;

    void draw_oscilloscopes(int x, int y, int w, int h, size_t max_channels_per_row);
    void draw_oscilloscope_view(size_t channel, int x, int y, int w, int h);
    void update_piano_view(int w);
    float piano_keys_x(int x, int w) const;
    void draw_piano_roll(int x, int y, int w, int h);
    void draw_piano_keys(int x, int y, int w, int h);
    void draw_channel_slices(int x, int y, int w, int h, bool outline);
    void draw_channel_key_spot(size_t channel, int x, int y, int w, int h);

public:
    Visualizer(size_t channels, uint32_t width, uint32_t height, uint32_t sample_rate,
               const PianoRollConfig& config, const std::string& font_path);

    const uint8_t* get_canvas_buffer() const { return canvas.data(); }
    std::vector<uint8_t>& get_canvas() { return canvas; }
    uint32_t canvas_width() const { return width; }
    uint32_t canvas_height() const { return height; }

    void clear();
    void draw();

    void consume_state(size_t channel, const ChannelState& state);
    void set_channel_settings(size_t channel, const ChannelSettings& settings);

    PianoRollConfig& get_config() { return config; }
};

} // namespace fp
