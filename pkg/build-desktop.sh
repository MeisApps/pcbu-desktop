#!/usr/bin/env bash
set -e

# Detect parameters
if [ -z "$PLATFORM" ] || [ -z "$ARCH" ] || [ -z "$QT_BASE_DIR" ]; then
    cmake_build_helper_output=$(cmake -P ../cmake/BuildHelper.cmake)
fi
if [ -z "$PLATFORM" ]; then
    PLATFORM=$(echo "$cmake_build_helper_output" | sed -n '1p' | tr -d '\r' | xargs)
fi
if [ -z "$ARCH" ]; then
    ARCH=$(echo "$cmake_build_helper_output" | sed -n '2p' | tr -d '\r' | xargs)
fi
if [ -z "$QT_BASE_DIR" ]; then
    QT_BASE_DIR=$(echo "$cmake_build_helper_output" | sed -n '3p' | tr -d '\r' | xargs)
fi

echo "OS: $PLATFORM"
echo "Arch: $ARCH"
echo "Qt: $QT_BASE_DIR"

if [[ "$PLATFORM" != "win" && "$PLATFORM" != "linux" && "$PLATFORM" != "mac" ]]; then
  echo 'Invalid OS.'
  exit 1
fi
if [[ "$ARCH" == "x64" ]]; then
  VS_ARCH=x64
  LINUX_ARCH=x86_64
elif [[ "$ARCH" == "arm64" ]]; then
  VS_ARCH=ARM64
  LINUX_ARCH=aarch64
else
  echo 'Invalid architecture.'
  exit 1
fi
if [ ! -d "$QT_BASE_DIR" ]; then
  echo 'Invalid QT directory.'
  exit 1
fi

# macOS code signing
MAC_SIGN_IDENTITY="${MAC_SIGN_IDENTITY:--}"
mac_sign() {
  if [[ "$MAC_SIGN_IDENTITY" == "-" ]]; then
    codesign --force -s - "$@"
  else
    codesign --force --options runtime --timestamp -s "$MAC_SIGN_IDENTITY" "$@"
  fi
}

mac_notarize() {
  local notary_auth=(--key "$MAC_NOTARY_KEY_PATH" --key-id "$MAC_NOTARY_KEY_ID" --issuer "$MAC_NOTARY_ISSUER_ID")
  local notary_result
  notary_result=$(xcrun notarytool submit "$1" "${notary_auth[@]}" --wait --timeout 1h --output-format json)
  echo "$notary_result"
  if [[ "$(plutil -extract status raw -o - - <<< "$notary_result")" != "Accepted" ]]; then
    xcrun notarytool log "$(plutil -extract id raw -o - - <<< "$notary_result")" "${notary_auth[@]}"
    exit 1
  fi
}

# Find Windows SDK
if [[ "$PLATFORM" == "win" ]]; then
  WIN_QT_PATH="$(cygpath -u "$QT_BASE_DIR")/msvc2022_64"
  WIN_QT_PATH_ARM64="$(cygpath -u "$QT_BASE_DIR")/msvc2022_arm64"

  win_sdk_ver_dir=$(find "/c/Program Files (x86)/Windows Kits/10/bin" \
      -mindepth 1 -maxdepth 1 \
      -type d \
      -regex '.*/10\.0\.[0-9]+\.[0-9]+' \
      -printf '%p\n' 2>/dev/null | sort -V | tail -n 1)
  if [ -n "$win_sdk_ver_dir" ] && [ -f "${win_sdk_ver_dir}/x64/mt.exe" ]; then
      WIN_MT_PATH="${win_sdk_ver_dir}/x64/mt.exe"
  else
      echo "No Windows SDK found."
      exit 1
  fi
fi

