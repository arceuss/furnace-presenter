#include "visualizer.h"
#include <cmath>
#include <algorithm>

namespace fp {

// Simple canvas helpers
void fill_rect(std::vector<uint8_t>& buf, int buf_w, int buf_h,
                      int x, int y, int w, int h,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int x0 = std::max(x, 0);
    int y0 = std::max(y, 0);
    int x1 = std::min(x + w, buf_w);
    int y1 = std::min(y + h, buf_h);
    if (x0 >= x1 || y0 >= y1) return;

    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            size_t idx = (py * buf_w + px) * 4;
            if (a == 255) {
                buf[idx+0] = r;
                buf[idx+1] = g;
                buf[idx+2] = b;
                buf[idx+3] = a;
            } else {
                float src_a = a / 255.0f;
                float inv_a = 1.0f - src_a;
                buf[idx+0] = (uint8_t)(r * src_a + buf[idx+0] * inv_a);
                buf[idx+1] = (uint8_t)(g * src_a + buf[idx+1] * inv_a);
                buf[idx+2] = (uint8_t)(b * src_a + buf[idx+2] * inv_a);
                buf[idx+3] = (uint8_t)(a * src_a + buf[idx+3] * inv_a);
            }
        }
    }
}

void draw_line(std::vector<uint8_t>& buf, int buf_w, int buf_h,
                      float x0, float y0, float x1, float y1,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int ix0 = (int)std::round(x0);
    int iy0 = (int)std::round(y0);
    int ix1 = (int)std::round(x1);
    int iy1 = (int)std::round(y1);

    int dx = std::abs(ix1 - ix0);
    int dy = std::abs(iy1 - iy0);
    int sx = (ix0 < ix1) ? 1 : -1;
    int sy = (iy0 < iy1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (ix0 >= 0 && ix0 < buf_w && iy0 >= 0 && iy0 < buf_h) {
            size_t idx = (iy0 * buf_w + ix0) * 4;
            if (a == 255) {
                buf[idx+0] = r; buf[idx+1] = g; buf[idx+2] = b; buf[idx+3] = a;
            } else {
                float src_a = a / 255.0f;
                float inv_a = 1.0f - src_a;
                buf[idx+0] = (uint8_t)(r * src_a + buf[idx+0] * inv_a);
                buf[idx+1] = (uint8_t)(g * src_a + buf[idx+1] * inv_a);
                buf[idx+2] = (uint8_t)(b * src_a + buf[idx+2] * inv_a);
                buf[idx+3] = (uint8_t)(a * src_a + buf[idx+3] * inv_a);
            }
        }
        if (ix0 == ix1 && iy0 == iy1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; ix0 += sx; }
        if (e2 < dx) { err += dx; iy0 += sy; }
    }
}

// OscilloscopeState
void OscilloscopeState::consume(const ChannelState& state) {
    amplitudes.push_back(state.amplitude);
    edges.push_back(state.edge);
    if (amplitudes.size() > 4096) {
        amplitudes.pop_front();
        edges.pop_front();
    }
}

// PianoRollState
PianoRollState::PianoRollState(float sample_rate, float scroll_speed, float starting_octave_)
    : samples_per_frame(sample_rate / (60.0f * scroll_speed)),
      taken_samples(0.0f),
      starting_octave(starting_octave_) {}

static const float C_0 = 16.351597831287f;

void PianoRollState::consume(const ChannelState& state, const ChannelSettings* settings) {
    float sample_volume = (state.frequency > 0.0 && state.volume > 0.0f) ? state.volume : 0.0f;
    volume_buf.push_back(sample_volume);

    taken_samples += 1.0f;
    if (state.edge) pending_edge = true;
    if (taken_samples < samples_per_frame) return;
    taken_samples -= samples_per_frame;

    float width = 0.0f;
    for (float volume : volume_buf) {
        if (volume > width) width = volume;
    }
    volume_buf.clear();

    bool split = pending_edge;
    pending_edge = false;

    if (state.frequency <= 0.0 || width <= 0.0f) {
        if (!slices.empty() && slices.back().width == 0.0f) {
            slices.back().height += 1.0f;
        } else {
            slices.push_back(SliceState{Color(0,0,0,0), 0.0f, 0.0f, 1.0f, frame_count});
            if (slices.size() > 4096) slices.pop_front();
            frame_count++;
        }
        return;
    }

    float n = 12.0f * std::log2f((float)state.frequency / C_0);
    float octave = std::floor(n / 12.0f) + starting_octave;
    float note = std::fmod(n, 12.0f);
    if (note < 0) note += 12.0f;

    Color color = settings ? settings->color(state.timbre) : Color(0x90,0x90,0x90);
    float index = note + 12.0f * octave;
    size_t frame = frame_count;

    if (!slices.empty()) {
        auto& last = slices.back();
        bool same_color = last.color.r == color.r && last.color.g == color.g && last.color.b == color.b;
        if (!split && last.width == width && ((same_color && last.index == index) || width == 0.0f)) {
            last.height += 1.0f;
            return;
        }

        if (std::abs(last.index - index) < 1.0f && same_color && last.width != 0.0f && width != 0.0f) {
            frame = last.frame;
        }
    }

    slices.push_back(SliceState{color, index, width, 1.0f, frame});
    if (slices.size() > 4096) slices.pop_front();
    frame_count++;
}

// Visualizer
Visualizer::Visualizer(size_t channels_, uint32_t width_, uint32_t height_, uint32_t sample_rate_,
                         const PianoRollConfig& config_, const std::string& font_path)
    : channels(channels_),
      width(width_),
      height(height_),
      sample_rate(sample_rate_),
      config(config_),
      channel_last_states(channels_),
      channel_filters(channels_, HighPassIIR((float)sample_rate_, 300.0f)),
      oscilloscope_states(channels_),
      piano_roll_states(),
      font(font_path),
      canvas(width_ * height_ * 4, 0) {
    piano_roll_states.reserve(channels_);
    for (size_t ch = 0; ch < channels_; ++ch) {
        piano_roll_states.emplace_back((float)sample_rate_, (float)config_.speed_multiplier * 4.0f, (float)config_.starting_octave);
    }
}

void Visualizer::clear() {
    std::fill(canvas.begin(), canvas.end(), 0);
}

void Visualizer::draw() {
    clear();

    size_t max_channels_per_row = (height > width) ? 4 : 8;
    int wave_h = (int)config.waveform_height;
    if (wave_h > (int)height / 3) wave_h = (int)height / 3;

    draw_oscilloscopes(0, 0, (int)width, wave_h, max_channels_per_row);
    draw_piano_roll(0, wave_h, (int)width, (int)height - wave_h);
}

void Visualizer::consume_state(size_t channel, const ChannelState& state) {
    if (channel >= channels) return;
    channel_filters[channel].consume(state.amplitude);

    ChannelState filtered = state;
    filtered.amplitude = channel_filters[channel].output();

    oscilloscope_states[channel].consume(filtered);
    piano_roll_states[channel].consume(filtered, config.settings.get(channel));
    channel_last_states[channel] = filtered;
}

void Visualizer::set_channel_settings(size_t channel, const ChannelSettings& settings) {
    if (channel >= channels) return;
    if (channel >= config.settings.settings.size()) {
        config.settings.settings.resize(channel + 1);
    }
    config.settings.settings[channel] = settings;
}

} // namespace fp
