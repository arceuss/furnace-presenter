#pragma once

#include <string>
#include <memory>

namespace fp {

class Gui {
public:
    Gui();
    ~Gui();

    bool init();
    void run();
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace fp
