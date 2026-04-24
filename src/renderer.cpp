#include "renderer.h"
#include <climits>
#include "engine/engine.h"
#include "engine/macroInt.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace fp {

// Map Furnace channel type to default color
static Color furnace_channel_color(int type) {
    switch (type) {
        case 0: return Color(51, 204, 255);   // FM
        case 1: return Color(102, 255, 51);   // PULSE
        case 2: return Color(204, 204, 204);  // NOISE
        case 3: return Color(255, 128, 51);   // WAVE
        case 4: return Color(255, 230, 51);   // PCM
        case 5: return Color(51, 102, 255);   // OP
        default: return Color(144, 144, 144);
    }
}


static bool furnace_system_color(DivSystem sys, Color& color) {
    switch (sys) {
        case DIV_SYSTEM_VBOY:
            color = Color(224, 16, 16);
            return true;
        default:
            return false;
    }
}

static const char* furnace_channel_system_label(DivEngine* engine, int channel) {
    if (!engine || channel < 0 || channel >= engine->song.chans) return "Unknown";
    DivSystem sys = engine->song.sysOfChan[channel];
    switch (sys) {
        case DIV_SYSTEM_YM2612:
        case DIV_SYSTEM_YM2612_DUALPCM:
        case DIV_SYSTEM_YM2612_EXT:
        case DIV_SYSTEM_YM2612_DUALPCM_EXT:
            return "YM2612";
        case DIV_SYSTEM_SMS:
            return "SN76489";
        default:
            return engine->getSystemName(sys);
    }
}

static uint8_t scale_color_channel(uint8_t channel, float scale) {
    return (uint8_t)std::max(0.0f, std::min(255.0f, channel * scale));
}

static std::vector<Color> furnace_color_variants(Color base) {
    return {
        Color(scale_color_channel(base.r, 0.70f), scale_color_channel(base.g, 0.70f), scale_color_channel(base.b, 0.70f)),
        base,
        Color(scale_color_channel(base.r, 1.15f), scale_color_channel(base.g, 1.15f), scale_color_channel(base.b, 1.15f)),
        Color(scale_color_channel(base.r, 1.35f), scale_color_channel(base.g, 1.35f), scale_color_channel(base.b, 1.35f)),
        Color((uint8_t)((base.r + 255) / 2), (uint8_t)((base.g + 255) / 2), (uint8_t)((base.b + 255) / 2))
    };
}

static void mix_macro_timbre(size_t& timbre, const DivMacroStruct& macro) {
    if (!macro.had && !macro.has) return;
    timbre = timbre * 131u + (size_t)macro.macroType * 17u + (size_t)(macro.val & 0xffff);
}

static void mix_op_timbre(size_t& timbre, const DivMacroInt::IntOp& op) {
    mix_macro_timbre(timbre, op.am);
    mix_macro_timbre(timbre, op.ar);
    mix_macro_timbre(timbre, op.dr);
    mix_macro_timbre(timbre, op.mult);
    mix_macro_timbre(timbre, op.rr);
    mix_macro_timbre(timbre, op.sl);
    mix_macro_timbre(timbre, op.tl);
    mix_macro_timbre(timbre, op.dt2);
    mix_macro_timbre(timbre, op.rs);
    mix_macro_timbre(timbre, op.dt);
    mix_macro_timbre(timbre, op.d2r);
    mix_macro_timbre(timbre, op.ssg);
    mix_macro_timbre(timbre, op.dam);
    mix_macro_timbre(timbre, op.dvb);
    mix_macro_timbre(timbre, op.egt);
    mix_macro_timbre(timbre, op.ksl);
    mix_macro_timbre(timbre, op.sus);
    mix_macro_timbre(timbre, op.vib);
    mix_macro_timbre(timbre, op.ws);
    mix_macro_timbre(timbre, op.ksr);
}

