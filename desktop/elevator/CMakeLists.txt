cmake_minimum_required(VERSION 3.22)
project(pcbu_elevator)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_definitions(WINDOWS _WIN32_WINNT=0x0A00)
elseif(APPLE)
    add_compile_definitions(APPLE)
elseif(UNIX)
    add_compile_definitions(LINUX)
endif()

include_directories(src)
add_executable(pcbu_elevator
        src/main.cpp
)
target_link_libraries(pcbu_elevator PRIVATE
        pcbu_common
)

if(WIN32)
    add_version_resource(pcbu_elevator "PulseUnlock Elevator")
endif()
