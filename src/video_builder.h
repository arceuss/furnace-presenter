#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>

namespace fp {

struct VideoOptions {
    std::string output_path;
    std::string video_codec = "libx264";
    std::string audio_codec = "aac";
    std::string pixel_format = "yuv420p";
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    uint32_t sample_rate = 44100;
    uint32_t audio_channels = 2;
    std::string background_path;
};

class VideoBuilder {
    VideoOptions options;
    FILE* video_pipe = nullptr;
    std::string temp_audio_path;
    FILE* audio_file = nullptr;
    size_t audio_samples_written = 0;
    bool failed = false;

public:
    explicit VideoBuilder(const VideoOptions& opts);
    ~VideoBuilder();

    bool start();
    void push_video_frame(const uint8_t* rgba);
    void push_audio_samples(const float* samples, size_t count);
    bool finish();

    bool is_failed() const { return failed; }
    size_t audio_frame_size() const;
};

} // namespace fp