static size_t furnace_macro_timbre(const DivMacroInt* macro) {
    if (!macro) return 0;

    size_t timbre = 0;
    mix_macro_timbre(timbre, macro->duty);
    mix_macro_timbre(timbre, macro->wave);
    mix_macro_timbre(timbre, macro->ex1);
    mix_macro_timbre(timbre, macro->ex2);
    mix_macro_timbre(timbre, macro->ex3);
    mix_macro_timbre(timbre, macro->alg);
    mix_macro_timbre(timbre, macro->fb);
    mix_macro_timbre(timbre, macro->fms);
    mix_macro_timbre(timbre, macro->ams);
    mix_macro_timbre(timbre, macro->panL);
    mix_macro_timbre(timbre, macro->panR);
    mix_macro_timbre(timbre, macro->phaseReset);
    mix_macro_timbre(timbre, macro->ex4);
    mix_macro_timbre(timbre, macro->ex5);
    mix_macro_timbre(timbre, macro->ex6);
    mix_macro_timbre(timbre, macro->ex7);
    mix_macro_timbre(timbre, macro->ex8);
    mix_macro_timbre(timbre, macro->ex9);
    mix_macro_timbre(timbre, macro->ex10);
    for (int op = 0; op < 4; ++op) {
        mix_op_timbre(timbre, macro->op[op]);
    }
    return timbre;
}

static std::string furnace_channel_type_name(int type) {
    switch (type) {
        case 0: return "FM";
        case 1: return "Pulse";
        case 2: return "Noise";
        case 3: return "Wave";
        case 4: return "PCM";
        case 5: return "OP";
        default: return "Ch";
    }
}

static uint32_t unpack_color(unsigned int c) {
    // ImU32 is 0xRRGGBBAA
    return c;
}

static bool channel_has_ordered_note(DivEngine* engine, int channel) {
    if (!engine || !engine->curSubSong || channel < 0 || channel >= engine->song.chans) return false;

    DivSubSong* subSong = engine->curSubSong;
    bool checked_pattern[DIV_MAX_PATTERNS];
    memset(checked_pattern, 0, sizeof(checked_pattern));

    for (int order = 0; order < subSong->ordersLen; ++order) {
        int pattern_index = subSong->orders.ord[channel][order];
        if (pattern_index < 0 || pattern_index >= DIV_MAX_PATTERNS) continue;
        if (checked_pattern[pattern_index]) continue;
        checked_pattern[pattern_index] = true;

        DivPattern* pattern = subSong->pat[channel].data[pattern_index];
        if (!pattern) continue;

        for (int row = 0; row < subSong->patLen; ++row) {
            short note = pattern->newData[row][DIV_PAT_NOTE];
            if (note >= 0 && note < DIV_NOTE_NULL_PAT) {
                return true;
            }
        }
    }

    return false;
}

Renderer::Renderer(const RenderOptions& opts) : options(opts) {}

Renderer::~Renderer() = default;

