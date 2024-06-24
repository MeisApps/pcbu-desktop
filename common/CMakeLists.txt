cmake_minimum_required(VERSION 3.22)
project(pcbu_common)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)
find_package(OpenSSL 3 REQUIRED)

add_compile_definitions(TARGET_ARCH="${TARGET_ARCH}")
if(WIN32)
    add_compile_definitions(WINDOWS)
    set(PLATFORM_LIBS
            crypt32
            Ws2_32
            Iphlpapi
            Sensapi
            Netapi32
            Wtsapi32
            Bthprops
            ole32
    )
    set(PLATFORM_SRC
            src/platform/PlatformHelper.Win.cpp
            src/platform/BluetoothHelper.Win.cpp
            src/utils/RegistryUtils.cpp
            src/utils/RegistryUtils.h
    )
elseif(APPLE)
    add_compile_definitions(APPLE)
    enable_language(OBJCXX)
    set(PLATFORM_LIBS
            "-framework CoreServices"
            "-framework SystemConfiguration"
            "-framework IOKit"
            "-framework Carbon"
            "-framework IOBluetooth"
    )
    set(PLATFORM_SRC
            src/platform/PlatformHelper.Mac.cpp
            src/platform/BluetoothHelper.Mac.mm
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libxcrypt IMPORTED_TARGET REQUIRED libxcrypt)

    add_compile_definitions(LINUX HAVE_CRYPT_H)
    set(PLATFORM_LIBS
            PkgConfig::libxcrypt
            bluetooth
    )
    set(PLATFORM_SRC
            src/platform/PlatformHelper.Linux.cpp
            src/platform/BluetoothHelper.Linux.cpp
    )
endif()

add_library(pcbu_common
        src/connection/BaseUnlockServer.h
        src/connection/BaseUnlockServer.cpp
        src/connection/clients/TCPUnlockClient.h
        src/connection/clients/TCPUnlockClient.cpp
        src/connection/clients/BTUnlockClient.h
        src/connection/clients/BTUnlockClient.cpp
        src/connection/servers/PairingServer.cpp
        src/connection/servers/PairingServer.h
        src/connection/servers/PairingStructs.h
        src/handler/I18n.h
        src/handler/I18n.cpp
        src/handler/KeyScanner.h
        src/handler/KeyScanner.cpp
        src/handler/UnlockHandler.h
        src/handler/UnlockHandler.cpp
        src/platform/PlatformHelper.h
        src/platform/NetworkHelper.cpp
        src/platform/NetworkHelper.h
        src/platform/BluetoothHelper.h
        src/shell/Shell.cpp
        src/shell/Shell.h
        src/storage/AppSettings.cpp
        src/storage/AppSettings.h
        src/storage/LoggingSystem.cpp
        src/storage/LoggingSystem.h
        src/storage/PairedDevicesStorage.cpp
        src/storage/PairedDevicesStorage.h
        src/storage/PairingMethod.h
        src/utils/CryptUtils.cpp
        src/utils/CryptUtils.h
        src/utils/StringUtils.cpp
        src/utils/StringUtils.h
        src/utils/Utils.cpp
        src/utils/Utils.h
        ${PLATFORM_SRC}
        src/utils/AppInfo.cpp
        src/utils/AppInfo.h
)
target_include_directories(pcbu_common PUBLIC
        src
        ../deps/headers
        ${Boost_INCLUDE_DIRS}
)
target_link_libraries(pcbu_common PUBLIC
        Threads::Threads
        spdlog::spdlog
        OpenSSL::Crypto
        OpenSSL::SSL
        Boost::system
        Boost::filesystem
        ${PLATFORM_LIBS}
)
