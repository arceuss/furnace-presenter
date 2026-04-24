#pragma once

#include "config.h"
#include "visualizer.h"
#include "video_builder.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

// Forward declare Furnace engine
class DivEngine;

namespace fp {

struct RenderOptions {
    std::string input_path;
    std::string output_path;
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    uint32_t sample_rate = 44100;
    std::string video_codec = "libx264";
    std::string audio_codec = "aac";
    std::string stop_condition = "loops:2";
    uint32_t fadeout_length = 180; // frames
    std::string background_path;
    Config config;
    bool hide_unused = false;
};

class Renderer {
    RenderOptions options;
    std::unique_ptr<DivEngine> engine;
    std::unique_ptr<Visualizer> viz;
    std::unique_ptr<VideoBuilder> vb;

    uint64_t cur_frame = 0;
    uint64_t loop_count = 0;
    int last_order = -1;
    int last_row = -1;
    uint64_t fadeout_timer = 0;
    uint64_t fadeout_start_frame = 0;
    bool fading = false;
    std::unordered_set<uint16_t> walked_positions;

    std::vector<float> audio_buf_l;
    std::vector<float> audio_buf_r;
    std::vector<double> prev_frequencies;
    std::vector<int> prev_visual_notes;

    bool load_module();
    void poll_channels(int samples_to_feed);
    bool should_stop();
    void apply_fadeout(float* out_l, float* out_r, size_t count);

public:
    explicit Renderer(const RenderOptions& opts);
    ~Renderer();

    bool init();
    bool render();
    void finish();
};

} // namespace fp
