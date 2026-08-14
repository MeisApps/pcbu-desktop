set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

# Workaround for MSVC arm64 __chkstk compilation bug
set(VCPKG_C_FLAGS "/Oy-")
set(VCPKG_CXX_FLAGS "/Oy-")
