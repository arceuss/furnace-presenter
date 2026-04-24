#include "cli.h"
#include "cxxopts.hpp"
#include <cmath>
#include <iostream>

namespace fp {

ParsedArgs parse_cli(int argc, char** argv) {
    ParsedArgs result;
    try {
        cxxopts::Options options("furnace-presenter", "Furnace tracker module visualizer");
        options.add_options()
            ("g,gui", "Open GUI window")
            ("i,input", "Input .fur/.dmf file", cxxopts::value<std::string>())
            ("o,output", "Output video path", cxxopts::value<std::string>())
            ("w,width", "Video width", cxxopts::value<uint32_t>()->default_value("1920"))
            ("h,height", "Video height", cxxopts::value<uint32_t>()->default_value("1080"))
            ("r,fps", "Video framerate", cxxopts::value<uint32_t>()->default_value("60"))
            ("s,sample-rate", "Audio sample rate", cxxopts::value<uint32_t>()->default_value("44100"))
            ("v,video-codec", "Video codec", cxxopts::value<std::string>()->default_value("libx264"))
            ("a,audio-codec", "Audio codec", cxxopts::value<std::string>()->default_value("aac"))
            ("t,stop", "Stop condition (loops:N, frames:N, time:N)", cxxopts::value<std::string>()->default_value("loops:2"))
            ("f,fadeout", "Fadeout length in frames", cxxopts::value<uint32_t>()->default_value("180"))
            ("fadeout-ms", "Fadeout length in milliseconds", cxxopts::value<uint32_t>())
            ("b,background", "Background image path", cxxopts::value<std::string>())
            ("c,config", "Config TOML path", cxxopts::value<std::string>())
            ("u,hide-unused", "Hide channels with no note-on events in ordered patterns")
            ("help", "Print help")
        ;

        auto parsed = options.parse(argc, argv);

        if (parsed.count("help")) {
            result.show_help = true;
            std::cout << options.help() << "\n";
            result.valid = true;
            return result;
        }

        bool gui_mode = parsed.count("gui");
        result.gui_mode = gui_mode;
        if (!gui_mode && (!parsed.count("input") || !parsed.count("output"))) {
            result.error_message = "Missing required arguments: --input and --output (or use --gui)";
            std::cerr << result.error_message << "\n" << options.help() << "\n";
            return result;
        }

        if (!gui_mode) {
            result.render_options.input_path = parsed["input"].as<std::string>();
            result.render_options.output_path = parsed["output"].as<std::string>();
        }
        result.render_options.width = parsed["width"].as<uint32_t>();
        result.render_options.height = parsed["height"].as<uint32_t>();
        result.render_options.fps = parsed["fps"].as<uint32_t>();
        result.render_options.sample_rate = parsed["sample-rate"].as<uint32_t>();
        result.render_options.video_codec = parsed["video-codec"].as<std::string>();
        result.render_options.audio_codec = parsed["audio-codec"].as<std::string>();
        result.render_options.stop_condition = parsed["stop"].as<std::string>();
        if (parsed.count("fadeout-ms")) {
            result.render_options.fadeout_length = (uint32_t)std::max(0, (int)std::lround((double)parsed["fadeout-ms"].as<uint32_t>() * (double)result.render_options.fps / 1000.0));
        } else {
            result.render_options.fadeout_length = parsed["fadeout"].as<uint32_t>();
        }

        if (parsed.count("background")) {
            result.render_options.background_path = parsed["background"].as<std::string>();
        }
        if (parsed.count("config")) {
            result.render_options.config = load_config(parsed["config"].as<std::string>());
        } else {
            result.render_options.config = default_config();
        }
        result.render_options.hide_unused = parsed.count("hide-unused") > 0;

        result.valid = true;
    } catch (const std::exception& e) {
        result.error_message = e.what();
        std::cerr << "CLI error: " << e.what() << "\n";
    }

    return result;
}

} // namespace fp