# Build
BUILD_CORES=4
mkdir build || true
cd build
if [[ "$PLATFORM" == "win" ]]; then
  cmake ../../ -DCMAKE_BUILD_TYPE=Release -DTARGET_ARCH="$ARCH" -DQT_BASE_DIR="$QT_BASE_DIR" -G "Visual Studio 18 2026" -A "$VS_ARCH" -DCMAKE_GENERATOR_PLATFORM="$VS_ARCH" -DMSVC_STATIC_LINK=1
  cmake --build . --target "win_pulseunlock" --config Release -- /maxcpucount:"$BUILD_CORES"

  rm -Rf ./*
  cmake ../../ -DCMAKE_BUILD_TYPE=Release -DTARGET_ARCH="$ARCH" -DQT_BASE_DIR="$QT_BASE_DIR" -G "Visual Studio 18 2026" -A "$VS_ARCH" -DCMAKE_GENERATOR_PLATFORM="$VS_ARCH"
  cmake --build . --target "pcbu_desktop" --config Release -- /maxcpucount:"$BUILD_CORES"
else
  cmake ../../ -DCMAKE_BUILD_TYPE=Release -DTARGET_ARCH="$ARCH" -DQT_BASE_DIR="$QT_BASE_DIR"
  cmake --build . --target "pam_pulseunlock" --config Release -- -j"$BUILD_CORES"
  cmake --build . --target "pcbu_auth" --config Release -- -j"$BUILD_CORES"
  cmake --build . --target "pcbu_ssh_askpass" --config Release -- -j"$BUILD_CORES"
  if [[ "$PLATFORM" == "mac" ]]; then
    for native in pcbu_auth pcbu_ssh_askpass pam_pulseunlock.dylib; do
      mac_sign ../../desktop/res/natives/mac/"$ARCH"/"$native"
    done
  fi
  cmake --build . --target "pcbu_desktop" --config Release -- -j"$BUILD_CORES"
fi

# Package
if [[ "$PLATFORM" == "win" ]]; then
  mkdir -p installer_dir || true
  cp desktop/Release/pcbu_desktop.exe installer_dir/
  cp desktop/Release/pcbu_elevator.exe installer_dir/
  "$WIN_MT_PATH" -manifest ../win/requireAdmin.manifest -outputresource:installer_dir/pcbu_desktop.exe
  "$WIN_QT_PATH/bin/windeployqt" --qmldir ../../desktop/qml installer_dir/pcbu_desktop.exe
  if [[ "$ARCH" == "arm64" ]]; then # ToDo: Workaround for no windeployqt on arm64
    find "installer_dir/" -type f -name "*.dll" | while read -r dll_file; do
      file_name=$(basename "$dll_file")
      replacement_file=$(find "$WIN_QT_PATH_ARM64" -type f -name "$file_name" | head -n 1)
      if [ -f "$replacement_file" ]; then
        echo "Replacing $dll_file with $replacement_file"
        cp "$replacement_file" "$dll_file"
      else
        echo "No replacement found for $dll_file"
      fi
    done
    rm installer_dir/D3Dcompiler_47.dll
    rm installer_dir/opengl32sw.dll
  fi

  APP_VERSION="$(cat version.txt)" iscc ../win/installer.iss
  mv mysetup.exe PulseUnlock-Setup-"$ARCH".exe
elif [[ "$PLATFORM" == "linux" ]]; then
  wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$LINUX_ARCH.AppImage" && chmod +x ./linuxdeploy-"$LINUX_ARCH".AppImage
  wget "https://github.com/darealshinji/linuxdeploy-plugin-checkrt/releases/download/continuous/linuxdeploy-plugin-checkrt.sh" && chmod +x ./linuxdeploy-plugin-checkrt.sh

  rm -Rf appimage_dir/ || true
  mkdir -p appimage_dir/usr/bin || true
  mkdir -p appimage_dir/usr/share/icons/hicolor/256x256/apps || true
  cp desktop/pcbu_desktop appimage_dir/usr/bin/
  cp desktop/pcbu_elevator appimage_dir/usr/bin/
  cp ../linux/run-app.sh appimage_dir/usr/bin/
  cp ../../desktop/res/icons/icon.png appimage_dir/usr/share/icons/hicolor/256x256/apps/PulseUnlock.png
  chmod +x appimage_dir/usr/bin/run-app.sh

  export QML_SOURCES_PATHS="../../desktop/qml"
  export EXTRA_QT_MODULES="svg;waylandcompositor"
  export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
  ./linuxdeploy-"$LINUX_ARCH".AppImage --appdir appimage_dir --plugin checkrt --desktop-file ../linux/PulseUnlock.desktop
  wget "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$LINUX_ARCH.AppImage" && chmod +x ./linuxdeploy-plugin-qt-"$LINUX_ARCH".AppImage
  ./linuxdeploy-plugin-qt-"$LINUX_ARCH".AppImage --appdir appimage_dir
  rm ./linuxdeploy-plugin-qt-"$LINUX_ARCH".AppImage
  LINUXDEPLOY_OUTPUT_VERSION="$(cat version.txt)" ./linuxdeploy-"$LINUX_ARCH".AppImage --appdir appimage_dir --plugin checkrt --output appimage --desktop-file ../linux/PulseUnlock.desktop
  mv PulseUnlock*.AppImage PulseUnlock.AppImage
  chmod +x PulseUnlock.AppImage
elif [[ "$PLATFORM" == "mac" ]]; then
  "$QT_BASE_DIR/macos/bin/macdeployqt" desktop/pcbu_desktop.app -qmldir=../../desktop/qml \
    -executable=desktop/pcbu_desktop.app/Contents/MacOS/pcbu_elevator
  for binary in desktop/pcbu_desktop.app/Contents/MacOS/* natives/pcbu-auth/pcbu_auth natives/pcbu-ssh-askpass/pcbu_ssh_askpass natives/pam_pulseunlock/pam_pulseunlock.dylib; do
    if [ ! -f "$binary" ]; then
      echo "$binary is missing."
      exit 1
    fi
    if otool -L "$binary" | grep -E '^[[:space:]]+/(opt|usr/local)/'; then
      echo "$binary links non-bundled libraries."
      exit 1
    fi
  done
  find "desktop/pcbu_desktop.app/Contents" -type f -name "*.dylib" | while read -r file; do
    echo "Signing $file"
    mac_sign "$file"
  done
  for framework in desktop/pcbu_desktop.app/Contents/Frameworks/*.framework; do
    echo "Signing $framework"
    mac_sign "$framework"
  done
  mac_sign desktop/pcbu_desktop.app/Contents/MacOS/pcbu_elevator
  mac_sign --entitlements ../mac/entitlements.plist desktop/pcbu_desktop.app

  rm -Rf dmg_dir/ || true
  mkdir -p dmg_dir/ || true
  ditto desktop/pcbu_desktop.app dmg_dir/PulseUnlock.app
  codesign --verify --deep --strict --verbose=2 dmg_dir/PulseUnlock.app
  ln -s /Applications dmg_dir/Applications

  if [[ "$CI_BUILD" == "1" ]]; then
    echo "Killing XProtect..."
    sudo pkill -9 XProtect >/dev/null || true
    echo "Waiting for XProtect..."
    while pgrep XProtect; do sleep 3; done
  fi
  hdiutil create -volname "PulseUnlock" -srcfolder dmg_dir/ -ov -format UDZO ./PulseUnlock-"$ARCH".dmg

  if [[ "$MAC_SIGN_IDENTITY" != "-" ]]; then
    codesign --force --timestamp -s "$MAC_SIGN_IDENTITY" ./PulseUnlock-"$ARCH".dmg
  fi
  if [[ -n "$MAC_NOTARY_KEY_PATH" ]]; then
    mac_notarize ./PulseUnlock-"$ARCH".dmg
    xcrun stapler staple ./PulseUnlock-"$ARCH".dmg
    xcrun stapler validate ./PulseUnlock-"$ARCH".dmg
    spctl --assess --type open --context context:primary-signature -v ./PulseUnlock-"$ARCH".dmg
  fi
fi
