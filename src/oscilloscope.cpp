#include "visualizer.h"
#include <cmath>
#include <algorithm>

namespace fp {

extern void fill_rect(std::vector<uint8_t>& buf, int buf_w, int buf_h,
                      int x, int y, int w, int h,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a);
extern void draw_line(std::vector<uint8_t>& buf, int buf_w, int buf_h,
                      float x0, float y0, float x1, float y1,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void Visualizer::draw_oscilloscopes(int x, int y, int w, int h, size_t max_channels_per_row) {
    fill_rect(canvas, (int)width, (int)height, x, y, w, h, 0, 0, 0, 255);

    std::vector<size_t> visible;
    for (size_t i = 0; i < channels; ++i) {
        const ChannelSettings* s = config.settings.get(i);
        if (s && !s->hidden) visible.push_back(i);
    }

    size_t row_start = 0;
    while (row_start < visible.size()) {
        size_t row_end = std::min(row_start + max_channels_per_row, visible.size());
        size_t row_count = row_end - row_start;
        int cell_w = w / (int)row_count;

        for (size_t i = row_start; i < row_end; ++i) {
            int cx = x + (int)(i - row_start) * cell_w;
            draw_oscilloscope_view(visible[i], cx, y, cell_w, h);
        }
        row_start = row_end;
    }
}

void Visualizer::draw_oscilloscope_view(size_t channel, int x, int y, int w, int h) {
    const ChannelSettings* settings = config.settings.get(channel);
    const ChannelState& last = channel_last_states[channel];
    const OscilloscopeState& osc = oscilloscope_states[channel];

    if (!settings || settings->hidden) return;

    Color col = settings->color(last.timbre);
    if (last.volume <= 0.0f) {
        col = Color(col.r / 2 + 16, col.g / 2 + 16, col.b / 2 + 16);
    }

    // Background
    fill_rect(canvas, (int)width, (int)height, x, y, w, h, col.r / 8, col.g / 8, col.b / 8, 255);

    // Draw oscilloscope trace
    if (osc.amplitudes.empty()) return;

    int window_size = w * 2;
    if (window_size < 2) window_size = 2;

    size_t start = 0;
    if (osc.amplitudes.size() > (size_t)window_size) {
        start = osc.amplitudes.size() - window_size;
    }


    size_t end = std::min(start + window_size, osc.amplitudes.size());
    if (end <= start) return;

    // Build points
    std::vector<std::pair<float,float>> pts;
    pts.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        float px = (float)(i - start) / 2.0f;
        float py = (15.0f - osc.amplitudes[i]) * h / 30.0f;
        if (py < 0) py = 0;
        if (py > h) py = (float)h;
        pts.push_back({px, py});
    }

    // Draw glow
    for (int t = 0; t < 3; ++t) {
        uint8_t glow_a = (uint8_t)(64 >> t);
        for (size_t i = 1; i < pts.size(); ++i) {
            draw_line(canvas, (int)width, (int)height,
                      x + pts[i-1].first, y + pts[i-1].second,
                      x + pts[i].first, y + pts[i].second,
                      col.r, col.g, col.b, glow_a);
        }
    }

    // Draw main line
    for (size_t i = 1; i < pts.size(); ++i) {
        draw_line(canvas, (int)width, (int)height,
                  x + pts[i-1].first, y + pts[i-1].second,
                  x + pts[i].first, y + pts[i].second,
                  col.r, col.g, col.b, 255);
    }

    // Text labels
    if (config.draw_text_labels && font.valid()) {
        int pad = font.height() / 2;
        font.draw_text(canvas, (int)width, (int)height,
                       settings->chip, x + pad, y + pad,
                       col.r, col.g, col.b, 0.2f);
        int name_w = (int)settings->name.size() * font.width();
        font.draw_text(canvas, (int)width, (int)height,
                       settings->name, x + w - name_w - pad, y + h - pad * 3,
                       col.r, col.g, col.b, 0.2f);
    }
}

} // namespace fp
