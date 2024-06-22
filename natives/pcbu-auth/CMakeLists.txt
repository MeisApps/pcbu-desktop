cmake_minimum_required(VERSION 3.22)

project(pcbu_auth)
set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_definitions(WINDOWS)
elseif(APPLE)
    add_compile_definitions(APPLE)
elseif(UNIX)
    add_compile_definitions(LINUX)
endif()

add_executable(pcbu_auth
        src/main.cpp
)
target_include_directories(pcbu_auth PRIVATE common-cpp/src ../../deps/headers)
target_link_libraries(pcbu_auth PRIVATE pcbu_common)
add_custom_command(TARGET pcbu_auth
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pcbu_auth> ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH})