bool Renderer::init() {
    engine = std::make_unique<DivEngine>();
    engine->preInit(true);
    engine->setAudio(DIV_AUDIO_DUMMY);
    engine->setConf("audioRate", (int)options.sample_rate);
    engine->setConf("opl3CoreRender", 0); // Furnace render core value 0 is Nuked-OPL3.

    if (!load_module()) {
        return false;
    }

    if (!engine->init()) {
        std::cerr << "Failed to initialize Furnace engine\n";
        return false;
    }

    // Temporarily keep the post-init dispatch state unchanged while isolating
    // the render-core switch path crash.

    int totalChans = engine->getTotalChannelCount();
    if (totalChans <= 0) {
        std::cerr << "Module has no channels\n";
        return false;
    }

    // Scale piano roll config for output resolution (defaults are for 960x540)
    float scale = (float)options.width / 960.0f;
    if (scale > 1.0f) {
        options.config.piano_roll.key_length *= scale;
        options.config.piano_roll.key_thickness *= scale;
        options.config.piano_roll.divider_width = (uint32_t)(options.config.piano_roll.divider_width * scale);
        options.config.piano_roll.waveform_height = (uint32_t)(options.config.piano_roll.waveform_height * scale);
        options.config.piano_roll.oscilloscope_glow_thickness *= scale;
        options.config.piano_roll.oscilloscope_line_thickness *= scale;
    }

    viz = std::make_unique<Visualizer>(
        (size_t)totalChans,
        options.width,
        options.height,
        options.sample_rate,
        options.config.piano_roll,
        "assets/8x8_font.png"
    );

    // Populate channel settings from Furnace
    for (int i = 0; i < totalChans; ++i) {
        ChannelSettings cs;
        int chType = engine->getChannelType(i);
        unsigned int customCol = engine->curSubSong->chanColor[i];
        Color base_color;
        if (customCol != 0) {
            uint32_t c = unpack_color(customCol);
            base_color = Color((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF);
        } else if (!furnace_system_color(engine->song.sysOfChan[i], base_color)) {
            base_color = furnace_channel_color(chType);
        }
        cs.colors = furnace_color_variants(base_color);
        cs.chip = furnace_channel_system_label(engine.get(), i);
        cs.name = engine->getChannelName(i);
        if (options.hide_unused && !channel_has_ordered_note(engine.get(), i)) {
            cs.hidden = true;
        }
        viz->set_channel_settings((size_t)i, cs);
    }

    VideoOptions vopts;
    vopts.output_path = options.output_path;
    vopts.width = options.width;
    vopts.height = options.height;
    vopts.fps = options.fps;
    vopts.sample_rate = options.sample_rate;
    vopts.video_codec = options.video_codec;
    vopts.audio_codec = options.audio_codec;
    vopts.background_path = options.background_path;

    vb = std::make_unique<VideoBuilder>(vopts);
    if (!vb->start()) {
        return false;
    }

    // Prepare audio buffers
    audio_buf_l.resize(options.sample_rate / options.fps);
    audio_buf_r.resize(options.sample_rate / options.fps);
    prev_frequencies.resize(totalChans, 0.0);
    prev_visual_notes.resize(totalChans, -1);

    walked_positions.clear();
    last_order = -1;
    last_row = -1;

    // Set up playback
    engine->setOrder(0);
    engine->play();

    return true;
}

bool Renderer::load_module() {
    FILE* f = fopen(options.input_path.c_str(), "rb");
    if (!f) {
        std::cerr << "Cannot open file: " << options.input_path << "\n";
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return false;
    }
    rewind(f);

    std::vector<unsigned char> buf(len);
    if (fread(buf.data(), 1, len, f) != (size_t)len) {
        fclose(f);
        return false;
    }
    fclose(f);

    unsigned char* engine_buf = new unsigned char[(size_t)len];
    memcpy(engine_buf, buf.data(), (size_t)len);
    if (!engine->load(engine_buf, (size_t)len, options.input_path.c_str())) {
        std::cerr << "Failed to load module: " << engine->getLastError() << "\n";
        return false;
    }

    return true;
}

void Renderer::poll_channels(int samples_to_feed) {
    int totalChans = engine->getTotalChannelCount();
    static std::vector<int> last_notes;
    if ((int)last_notes.size() < totalChans) last_notes.resize(totalChans, -1);

    for (int i = 0; i < totalChans; ++i) {
        DivChannelState* cs = engine->getChanState(i);
        DivDispatchOscBuffer* osc = engine->getOscBuffer(i);

        ChannelState base_state;
        base_state.volume = 0.0f;
        base_state.amplitude = 0.0f;
        base_state.frequency = 0.0;
        base_state.timbre = 0;
        base_state.balance = 0.5f;
        base_state.edge = false;

        bool key_hit = false;

        SharedChannel* shared = engine->getDispatchChanState(i);
        int visual_note = -1;

        if (cs) {
            int volMax = cs->volMax >> 8;
            if (volMax <= 0) volMax = 15;

            // Dispatch state reflects active note state and post-macro output volume.
            int vol = shared ? shared->outVol : (cs->volume >> 8);
            if (vol < 0) vol = 0;

            bool playing = shared ? (shared->active && vol > 0) : (cs->note >= 0 && cs->note < 180 && vol > 0);
            base_state.volume = playing ? std::min((float)vol / (float)volMax, 1.0f) * 15.0f : 0.0f;

            size_t timbre = shared && shared->ins >= 0 ? (size_t)shared->ins : 0;
            timbre += furnace_macro_timbre(engine->getMacroInt(i));
            base_state.timbre = timbre;

            if (playing) {
                double note_pos = -1.0;
                if (shared) {
                    visual_note = shared->fixedArp ? shared->baseNoteOverride : shared->note + shared->arpOff;
                    if (engine->song.compatFlags.linearPitch) {
                        int base128 = shared->fixedArp ? (shared->baseNoteOverride << 7) : shared->baseFreq + (shared->arpOff << 7);
                        note_pos = (double)(base128 + shared->pitch + shared->pitch2) / 128.0;
                    } else {
                        note_pos = (double)visual_note + (double)(shared->pitch + shared->pitch2) / 128.0;
                    }
                } else {
                    visual_note = cs->note;
                    note_pos = (double)visual_note + (double)(cs->pitch / 32) / 128.0;
                }

                if (note_pos >= 0.0 && note_pos < 180.0) {
                    base_state.frequency = 16.351597831287 * std::pow(2.0, note_pos / 12.0);
                } else {
                    visual_note = -1;
                    base_state.volume = 0.0f;
                }
            }

            base_state.balance = (cs->panR - cs->panL + 255.0f) / 510.0f;
            base_state.balance = std::max(0.0f, std::min(1.0f, base_state.balance));

            if (engine->keyHit[i]) {
                key_hit = true;
                engine->keyHit[i] = false;
            }
            last_notes[i] = visual_note;
        }

        std::vector<float> amplitudes;
        amplitudes.reserve(samples_to_feed);

        if (osc) {
            unsigned short needlePos = (unsigned short)(osc->needle >> 16);
            float last_amp = 0.0f;
            for (int j = 0; j < samples_to_feed; ++j) {
                unsigned short idx = needlePos - (unsigned short)(samples_to_feed - 1 - j);
                short y = osc->data[idx];
                if (y == -1) {
                    // hold previous
                } else if (y == -2) {
                    last_amp = -1.0f / 2048.0f;
                } else {
                    last_amp = (float)y / 2048.0f;
                }
                amplitudes.push_back(last_amp);
            }
        } else {
            float fallback_amp = (base_state.volume / 15.0f - 0.5f) * 30.0f;
            for (int j = 0; j < samples_to_feed; ++j) {
                amplitudes.push_back(fallback_amp);
            }
        }

        double prev_freq = prev_frequencies[i];
        int prev_note = prev_visual_notes[i];
        bool interpolate_pitch = prev_note == visual_note && prev_freq > 0.0 && base_state.frequency > 0.0;

        for (int j = 0; j < samples_to_feed; ++j) {
            ChannelState state = base_state;
            state.amplitude = amplitudes[j];
            state.edge = (j == 0 && key_hit);
            if (interpolate_pitch && samples_to_feed > 1) {
                double t = (double)j / (double)(samples_to_feed - 1);
                state.frequency = prev_freq + (base_state.frequency - prev_freq) * t;
            }
            viz->consume_state((size_t)i, state);
        }
        prev_frequencies[i] = base_state.frequency;
        prev_visual_notes[i] = visual_note;
    }
}

bool Renderer::should_stop() {
    if (!engine->isPlaying()) return true;

    auto begin_fade = [&]() {
        if (!fading) {
            fading = true;
            fadeout_timer = options.fadeout_length;
            fadeout_start_frame = cur_frame;
        }
    };

    if (options.stop_condition.rfind("loops:", 0) == 0) {
        int target = std::stoi(options.stop_condition.substr(6));
        if ((int)loop_count >= target) {
            begin_fade();
        }
    } else if (options.stop_condition.rfind("frames:", 0) == 0) {
        int target = std::stoi(options.stop_condition.substr(7));
        if ((int)cur_frame >= target) {
            begin_fade();
        }
    } else if (options.stop_condition.rfind("time:", 0) == 0) {
        int target = std::stoi(options.stop_condition.substr(5));
        if ((int)cur_frame >= target * (int)options.fps) {
            begin_fade();
        }
    }

    return fading && cur_frame >= fadeout_start_frame + options.fadeout_length;
}

void Renderer::apply_fadeout(float* out_l, float* out_r, size_t count) {
    if (!fading || options.fadeout_length == 0) return;

    float start_mul = (float)fadeout_timer / (float)options.fadeout_length;
    float end_mul = (float)std::max<int64_t>((int64_t)fadeout_timer - 1, 0) / (float)options.fadeout_length;
    for (size_t i = 0; i < count; ++i) {
        float t = count > 1 ? (float)i / (float)(count - 1) : 1.0f;
        float mul = start_mul + (end_mul - start_mul) * t;
        out_l[i] *= mul;
        out_r[i] *= mul;
    }

    if (fadeout_timer > 0) {
        fadeout_timer--;
    }
}

bool Renderer::render() {
    size_t buf_size = audio_buf_l.size();

    auto start_time = std::chrono::high_resolution_clock::now();

    while (engine->isPlaying()) {
        float* outBuf[2] = {audio_buf_l.data(), audio_buf_r.data()};
        engine->nextBuf(NULL, outBuf, 0, 2, (unsigned int)buf_size);
        apply_fadeout(outBuf[0], outBuf[1], buf_size);

        poll_channels((int)buf_size);
        viz->draw();
        if (fading && options.fadeout_length > 0) {
            float fade_mul = (float)fadeout_timer / (float)options.fadeout_length;
            uint8_t overlay_alpha = (uint8_t)std::max(0.0f, std::min(255.0f, (1.0f - fade_mul) * 255.0f));
            if (overlay_alpha > 0) {
                std::vector<uint8_t>& canvas = viz->get_canvas();
                float src_a = overlay_alpha / 255.0f;
                for (size_t px = 0; px < canvas.size(); px += 4) {
                    canvas[px + 0] = (uint8_t)(canvas[px + 0] * (1.0f - src_a));
                    canvas[px + 1] = (uint8_t)(canvas[px + 1] * (1.0f - src_a));
                    canvas[px + 2] = (uint8_t)(canvas[px + 2] * (1.0f - src_a));
                    canvas[px + 3] = 255;
                }
            }
        }

        vb->push_video_frame(viz->get_canvas_buffer());
        if (vb->is_failed()) {
            std::cerr << "Video output failed\n";
            return false;
        }

        std::vector<float> interleaved(buf_size * 2);
        for (size_t i = 0; i < buf_size; ++i) {
            interleaved[i * 2 + 0] = outBuf[0][i];
            interleaved[i * 2 + 1] = outBuf[1][i];
        }
        vb->push_audio_samples(interleaved.data(), interleaved.size());
        if (vb->is_failed()) {
            std::cerr << "Audio output failed\n";
            return false;
        }

        int order = 0, row = 0;
        engine->getPlayPos(order, row);
        if (last_order >= 0 && (order != last_order || row != last_row)) {
            uint16_t pos = (uint16_t)((((unsigned int)order & 0xffu) << 8) | ((unsigned int)row & 0xffu));
            if (walked_positions.find(pos) != walked_positions.end()) {
                loop_count++;
                walked_positions.clear();
            }
            walked_positions.insert(pos);
        }
        last_order = order;
        last_row = row;

        cur_frame++;

        if (cur_frame % 60 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - start_time).count();
            double fps = (double)cur_frame / elapsed;
            std::cout << "Frame " << cur_frame << " (" << (int)fps << " fps, loop " << loop_count << ")\r" << std::flush;
        }

        if (should_stop()) break;
    }

    std::cout << "\n";
    return true;
}

void Renderer::finish() {
    if (vb) {
        vb->finish();
    }
}

} // namespace fp
