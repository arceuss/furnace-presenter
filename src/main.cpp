// Provide SDL_main since SDL2main is linked by furnace-engine
#include <string>
#include <iostream>

void reportError(std::string what) {
    std::cerr << "Furnace error: " << what << "\n";
}

#include "cli.h"
#include "renderer.h"
#include "gui.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    auto args = fp::parse_cli(argc, argv);
    if (!args.valid) {
        return 1;
    }
    if (args.show_help) {
        return 0;
    }

    if (args.gui_mode) {
        fp::Gui gui;
        if (!gui.init()) {
            std::cerr << "GUI initialization failed\n";
            return 1;
        }
        gui.run();
        gui.shutdown();
        return 0;
    }

    try {
        fp::Renderer renderer(args.render_options);
        if (!renderer.init()) {
            std::cerr << "Renderer initialization failed\n";
            return 1;
        }
        if (!renderer.render()) {
            std::cerr << "Render failed\n";
            return 1;
        }
        renderer.finish();
        std::cout << "Done.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

extern "C" int SDL_main(int argc, char** argv) {
    return main(argc, argv);
}
