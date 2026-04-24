#include "config.h"

namespace fp {

Color ChannelSettings::color(size_t timbre) const {
    size_t idx = colors.empty() ? timbre : (timbre % colors.size());
    if (idx < colors.size()) return colors[idx];
    return Color(0x90, 0x90, 0x90);
}

Config default_config() {
    Config cfg;
    // Default colors for common systems - can be customized via TOML later
    cfg.piano_roll.settings = ChannelSettingsManager({
        ChannelSettings("GB", "Pulse 1",  {Color(0xFF,0xBF,0xD4), Color(0xFF,0x73,0x8A), Color(0xFF,0x40,0x40), Color(0xFF,0x73,0x8A)}),
        ChannelSettings("GB", "Pulse 2",  {Color(0xFF,0xE0,0xA0), Color(0xFF,0xC0,0x40), Color(0xFF,0xFF,0x40), Color(0xFF,0xC0,0x40)}),
        ChannelSettings("GB", "Wave",     {Color(0x40,0xFF,0x40), Color(0x9A,0x4F,0xFF), Color(0x38,0xAB,0xF2), Color(0xAC,0xED,0x32), Color(0x24,0x7B,0xA0), Color(0x0F,0xF4,0xC6)}),
        ChannelSettings("GB", "Noise",    {Color(0xC0,0xC0,0xC0), Color(0x80,0xF0,0xFF)}),
    });
    return cfg;
}

Config load_config(const std::string& path) {
    // TODO: TOML parsing with toml11
    (void)path;
    return default_config();
}

} // namespace fp
