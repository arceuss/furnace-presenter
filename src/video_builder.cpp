#include "video_builder.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#else
#include <unistd.h>
#endif

namespace fp {

VideoBuilder::VideoBuilder(const VideoOptions& opts) : options(opts) {}

VideoBuilder::~VideoBuilder() {
    if (video_pipe) {
#ifdef _WIN32
        _pclose(video_pipe);
#else
        pclose(video_pipe);
#endif
        video_pipe = nullptr;
    }
    if (audio_file) {
        fclose(audio_file);
        audio_file = nullptr;
    }
    if (!temp_audio_path.empty()) {
        std::remove(temp_audio_path.c_str());
    }
}

bool VideoBuilder::start() {
    // Create temp audio file
#ifdef _WIN32
    char tmp_path[L_tmpnam];
    tmpnam_s(tmp_path, L_tmpnam);
    temp_audio_path = std::string(tmp_path) + ".f32";
#else
    char tmp_path[] = "/tmp/fp_audio_XXXXXX.f32";
    int fd = mkstemps(tmp_path, 4);
    if (fd >= 0) close(fd);
    temp_audio_path = tmp_path;
#endif

    audio_file = fopen(temp_audio_path.c_str(), "wb");
    if (!audio_file) {
        std::cerr << "Failed to create temp audio file\n";
        failed = true;
        return false;
    }

    // Start ffmpeg for video encoding
    std::ostringstream cmd;
    cmd << "ffmpeg -y -f rawvideo -pix_fmt rgba -s "
        << options.width << "x" << options.height
        << " -r " << options.fps
        << " -i - -an -c:v " << options.video_codec;
    
    if (options.video_codec == "libx264" || options.video_codec == "libx265") {
        cmd << " -preset veryfast -crf 20 -tune film";
    }
    cmd << " -pix_fmt " << options.pixel_format
        << " -shortest \"" << options.output_path << ".video.mkv\"";

#ifdef _WIN32
    video_pipe = _popen(cmd.str().c_str(), "wb");
#else
    video_pipe = popen(cmd.str().c_str(), "w");
#endif
    if (!video_pipe) {
        std::cerr << "Failed to start ffmpeg for video encoding\n";
        failed = true;
        return false;
    }

    return true;
}

void VideoBuilder::push_video_frame(const uint8_t* rgba) {
    if (!video_pipe || failed) return;
    size_t frame_size = options.width * options.height * 4;
    size_t written = fwrite(rgba, 1, frame_size, video_pipe);
    if (written != frame_size) {
        std::cerr << "Warning: short write to video pipe\n";
    }
}

void VideoBuilder::push_audio_samples(const float* samples, size_t count) {
    if (!audio_file || failed) return;
    fwrite(samples, sizeof(float), count, audio_file);
    audio_samples_written += count;
}

bool VideoBuilder::finish() {
    if (video_pipe) {
#ifdef _WIN32
        _pclose(video_pipe);
#else
        pclose(video_pipe);
#endif
        video_pipe = nullptr;
    }
    if (audio_file) {
        fclose(audio_file);
        audio_file = nullptr;
    }

    // Mux video and audio
    std::ostringstream cmd;
    cmd << "ffmpeg -y -i \"" << options.output_path << ".video.mkv\""
        << " -f f32le -ar " << options.sample_rate
        << " -ac " << options.audio_channels
        << " -i \"" << temp_audio_path << "\""
        << " -c:v copy -c:a " << options.audio_codec;
    if (options.audio_codec == "aac") {
        cmd << " -profile:a aac_low";
    }
    cmd << " -b:a 384k \"" << options.output_path << "\"";

    int ret = std::system(cmd.str().c_str());

    // Cleanup temp files
    std::string temp_video = options.output_path + ".video.mkv";
    std::remove(temp_video.c_str());
    std::remove(temp_audio_path.c_str());
    temp_audio_path.clear();

    return ret == 0;
}

size_t VideoBuilder::audio_frame_size() const {
    return options.sample_rate * options.audio_channels / options.fps;
}

} // namespace fp
