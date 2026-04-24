#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fp {

class Font {
    std::vector<uint8_t> pixels;
    int img_w = 0, img_h = 0;
    int tile_w = 8, tile_h = 8;
    std::string char_map;

    int find_char(char c) const;

public:
    Font(const std::string& png_path);
    bool valid() const { return !pixels.empty(); }
    int width() const { return tile_w; }
    int height() const { return tile_h; }

    // Draw text into RGBA buffer at (x,y) with given opacity (0-1)
    void draw_text(std::vector<uint8_t>& dst, int dst_w, int dst_h,
                   const std::string& text, int x, int y,
                   uint8_t r, uint8_t g, uint8_t b, float opacity) const;
};

} // namespace fp
