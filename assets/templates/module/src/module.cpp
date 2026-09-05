#include <SDK/archetyped/FractalSDK.h>
#include <SDK/archetyped/IKernel.h>
#include <iostream>

#ifdef _WIN32
    #define FRACTAL_EXPORT extern "C" __declspec(dllexport)
#else
    #define FRACTAL_EXPORT extern "C" __attribute__((visibility("default")))
#endif

FRACTAL_EXPORT void ModuleMain(IKernel* kernel) {
    FractalSDK::SDK::Initialize(kernel);

    std::cout << "[{{MODULE_NAME}}] ModuleMain" << std::endl;
}
