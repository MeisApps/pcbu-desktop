cmake_minimum_required(VERSION 3.22)

project(pcbu_ssh_askpass)
set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_definitions(WINDOWS)
elseif(APPLE)
    add_compile_definitions(APPLE)
elseif(UNIX)
    add_compile_definitions(LINUX)
endif()

add_executable(pcbu_ssh_askpass
        src/main.cpp
        src/RootGuard.cpp
        src/SshPrompt.cpp
        src/TtyPrompt.cpp
)
target_include_directories(pcbu_ssh_askpass PRIVATE src)
target_link_libraries(pcbu_ssh_askpass PRIVATE pcbu_common)
add_custom_command(TARGET pcbu_ssh_askpass
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pcbu_ssh_askpass> ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH})
