cmake_minimum_required(VERSION 3.22)

project(win-pcbiounlock)
set(CMAKE_CXX_STANDARD 23)

add_compile_definitions(_WIN32_WINNT=0x0A00 _WIN32 WIN32 SAMPLEV2CREDENTIALPROVIDER_EXPORTS _WINDOWS WINDOWS _USRDLL UNICODE _UNICODE)
add_library(win-pcbiounlock SHARED
        src/win-pcbiounlock.def
        src/resource.rc
        src/common.h
        src/guid.h
        src/guid.cpp
        src/Dll.h
        src/Dll.cpp
        src/helpers.h
        src/helpers.cpp
        src/CSampleProvider.h
        src/CSampleProvider.cpp
        src/CUnlockCredential.h
        src/CUnlockCredential.cpp
        src/CUnlockListener.h
        src/CUnlockListener.cpp
)
target_include_directories(win-pcbiounlock PUBLIC common-cpp/src ../../deps/headers)
target_link_libraries(win-pcbiounlock PUBLIC Credui Shlwapi Secur32 pcbu_common)
add_custom_command(TARGET win-pcbiounlock
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:win-pcbiounlock> ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH})
