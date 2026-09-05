cmake_minimum_required(VERSION 3.16)
project(Archetyped VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" OFF)
option(ENABLE_JVM "Enable JVMHost (JNI) support" ON)
option(ENABLE_CLR "Enable ClrHost (.NET) support" OFF)

add_library(engine STATIC
    src/Engine.cpp
    src/engine/core/Bootstrap.cpp
    src/engine/core/FractalKernel.cpp
    src/engine/FURCMD/FURCMD.cpp
    src/engine/core/KernelCommands.cpp
    src/engine/components/workScheduler/WSFactory.cpp
    src/engine/components/workScheduler/workScheduler.cpp
    src/engine/components/ECS/Entity/entity.cpp
    src/engine/components/ECS/Component/ComponentInstance.cpp
    src/engine/components/ECS/Component/ComponentRegistry.cpp
    src/engine/components/ECS/Component/VaultInstance.cpp
    src/engine/components/ECS/Component/CMFactory.cpp
    src/engine/components/Event/EventInstance.cpp
    src/engine/components/Event/EBFactory.cpp
    src/engine/components/ModuleLoader/ModuleFactory.cpp
    src/engine/components/ContainerLoader/ContainerLoader.cpp
    src/engine/components/SmartScheduler/SSDomains.cpp
    src/engine/components/SystemExecution/SEDomains.cpp
    src/engine/components/Clock/Clock.cpp
    src/engine/components/Clock/ClockDomain.cpp
    src/engine/components/DataBase/SQLFactory.cpp
    src/engine/components/DataBase/SqlInstance.cpp
    src/engine/components/vfs/VFSDomains.cpp
    src/engine/components/vfs/VFS.cpp
    src/engine/components/vfs/VFSWatch.cpp
    src/engine/components/JvmHost/JvmHost.cpp
    src/engine/components/JvmScheduler/JvmScheduler.cpp
    src/engine/components/JvmRegistry/JvmFactory.cpp
    src/engine/components/BridgeRegistry/BridgeRegistry.cpp
    src/engine/components/SchemaRegistry/SchemaInstance.cpp
    src/engine/components/SchemaRegistry/SchemaFactory.cpp
    src/engine/components/SchemaRegistry/SchemaRegistryCMD.cpp
    include/sqlite/sqlite3.c
)

target_include_directories(engine
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PUBLIC ${PROJECT_SOURCE_DIR}/include/sqlite
    PUBLIC ${PROJECT_SOURCE_DIR}/src/engine
    PUBLIC ${PROJECT_SOURCE_DIR}/src
)

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# JVM (JNI) — optional, for JvmHost
if(ENABLE_JVM)
    find_package(JNI)
    if(JNI_FOUND)
        message(STATUS "JNI found: ${JNI_INCLUDE_DIRS} -> ${JNI_LIBRARIES}")
    else()
        message(STATUS "JNI not found — using vendored headers from include/jdk (standalone build)")
    endif()
endif()

if(MSVC)
    target_compile_options(engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(archetyped src/main.cpp)
target_link_libraries(archetyped PRIVATE engine)

# Toolchain: Forge
add_executable(forge
    src/tools/forge/main.cpp
    src/tools/forge/App.cpp
    src/tools/forge/LineEditor.cpp
    src/tools/forge/Workspace.cpp
    src/tools/forge/Subprocess.cpp
    src/tools/forge/CLIUtils.cpp
    src/tools/forge/SHA256.cpp
    src/tools/forge/commands/CmdModule.cpp
    src/tools/forge/commands/CmdContainer.cpp
    src/tools/forge/commands/CmdBuild.cpp
    src/tools/forge/commands/CmdSDK.cpp
    src/tools/forge/commands/CmdUtil.cpp
    src/tools/forge/commands/CmdGenSchema.cpp
    src/tools/forge/commands/CmdInit.cpp
)
target_include_directories(forge PRIVATE ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/forge)
target_link_libraries(forge PRIVATE engine)

if(UNIX AND NOT APPLE)
    target_link_options(archetyped PRIVATE -rdynamic)
    target_link_libraries(archetyped PRIVATE dl)
    target_link_libraries(engine PRIVATE dl)
endif()

if(ENABLE_JVM AND JNI_FOUND)
    target_include_directories(engine PRIVATE ${JNI_INCLUDE_DIRS})
    target_link_libraries(engine PRIVATE ${JNI_LIBRARIES})
    target_compile_definitions(engine PRIVATE ARCHETYPED_ENABLE_JVM=1)
elseif(ENABLE_JVM)
    if(WIN32)
        target_include_directories(engine PRIVATE ${PROJECT_SOURCE_DIR}/include/jdk ${PROJECT_SOURCE_DIR}/include/jdk/win32)
    else()
        target_include_directories(engine PRIVATE ${PROJECT_SOURCE_DIR}/include/jdk ${PROJECT_SOURCE_DIR}/include/jdk/linux)
    endif()
    target_compile_definitions(engine PRIVATE ARCHETYPED_ENABLE_JVM=1 ARCHETYPED_JVM_VENDORED=1)
endif()

if(ENABLE_JVM)
    target_sources(engine PRIVATE src/engine/components/KotlinRuntime/KotlinFactory.cpp)
endif()

if(ENABLE_CLR)
    target_sources(engine PRIVATE src/engine/components/ClrHost/ClrHost.cpp src/engine/components/ClrScheduler/ClrScheduler.cpp src/engine/components/ClrRegistry/ClrFactory.cpp src/engine/components/CSharpRuntime/CSharpFactory.cpp)
    target_include_directories(engine PRIVATE $ENV{HOME}/.dotnet/packs/Microsoft.NETCore.App.Host.linux-x64/9.0.19/runtimes/linux-x64/native)
    target_compile_definitions(engine PRIVATE ARCHETYPED_ENABLE_CLR=1)
endif()


if(ENABLE_JVM)
add_executable(test_jvm src/tests/test_jvm.cpp)
target_link_libraries(test_jvm PRIVATE engine)
if(ENABLE_JVM AND JNI_FOUND)
    target_include_directories(test_jvm PRIVATE ${JNI_INCLUDE_DIRS})
    target_link_libraries(test_jvm PRIVATE ${JNI_LIBRARIES})
    target_compile_definitions(test_jvm PRIVATE ARCHETYPED_ENABLE_JVM=1)
elseif(ENABLE_JVM)
    if(WIN32)
        target_include_directories(test_jvm PRIVATE ${PROJECT_SOURCE_DIR}/include/jdk ${PROJECT_SOURCE_DIR}/include/jdk/win32)
    else()
        target_include_directories(test_jvm PRIVATE ${PROJECT_SOURCE_DIR}/include/jdk ${PROJECT_SOURCE_DIR}/include/jdk/linux)
    endif()
    target_compile_definitions(test_jvm PRIVATE ARCHETYPED_ENABLE_JVM=1 ARCHETYPED_JVM_VENDORED=1)
endif()
endif()

if(ENABLE_CLR)
add_executable(test_clr src/tests/test_clr.cpp)
target_link_libraries(test_clr PRIVATE engine)
target_include_directories(test_clr PRIVATE $ENV{HOME}/.dotnet/packs/Microsoft.NETCore.App.Host.linux-x64/9.0.19/runtimes/linux-x64/native)
target_compile_definitions(test_clr PRIVATE ARCHETYPED_ENABLE_CLR=1)
endif()

install(TARGETS archetyped engine)
