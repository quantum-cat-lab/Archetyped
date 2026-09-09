#include "engine/core/Bootstrap.h"
#include "engine/core/FractalKernel.h"
#include <chrono>
#include <thread>
int main() {
    Bootstrap::instance().init();
    bool running = true;
    while (running) {
        Bootstrap::instance().tick();
        if (Bootstrap::instance().shouldExit()) running = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    Bootstrap::instance().shutdown();
    return 0;
}
