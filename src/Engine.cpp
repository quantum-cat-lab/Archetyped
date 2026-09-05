#include "Engine/Engine.hpp"
#include <iostream>
#include "FURCMD/FURCMD.h"
#include "engine/core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include "components/workScheduler/WSFactory.h"



namespace arche {

void Engine::run() {
    std::cout << "Archetyped running (C++23)\n";

}
void Engine::init() {
    std::cout << "Archetyped initializing...\n";
    FractalKernel::instance().init();
}
 
}