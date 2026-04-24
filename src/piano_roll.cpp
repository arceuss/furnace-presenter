#include "visualizer.h"
#include <cmath>
#include <algorithm>

namespace fp {

extern void fill_rect(std::vector<uint8_t>& buf, int buf_w, int buf_h,
                      int x, int y, int w, int h,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a);

static const int PIANO_KEYS[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0}; // 0=white, 1=black

static int piano_key_type(int index, int key_count) {
    int k = PIANO_KEYS[index % 12];
    if (index >= key_count - 1 && k == 0 && (index % 12) != 4 && (index % 12) != 11) {
        return 2; // WhiteFull
    }
    return k;
}

static float piano_note_index(double frequency, int starting_octave) {
    float n = 12.0f * std::log2f((float)frequency / 16.351597831287f);
    float octave = std::floor(n / 12.0f) + (float)starting_octave;
    float note = std::fmod(n, 12.0f);
    if (note < 0) note += 12.0f;
    return note + 12.0f * octave;
}

float Visualizer::piano_keys_x(int x, int w) const {
    int key_count = 12 * (int)config.octave_count + 1;
    float key_thickness = std::max(config.key_thickness, 1.0f);
    float keys_w = key_thickness * key_count;
    if (keys_w <= (float)w) {
        return x + (((float)w - keys_w) / 2.0f) + (key_thickness / 2.0f) - 1.0f;
    }
    return x + (key_thickness / 2.0f) - 1.0f - piano_view_offset * key_thickness;
}

void Visualizer::update_piano_view(int w) {
    int key_count = 12 * (int)config.octave_count + 1;
    float key_thickness = std::max(config.key_thickness, 1.0f);
    float visible_keys = (float)w / key_thickness;
    float max_offset = std::max(0.0f, (float)key_count - visible_keys);
    if (max_offset <= 0.0f) {
        piano_view_offset = 0.0f;
        return;
    }

    if (piano_view_offset < 0.0f) {
        piano_view_offset = max_offset * 0.5f;
    }

    bool have_active = false;
    float min_index = 0.0f;
    float max_index = 0.0f;
    for (size_t ch = 0; ch < channels; ++ch) {
        const ChannelSettings* settings = config.settings.get(ch);
        const ChannelState& state = channel_last_states[ch];
        if (!settings || settings->hidden || state.volume <= 0.0f || state.frequency <= 0.0) continue;

        float index = piano_note_index(state.frequency, config.starting_octave);
        if (!std::isfinite(index)) continue;

        if (!have_active) {
            min_index = max_index = index;
            have_active = true;
        } else {
            min_index = std::min(min_index, index);
            max_index = std::max(max_index, index);
        }
    }

    float target = piano_view_offset;
    if (have_active) {
        float padding = std::min(12.0f, visible_keys * 0.15f);
        float active_span = max_index - min_index;
        if (active_span + 2.0f * padding >= visible_keys) {
            target = ((min_index + max_index) - visible_keys) * 0.5f;
        } else {
            float left = piano_view_offset;
            if (min_index < left + padding) {
                target = min_index - padding;
            }
            if (max_index > target + visible_keys - padding) {
                target = max_index + padding - visible_keys;
            }
        }
    }

    target = std::max(0.0f, std::min(target, max_offset));
    piano_view_offset += (target - piano_view_offset) * 0.12f;
    if (std::abs(target - piano_view_offset) < 0.01f) {
        piano_view_offset = target;
    }
}

void Visualizer::draw_piano_roll(int x, int y, int w, int h) {
    int key_len = (int)config.key_length;
    if (key_len > h / 4) key_len = h / 4;

    int slices_y = y + key_len;
    int slices_h = h - key_len;

    update_piano_view(w);
    // Draw outlines first
    draw_channel_slices(x, slices_y, w, slices_h, true);
    if (config.draw_piano_strings) {
        draw_piano_keys(x, slices_y, w, slices_h); // strings overlay
    }
    draw_channel_slices(x, slices_y, w, slices_h, false);
    draw_piano_keys(x, y, w, key_len);

    for (size_t ch = 0; ch < channels; ++ch) {
        draw_channel_key_spot(ch, x, y, w, key_len);
    }
}

void Visualizer::draw_piano_keys(int x, int y, int w, int h) {
    int key_count = 12 * (int)config.octave_count + 1;
    float keys_w = config.key_thickness * key_count;
    float keys_x = piano_keys_x(x, w);

    fill_rect(canvas, (int)width, (int)height, x, y, w, h + 1, 4, 4, 4, 255);
    fill_rect(canvas, (int)width, (int)height, (int)keys_x, y, (int)keys_w, h, 24, 24, 24, 255);

    for (int i = 0; i < key_count; ++i) {
        int kx = (int)(keys_x + config.key_thickness * i);
        int kt = piano_key_type(i, key_count);
        if (kt == 1) {
            fill_rect(canvas, (int)width, (int)height, kx, y, (int)config.key_thickness + 1, h / 2 + 1, 0, 0, 0, 255);
        } else {
            fill_rect(canvas, (int)width, (int)height, kx + 1, y + 1, (int)config.key_thickness - 1, h - 1, 32, 32, 32, 255);
        }
    }

    fill_rect(canvas, (int)width, (int)height, x, y, w, 1, 4, 4, 4, 255);
}

void Visualizer::draw_channel_key_spot(size_t channel, int x, int y, int w, int h) {
    int key_count = 12 * (int)config.octave_count + 1;
    const ChannelSettings* settings = config.settings.get(channel);
    const ChannelState& last = channel_last_states[channel];

    if (!settings || settings->hidden || last.volume <= 0.0f) return;
    if (last.frequency <= 0.0) return;

    Color col = settings->color(last.timbre);
    float volume_alpha = 0.5f + last.volume / 30.0f;
    if (volume_alpha > 1.0f) volume_alpha = 1.0f;

    float note_index = piano_note_index(last.frequency, config.starting_octave);
    if (!std::isfinite(note_index)) return;

    float note = std::fmod(note_index, 12.0f);
    if (note < 0) note += 12.0f;
    float octave = std::floor(note_index / 12.0f);

    float keys_x = piano_keys_x(x, w);

    int lower_note = (int)std::floor(note);
    float lower_alpha = 1.0f - (note - lower_note);
    int upper_note = (int)std::ceil(note) % 12;
    float upper_alpha = note - lower_note;

    int lower_idx = lower_note + 12 * (int)octave;
    int upper_idx = ((int)std::ceil(note)) % 12 + 12 * ((int)octave + (int)(std::ceil(note) / 12.0f));

    if (lower_idx >= 0 && lower_idx < key_count) {
        int kx = (int)(keys_x + config.key_thickness * lower_idx);
        uint8_t a = (uint8_t)(255 * volume_alpha * lower_alpha);
        fill_rect(canvas, (int)width, (int)height, kx, y, (int)config.key_thickness, h, col.r, col.g, col.b, a);
    }
    if (upper_alpha > 0.01f && upper_idx >= 0 && upper_idx < key_count && upper_idx != lower_idx) {
        int kx = (int)(keys_x + config.key_thickness * upper_idx);
        uint8_t a = (uint8_t)(255 * volume_alpha * upper_alpha);
        fill_rect(canvas, (int)width, (int)height, kx, y, (int)config.key_thickness, h, col.r, col.g, col.b, a);
    }
}

void Visualizer::draw_channel_slices(int x, int y, int w, int h, bool outline) {
    float keys_x = piano_keys_x(x, w);

    struct DrawableSlice {
        float y;
        SliceState slice;
    };
    std::vector<DrawableSlice> drawable;

    for (size_t ch = 0; ch < channels; ++ch) {
        const ChannelSettings* settings = config.settings.get(ch);
        if (!settings || settings->hidden) continue;

        float cy = (float)y;
        const auto& slices = piano_roll_states[ch].slices;
        for (auto it = slices.rbegin(); it != slices.rend(); ++it) {
            DrawableSlice item{cy, *it};
            cy += it->height;
            if (cy >= y + h) {
                item.slice.height -= cy - (y + h);
                drawable.push_back(item);
                break;
            }
            drawable.push_back(item);
        }
    }

    if (!outline) {
        std::sort(drawable.begin(), drawable.end(), [](const DrawableSlice& a, const DrawableSlice& b) {
            if (a.slice.width != b.slice.width) return a.slice.width > b.slice.width;
            if (a.slice.frame != b.slice.frame) return a.slice.frame > b.slice.frame;
            return a.y < b.y;
        });
    }

    for (const DrawableSlice& item : drawable) {
        const SliceState& slice = item.slice;
        if (slice.width <= 0.0f) continue;

        float sx = keys_x + config.key_thickness * slice.index - slice.width / 2.0f;
        float sw = slice.width;
        float sy = item.y;
        float sh = slice.height;
        if (outline) {
            sx -= config.key_thickness / 2.0f;
            sy -= config.key_thickness / 2.0f;
            sw += config.key_thickness;
            sh += config.key_thickness;
        }

        int ix = (int)sx;
        int iy = (int)sy;
        int iw = (int)std::ceil(sw);
        int ih = (int)std::ceil(sh);

        if (outline) {
            fill_rect(canvas, (int)width, (int)height, ix, iy, iw, ih,
                      config.outline_color.r, config.outline_color.g, config.outline_color.b, 255);
        } else {
            Color c = slice.color;
            uint8_t a = 255;
            if (slice.width < 1.0f) {
                a = (uint8_t)(std::max(0.0f, std::min(slice.width, 1.0f)) * 255.0f);
            }
            fill_rect(canvas, (int)width, (int)height, ix, iy, iw, ih, c.r, c.g, c.b, a);
        }
    }
}

} // namespace fp
