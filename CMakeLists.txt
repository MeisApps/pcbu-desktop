cmake_minimum_required(VERSION 3.22)
project(pcbu_desktop_all)

if(NOT DEFINED TARGET_ARCH)
    set(TARGET_ARCH "arm64") # arm64
endif()

if(WIN32)
    set(TARGET_OS "win")
elseif(APPLE)
    set(TARGET_OS "mac")
elseif(UNIX)
    set(TARGET_OS "linux")
else()
    message(FATAL_ERROR "Unsupported OS.")
endif()

if(WIN32)
    if(NOT EXISTS "$ENV{VCPKG_ROOT}")
        message(FATAL_ERROR "VCPKG_ROOT does not exist.")
    endif()
    file(TO_CMAKE_PATH $ENV{VCPKG_ROOT} VCPKG_ROOT)
    message(STATUS "VCPKG_ROOT set to ${VCPKG_ROOT}")
    include("${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

    if(MSVC_STATIC_LINK)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(OPENSSL_MSVC_STATIC_RT TRUE)
    endif()
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(OpenSSL_ROOT "${VCPKG_ROOT}/installed/${TARGET_ARCH}-windows-static")
endif()

message("-- Building for arch '${TARGET_ARCH}' on '${TARGET_OS}'")

set(SPDLOG_BUILD_PIC ON CACHE BOOL "")
add_subdirectory(deps/spdlog)
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "")
add_subdirectory(deps/boost)
set(Boost_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/align/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/algorithm/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/asio/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/date_time/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/dll/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/exception/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/filesystem/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/interprocess/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/move/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/numeric/conversion/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/process/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/range/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/system/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/tokenizer/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/libs/type_index/include"
)

add_subdirectory(common)
add_subdirectory(desktop)
add_subdirectory(desktop/elevator)
add_dependencies(pcbu_desktop pcbu_elevator)
if(WIN32)
    add_subdirectory(natives/win-pcbiounlock)
    #add_dependencies(pcbu_desktop win-pcbiounlock)
elseif(UNIX)
    add_subdirectory(natives/pcbu-auth)
    add_subdirectory(natives/pam-pcbiounlock)
    add_dependencies(pcbu_desktop pcbu_auth pam_pcbiounlock)
endif()
