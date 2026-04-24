#include "font.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cstring>

namespace fp {

Font::Font(const std::string& png_path) {
    char_map = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

    int channels = 0;
    unsigned char* data = stbi_load(png_path.c_str(), &img_w, &img_h, &channels, 4);
    if (!data) return;

    pixels.resize(img_w * img_h * 4);
    std::memcpy(pixels.data(), data, pixels.size());
    stbi_image_free(data);
}

int Font::find_char(char c) const {
    size_t pos = char_map.find(c);
    if (pos == std::string::npos) return -1;
    return (int)pos;
}

void Font::draw_text(std::vector<uint8_t>& dst, int dst_w, int dst_h,
                     const std::string& text, int x, int y,
                     uint8_t r, uint8_t g, uint8_t b, float opacity) const {
    if (opacity <= 0.0f || pixels.empty()) return;

    int cols = img_w / tile_w;

    for (size_t i = 0; i < text.size(); ++i) {
        int ci = find_char(text[i]);
        if (ci < 0) continue;

        int tx = (ci % cols) * tile_w;
        int ty = (ci / cols) * tile_h;
        int dx = x + (int)i * tile_w;

        for (int py = 0; py < tile_h; ++py) {
            for (int px = 0; px < tile_w; ++px) {
                int sx = tx + px;
                int sy = ty + py;
                int dst_x = dx + px;
                int dst_y = y + py;

                if (dst_x < 0 || dst_x >= dst_w || dst_y < 0 || dst_y >= dst_h) continue;

                size_t src_idx = (sy * img_w + sx) * 4;
                size_t dst_idx = (dst_y * dst_w + dst_x) * 4;

                float src_a = pixels[src_idx + 3] / 255.0f * opacity;
                if (src_a <= 0.0f) continue;

                float font_r = pixels[src_idx + 0] / 255.0f;
                float font_g = pixels[src_idx + 1] / 255.0f;
                float font_b = pixels[src_idx + 2] / 255.0f;

                float fr = (font_r * r) / 255.0f;
                float fg = (font_g * g) / 255.0f;
                float fb = (font_b * b) / 255.0f;

                float inv_a = 1.0f - src_a;
                dst[dst_idx + 0] = (uint8_t)(fr * src_a * 255.0f + dst[dst_idx + 0] * inv_a);
                dst[dst_idx + 1] = (uint8_t)(fg * src_a * 255.0f + dst[dst_idx + 1] * inv_a);
                dst[dst_idx + 2] = (uint8_t)(fb * src_a * 255.0f + dst[dst_idx + 2] * inv_a);
                dst[dst_idx + 3] = (uint8_t)(src_a * 255.0f + dst[dst_idx + 3] * inv_a);
            }
        }
    }
}

} // namespace fp
