#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace fp {

struct Color {
    uint8_t r, g, b, a;
    Color(uint8_t r_=0, uint8_t g_=0, uint8_t b_=0, uint8_t a_=255) : r(r_), g(g_), b(b_), a(a_) {}
    static Color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return Color(r,g,b,a); }
};

struct ChannelSettings {
    std::string chip;
    std::string name;
    bool hidden;
    std::vector<Color> colors;

    ChannelSettings(const std::string& chip_="<?>", const std::string& name_="<?>",
                    const std::vector<Color>& colors_={Color(0x90,0x90,0x90)})
        : chip(chip_), name(name_), hidden(false), colors(colors_) {}

    Color color(size_t timbre) const;
    size_t num_colors() const { return colors.size(); }
};

struct ChannelSettingsManager {
    std::vector<ChannelSettings> settings;

    ChannelSettingsManager() = default;
    explicit ChannelSettingsManager(std::vector<ChannelSettings>&& s) : settings(std::move(s)) {}

    const ChannelSettings* get(size_t channel) const {
        if (channel < settings.size()) return &settings[channel];
        return nullptr;
    }
    ChannelSettings* get_mut(size_t channel) {
        if (channel < settings.size()) return &settings[channel];
        return nullptr;
    }
};

struct PianoRollConfig {
    ChannelSettingsManager settings;
    float key_length = 24.0f;
    float key_thickness = 7.5f;
    uint32_t divider_width = 5;
    uint32_t octave_count = 16;
    uint32_t speed_multiplier = 1;
    int32_t starting_octave = 0;
    uint32_t waveform_height = 48;
    float oscilloscope_glow_thickness = 2.0f;
    float oscilloscope_line_thickness = 0.75f;
    bool draw_piano_strings = false;
    bool draw_text_labels = true;
    Color outline_color = Color(0,0,0);
    Color divider_color = Color(0,0,0);
};

struct Config {
    PianoRollConfig piano_roll;
};

Config load_config(const std::string& path);
Config default_config();

} // namespace fp
