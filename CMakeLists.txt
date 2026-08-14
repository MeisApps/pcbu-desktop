cmake_minimum_required(VERSION 3.22)
project(pcbu_desktop_all)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DetectArchitecture)
include(FetchContent)

if(NOT DEFINED TARGET_ARCH)
    detect_architecture(TARGET_ARCH)
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

    if(MSVC_STATIC_LINK)
        set(PCBU_VCPKG_TRIPLET "${TARGET_ARCH}-windows-static")
    else()
        set(PCBU_VCPKG_TRIPLET "${TARGET_ARCH}-windows-static-md")
    endif()
    set(VCPKG_TARGET_TRIPLET "${PCBU_VCPKG_TRIPLET}" CACHE STRING "vcpkg target triplet" FORCE)
    message(STATUS "vcpkg triplet set to ${VCPKG_TARGET_TRIPLET}")

    if(NOT "${PCBU_VCPKG_TRIPLET}" STREQUAL "$CACHE{PCBU_VCPKG_TRIPLET_LAST}")
        foreach(_pcbu_var OPENSSL_INCLUDE_DIR OPENSSL_SSL_LIBRARY OPENSSL_CRYPTO_LIBRARY
                LIB_EAY_DEBUG LIB_EAY_RELEASE SSL_EAY_DEBUG SSL_EAY_RELEASE
                LIB_EAY_LIBRARY_DEBUG LIB_EAY_LIBRARY_RELEASE
                SSL_EAY_LIBRARY_DEBUG SSL_EAY_LIBRARY_RELEASE)
            unset(${_pcbu_var} CACHE)
        endforeach()
        set(PCBU_VCPKG_TRIPLET_LAST "${PCBU_VCPKG_TRIPLET}" CACHE INTERNAL "last configured vcpkg triplet")
    endif()

    include("${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

    if(MSVC_STATIC_LINK)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(OPENSSL_MSVC_STATIC_RT TRUE)
        set(BOOST_RUNTIME_LINK static)
    endif()
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

message("-- Building for arch '${TARGET_ARCH}' on '${TARGET_OS}'")

FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_Declare(
        cpp_httplib
        URL https://github.com/yhirose/cpp-httplib/raw/v0.49.0/httplib.h
        DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.17.0
)
FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.7z
        DOWNLOAD_EXTRACT_TIMESTAMP ON
)

message("-- Fetching json...")
FetchContent_MakeAvailable(json)

message("-- Fetching cpp-httplib...")
FetchContent_MakeAvailable(cpp_httplib)

message("-- Fetching boost...")
set(BOOST_INCLUDE_LIBRARIES asio filesystem process system CACHE STRING "")
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "")
FetchContent_MakeAvailable(Boost)
set_property(TARGET boost_process PROPERTY POSITION_INDEPENDENT_CODE ON)

message("-- Fetching spdlog...")
set(SPDLOG_BUILD_PIC ON CACHE BOOL "")
FetchContent_MakeAvailable(spdlog)

add_subdirectory(common)
add_subdirectory(desktop)
if(WIN32)
    add_subdirectory(natives/win-pcbiounlock)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_dependencies(pcbu_desktop win-pcbiounlock)
    endif()
elseif(UNIX)
    add_subdirectory(natives/pcbu-auth)
    add_subdirectory(natives/pam-pcbiounlock)
    add_dependencies(pcbu_desktop pcbu_auth pam_pcbiounlock)
endif()
