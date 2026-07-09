list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_MESSAGE_LOG_LEVEL "ERROR" CACHE STRING "" FORCE)

set(CMAKE_PLATFORM_INFO_DIR "${CMAKE_CURRENT_BINARY_DIR}/CMakePlatformTmp")
include(CMakeDetermineSystem)
include(CMakeDetermineCompiler)
if(CMAKE_PLATFORM_INFO_DIR AND EXISTS "${CMAKE_PLATFORM_INFO_DIR}")
    file(REMOVE_RECURSE "${CMAKE_PLATFORM_INFO_DIR}")
endif()

include(DetectArchitecture)
include(FindQtInstallation)

if(WIN32)
    set(TARGET_OS "win")
elseif(APPLE)
    set(TARGET_OS "mac")
elseif(UNIX)
    set(TARGET_OS "linux")
else()
    message(FATAL_ERROR "Unsupported OS.")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${TARGET_OS}")

detect_architecture(TARGET_ARCH)
execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${TARGET_ARCH}")

find_qt_installation(QT_BASE_DIR)
execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${QT_BASE_DIR}")
