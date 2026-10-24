cmake_minimum_required(VERSION 3.22)
project(pcbu_cli)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_definitions(WINDOWS _WIN32_WINNT=0x0A00)
elseif(APPLE)
    add_compile_definitions(APPLE)
elseif(UNIX)
    add_compile_definitions(LINUX)
endif()

add_executable(pcbu_cli
        src/main.cpp
        src/cli/Cli.cpp
        src/cli/Cli.h
        src/cli/CommandLine.cpp
        src/cli/CommandLine.h
        src/commands/DeviceCommands.cpp
        src/commands/DeviceCommands.h
        src/commands/InstallCommands.cpp
        src/commands/InstallCommands.h
        src/commands/PairingCommand.cpp
        src/commands/PairingCommand.h
        src/commands/SettingsCommands.cpp
        src/commands/SettingsCommands.h
        src/utils/TerminalUtils.cpp
        src/utils/TerminalUtils.h
)
target_include_directories(pcbu_cli PRIVATE src)
target_link_libraries(pcbu_cli PRIVATE pcbu_common_desktop)
set_target_properties(pcbu_cli PROPERTIES OUTPUT_NAME pulseunlock)
if(NOT PCBU_BUILD_GUI)
    target_compile_definitions(pcbu_cli PRIVATE STANDALONE_CLI)
endif()

if(WIN32)
    add_version_resource(pcbu_cli "PulseUnlock CLI")
endif()
