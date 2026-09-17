cmake_minimum_required(VERSION 3.22)

project(win_pulseunlock)
set(CMAKE_CXX_STANDARD 23)

add_compile_definitions(_WIN32 WIN32 SAMPLEV2CREDENTIALPROVIDER_EXPORTS _WINDOWS WINDOWS _USRDLL UNICODE _UNICODE)
add_library(win_pulseunlock SHARED
        src/win_pulseunlock.def
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
target_include_directories(win_pulseunlock PUBLIC common-cpp/src)
target_link_libraries(win_pulseunlock PUBLIC Credui Netapi32 Shlwapi Secur32 Wtsapi32 pcbu_common)
add_custom_command(TARGET win_pulseunlock
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:win_pulseunlock> ${CMAKE_SOURCE_DIR}/desktop/res/natives/${TARGET_OS}/${TARGET_ARCH})
