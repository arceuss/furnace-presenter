#include "gui.h"
#include "engine/song.h"
#include "engine/engine.h"
#include "engine/macroInt.h"
#include "renderer.h"
#include "visualizer.h"
#include "config.h"
#include "filters.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl2.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace fp {

static bool furnace_system_color(DivSystem sys, Color& color) {
    switch (sys) {
        case DIV_SYSTEM_VBOY:
            color = Color(0xE0, 0x10, 0x10);
            return true;
        default:
            return false;
    }
}

static Color channel_type_color(int type) {
    switch (type) {
        case 0: return Color(0x33, 0xCC, 0xFF); // FM
        case 1: return Color(0x66, 0xFF, 0x33); // Pulse
        case 2: return Color(0xCC, 0xCC, 0xCC); // Noise
        case 3: return Color(0xFF, 0x80, 0x33); // Wave
        case 4: return Color(0xFF, 0xE6, 0x33); // PCM
        case 5: return Color(0x33, 0x66, 0xFF); // FM Operator
        default: return Color(0x90, 0x90, 0x90);
    }
}

static uint8_t scale_color_channel(uint8_t channel, float scale) {
    return (uint8_t)std::max(0.0f, std::min(255.0f, channel * scale));
}

static std::vector<Color> color_variants(Color base) {
    return {
        Color(scale_color_channel(base.r, 0.70f), scale_color_channel(base.g, 0.70f), scale_color_channel(base.b, 0.70f)),
        base,
        Color(scale_color_channel(base.r, 1.15f), scale_color_channel(base.g, 1.15f), scale_color_channel(base.b, 1.15f)),
        Color(scale_color_channel(base.r, 1.35f), scale_color_channel(base.g, 1.35f), scale_color_channel(base.b, 1.35f)),
        Color((uint8_t)((base.r + 255) / 2), (uint8_t)((base.g + 255) / 2), (uint8_t)((base.b + 255) / 2))
    };
}

static size_t macro_timbre(const DivMacroInt* macro) {
    if (!macro) return 0;
    size_t timbre = 0;
    auto mix = [&](const DivMacroStruct& m) {
        if (!m.had && !m.has) return;
        timbre = timbre * 131u + (size_t)m.macroType * 17u + (size_t)(m.val & 0xffff);
    };
    mix(macro->duty);
    mix(macro->wave);
    mix(macro->pitch);
    mix(macro->ex1);
    mix(macro->ex2);
    mix(macro->ex3);
    mix(macro->alg);
    mix(macro->fb);
    mix(macro->fms);
    mix(macro->ams);
    mix(macro->panL);
    mix(macro->panR);
    mix(macro->phaseReset);
    mix(macro->ex4);
    mix(macro->ex5);
    mix(macro->ex6);
    mix(macro->ex7);
    mix(macro->ex8);
    mix(macro->ex9);
    mix(macro->ex10);
    for (int op = 0; op < 4; ++op) {
        mix(macro->op[op].am);
        mix(macro->op[op].ar);
        mix(macro->op[op].dr);
        mix(macro->op[op].mult);
        mix(macro->op[op].rr);
        mix(macro->op[op].sl);
        mix(macro->op[op].tl);
        mix(macro->op[op].dt2);
        mix(macro->op[op].rs);
        mix(macro->op[op].dt);
        mix(macro->op[op].d2r);
        mix(macro->op[op].ssg);
        mix(macro->op[op].dam);
        mix(macro->op[op].dvb);
        mix(macro->op[op].egt);
        mix(macro->op[op].ksl);
        mix(macro->op[op].sus);
        mix(macro->op[op].vib);
        mix(macro->op[op].ws);
        mix(macro->op[op].ksr);
    }
    return timbre;
}

static double state_note_pos(const DivSong& song, SharedChannel* shared, DivChannelState* cs, int& visual_note) {
    visual_note = -1;
    if (shared) {
        visual_note = shared->fixedArp ? shared->baseNoteOverride : shared->note + shared->arpOff;
        if (song.compatFlags.linearPitch) {
            int base128 = shared->fixedArp ? (shared->baseNoteOverride << 7) : shared->baseFreq + (shared->arpOff << 7);
            return (double)(base128 + shared->pitch + shared->pitch2) / 128.0;
        }
        return (double)visual_note + (double)(shared->pitch + shared->pitch2) / 128.0;
    }
    if (cs) {
        visual_note = cs->note;
        return (double)visual_note + (double)(cs->pitch / 32) / 128.0;
    }
    return -1.0;
}

