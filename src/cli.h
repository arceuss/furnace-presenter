#pragma once

#include "renderer.h"
#include <string>

namespace fp {

struct ParsedArgs {
    bool valid = false;
    bool show_help = false;
    bool gui_mode = false;
    RenderOptions render_options;
    std::string error_message;
};

ParsedArgs parse_cli(int argc, char** argv);

} // namespace fp