struct Gui::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    SDL_AudioDeviceID audio_dev = 0;

    std::unique_ptr<DivEngine> engine;
    std::unique_ptr<Visualizer> viz;

    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t sample_rate = 44100;
    uint32_t fps = 60;

    bool playing = false;
    bool need_load = false;
    bool render_requested = false;
    bool render_error = false;
    std::string pending_path;
    std::string render_error_message;
    char file_path[512] = {};
    char output_path[512] = "output.mp4";
    int render_loops = 2;
    float render_fade_seconds = 3.0f;
    bool render_hide_unused = false;
    std::atomic<bool> render_in_progress = false;
    std::thread render_thread;

    GLuint viz_texture = 0;
    std::vector<float> audio_l;
    std::vector<float> audio_r;
    std::vector<double> prev_freqs;
    std::vector<int> prev_notes;

    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow("furnace-presenter",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            (int)width, (int)height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
            return false;
        }

        gl_context = SDL_GL_CreateContext(window);
        if (!gl_context) {
            std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
            return false;
        }
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL2_Init();

        SDL_AudioSpec want{}, have;
        want.freq = (int)sample_rate;
        want.format = AUDIO_F32SYS;
        want.channels = 2;
        want.samples = 2048;
        audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
        if (audio_dev == 0) {
            std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << "\n";
        } else {
            SDL_PauseAudioDevice(audio_dev, 0);
        }

        audio_l.resize(sample_rate / fps);
        audio_r.resize(sample_rate / fps);

        glGenTextures(1, &viz_texture);
        glBindTexture(GL_TEXTURE_2D, viz_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        return true;
    }

    bool load_module(const std::string& path) {
        engine = std::make_unique<DivEngine>();
        engine->preInit(true);
        engine->setAudio(DIV_AUDIO_DUMMY);
        engine->setConf("audioRate", (int)sample_rate);
        engine->setConf("opl3CoreRender", 0);

        FILE* f = fopen(path.c_str(), "rb");
        if (!f) {
            std::cerr << "Cannot open file: " << path << "\n";
            return false;
        }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        if (len <= 0) { fclose(f); return false; }
        rewind(f);
        std::vector<unsigned char> buf(len);
        fread(buf.data(), 1, len, f);
        fclose(f);

        if (!engine->load(buf.data(), (size_t)len, path.c_str())) {
            std::cerr << "Failed to load module: " << engine->getLastError() << "\n";
            return false;
        }

        if (!engine->init()) {
            std::cerr << "Failed to initialize Furnace engine\n";
            return false;
        }

        engine->quitDispatch();
        engine->initDispatch(true);
        engine->renderSamplesP();

        int totalChans = engine->getTotalChannelCount();
        if (totalChans <= 0) {
            std::cerr << "Module has no channels\n";
            return false;
        }

        Config cfg = default_config();
        float scale = (float)width / 960.0f;
        if (scale > 1.0f) {
            cfg.piano_roll.key_length *= scale;
            cfg.piano_roll.key_thickness *= scale;
            cfg.piano_roll.divider_width = (uint32_t)(cfg.piano_roll.divider_width * scale);
            cfg.piano_roll.waveform_height = (uint32_t)(cfg.piano_roll.waveform_height * scale);
            cfg.piano_roll.oscilloscope_glow_thickness *= scale;
            cfg.piano_roll.oscilloscope_line_thickness *= scale;
        }

        viz = std::make_unique<Visualizer>(
            (size_t)totalChans, width, height, sample_rate,
            cfg.piano_roll, "assets/8x8_font.png");

        for (int i = 0; i < totalChans; ++i) {
            ChannelSettings cs;
            int chType = engine->getChannelType(i);
            unsigned int customCol = engine->curSubSong->chanColor[i];
            Color base_color;
            if (customCol != 0) {
                uint32_t c = customCol;
                base_color = Color((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF);
            } else if (!furnace_system_color(engine->song.sysOfChan[i], base_color)) {
                base_color = channel_type_color(chType);
            }
            cs.colors = color_variants(base_color);
            cs.chip = "Ch";
            cs.name = !engine->curSubSong->chanName[i].empty() ? engine->curSubSong->chanName[i].c_str() : ("Channel " + std::to_string(i + 1));
            viz->set_channel_settings((size_t)i, cs);
        }

        prev_freqs.resize(totalChans, 0.0);
        prev_notes.resize(totalChans, -1);

        if (audio_dev != 0) SDL_ClearQueuedAudio(audio_dev);
        engine->setOrder(0);
        engine->setLoops(-1);
        engine->play();
        playing = true;
        render_error = false;
        render_error_message.clear();
        return true;
    }

    void poll_channels(int samples_to_feed) {
        int totalChans = engine->getTotalChannelCount();
        static std::vector<int> last_notes;
        if ((int)last_notes.size() < totalChans) last_notes.resize(totalChans, -1);

        for (int i = 0; i < totalChans; ++i) {
            DivChannelState* cs = engine->getChanState(i);
            DivDispatchOscBuffer* osc = engine->getOscBuffer(i);
            SharedChannel* shared = engine->getDispatchChanState(i);

            ChannelState base_state;
            base_state.volume = 0.0f;
            base_state.amplitude = 0.0f;
            base_state.frequency = 0.0;
            base_state.timbre = 0;
            base_state.balance = 0.5f;
            base_state.edge = false;

            bool key_hit = false;
            int visual_note = -1;

            if (cs) {
                int volMax = cs->volMax >> 8;
                if (volMax <= 0) volMax = 15;
                int vol = shared ? shared->outVol : (cs->volume >> 8);
                if (vol < 0) vol = 0;

                bool playing = shared ? (shared->active && vol > 0) : (cs->note >= 0 && cs->note < 180 && vol > 0);
                base_state.volume = playing ? std::min((float)vol / (float)volMax, 1.0f) * 15.0f : 0.0f;

                size_t timbre = shared && shared->ins >= 0 ? (size_t)shared->ins : 0;
                timbre += macro_timbre(engine->getMacroInt(i));
                base_state.timbre = timbre;

                if (playing) {
                    double note_pos = state_note_pos(engine->song, shared, cs, visual_note);
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

            double prev_freq = prev_freqs[i];
            int prev_note = prev_notes[i];
            bool interpolate = prev_note == visual_note && prev_freq > 0.0 && base_state.frequency > 0.0;

            for (int j = 0; j < samples_to_feed; ++j) {
                ChannelState state = base_state;
                state.amplitude = amplitudes[j];
                state.edge = (j == 0 && key_hit);
                if (interpolate && samples_to_feed > 1) {
                    double t = (double)j / (double)(samples_to_feed - 1);
                    state.frequency = prev_freq + (base_state.frequency - prev_freq) * t;
                }
                viz->consume_state((size_t)i, state);
            }
            prev_freqs[i] = base_state.frequency;
            prev_notes[i] = visual_note;
        }
    }

    void run() {
        bool running = true;
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                ImGui_ImplSDL2_ProcessEvent(&e);
                if (e.type == SDL_QUIT) running = false;
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == SDL_GetWindowID(window)) {
                    running = false;
                }
                if (e.type == SDL_DROPFILE) {
                    pending_path = e.drop.file;
                    need_load = true;
                    SDL_free(e.drop.file);
                }
            }

            if (need_load) {
                need_load = false;
                load_module(pending_path);
                std::strncpy(file_path, pending_path.c_str(), sizeof(file_path) - 1);
                file_path[sizeof(file_path) - 1] = '\0';
            }

            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            width = (uint32_t)w;
            height = (uint32_t)h;

            ImGui_ImplOpenGL2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Main menu
            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                        // Simple file open via input text for now
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Exit")) running = false;
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            // Control panel
            ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeightWithSpacing()));
            ImGui::SetNextWindowSize(ImVec2((float)width, 140));
            ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

            ImGui::InputText("File", file_path, sizeof(file_path));
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                pending_path = file_path;
                need_load = true;
            }

            ImGui::InputText("Output", output_path, sizeof(output_path));
            ImGui::InputInt("Loop count", &render_loops);
            if (render_loops < 0) render_loops = 0;
            ImGui::InputFloat("Fade seconds", &render_fade_seconds, 0.5f, 1.0f, "%.2f");
            if (render_fade_seconds < 0.0f) render_fade_seconds = 0.0f;
            ImGui::Checkbox("Hide unused channels", &render_hide_unused);

            if (render_error && !render_error_message.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", render_error_message.c_str());
            } else if (render_in_progress) {
                ImGui::Text("Rendering to %s...", output_path);
            }

            ImGui::SameLine();
            if (ImGui::Button(playing ? "Pause" : "Play")) {
                playing = !playing;
                if (engine) {
                    if (playing) engine->play(); else engine->stop();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop")) {
                playing = false;
                if (engine) {
                    engine->stop();
                    engine->setOrder(0);
                    engine->play();
                    engine->stop();
                }
                if (audio_dev != 0) SDL_ClearQueuedAudio(audio_dev);
            }
            ImGui::SameLine();
            if (ImGui::Button(render_in_progress ? "Rendering..." : "Render") && !render_in_progress) {
                start_render();
            }
            ImGui::End();

            // Visualizer window
            float viz_y = ImGui::GetFrameHeightWithSpacing() + 140;

            ImGui::SetNextWindowPos(ImVec2(0, viz_y));
            ImGui::SetNextWindowSize(ImVec2((float)width, (float)height - viz_y));
            ImGui::Begin("Visualizer", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

            if (engine && playing && engine->isPlaying()) {
                size_t buf_size = audio_l.size();
                float* outBuf[2] = {audio_l.data(), audio_r.data()};
                engine->nextBuf(nullptr, outBuf, 0, 2, (unsigned int)buf_size);
                poll_channels((int)buf_size);
                viz->draw();

                // Queue audio
                if (audio_dev != 0) {
                    std::vector<float> interleaved(buf_size * 2);
                    for (size_t i = 0; i < buf_size; ++i) {
                        interleaved[i * 2 + 0] = audio_l[i];
                        interleaved[i * 2 + 1] = audio_r[i];
                    }
                    SDL_QueueAudio(audio_dev, interleaved.data(), (Uint32)(interleaved.size() * sizeof(float)));
                }

                const uint8_t* pixels = viz->get_canvas_buffer();
                glBindTexture(GL_TEXTURE_2D, viz_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)viz->canvas_width(), (GLsizei)viz->canvas_height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            }

            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::Image((ImTextureID)(intptr_t)viz_texture, avail);
            ImGui::End();

            // Rendering
            ImGui::Render();
            glViewport(0, 0, (int)width, (int)height);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);

            // Simple frame rate limiting
            SDL_Delay(16);
        }
    }

    void start_render() {
        if (render_in_progress) return;
        if (file_path[0] == '\0' || output_path[0] == '\0') return;

        RenderOptions opts;
        opts.input_path = file_path;
        opts.output_path = output_path;
        opts.width = 1920;
        opts.height = 1080;
        opts.fps = fps;
        opts.sample_rate = sample_rate;
        opts.video_codec = "libx264";
        opts.audio_codec = "aac";
        opts.stop_condition = "loops:" + std::to_string(std::max(render_loops, 0));
        opts.fadeout_length = (uint32_t)std::max(0, (int)std::lround(render_fade_seconds * (float)fps));
        opts.config = default_config();
        opts.hide_unused = render_hide_unused;

        render_error = false;
        render_error_message.clear();
        render_in_progress = true;

        if (render_thread.joinable()) {
            render_thread.join();
        }

        render_thread = std::thread([this, opts]() {
            try {
                Renderer renderer(opts);
                if (!renderer.init()) throw std::runtime_error("Renderer initialization failed");
                if (!renderer.render()) throw std::runtime_error("Render failed");
                renderer.finish();
            } catch (const std::exception& e) {
                render_error = true;
                render_error_message = e.what();
            }
            render_in_progress = false;
        });
    }

    void shutdown() {
        if (render_thread.joinable()) {
            render_thread.join();
        }
        if (audio_dev != 0) {
            SDL_CloseAudioDevice(audio_dev);
            audio_dev = 0;
        }
        if (viz_texture != 0) {
            glDeleteTextures(1, &viz_texture);
            viz_texture = 0;
        }
        ImGui_ImplOpenGL2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        if (gl_context) {
            SDL_GL_DeleteContext(gl_context);
            gl_context = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        SDL_Quit();
    }
};

Gui::Gui() : impl(std::make_unique<Impl>()) {}
Gui::~Gui() = default;

bool Gui::init() { return impl->init(); }
void Gui::run() { impl->run(); }
void Gui::shutdown() { impl->shutdown(); }

} // namespace fp
