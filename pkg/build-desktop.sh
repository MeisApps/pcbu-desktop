#!/usr/bin/env bash
set -e
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  PLATFORM=linux
elif [[ "$OSTYPE" == "darwin"* ]]; then
  PLATFORM=mac
elif [[ "$OSTYPE" == "msys" ]]; then
  PLATFORM=win
else
  echo 'Could not detect OS.'
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
if [ -z "$QT_BASE_DIR" ]; then
  echo 'QT_BASE_DIR is not set.'
  exit 1
fi

# Build
mkdir build || true
cd build
if [[ "$PLATFORM" == "win" ]]; then
  cmake ../../ -DCMAKE_BUILD_TYPE=Release -DTARGET_ARCH=$ARCH -DQT_BASE_DIR=$QT_BASE_DIR -G "Visual Studio 17 2022" -A $VS_ARCH -DCMAKE_GENERATOR_PLATFORM=$VS_ARCH -DMSVC_STATIC_LINK=1
  cmake --build . --target "win-pcbiounlock" --config Release -- /maxcpucount:3

  rm -Rf ./*
  cmake ../../ -DCMAKE_BUILD_TYPE=Release -DTARGET_ARCH=$ARCH -DQT_BASE_DIR=$QT_BASE_DIR -G "Visual Studio 17 2022" -A $VS_ARCH -DCMAKE_GENERATOR_PLATFORM=$VS_ARCH
  cmake --build . --target "pcbu_desktop" --config Release -- /maxcpucount:3
else
  if [[ "$DOCKER_BUILD" == "1" ]]; then
    cmake ../../ -DCMAKE_BUILD_TYPE=Release -DTARGET_ARCH=$ARCH -DQT_BASE_DIR=$QT_BASE_DIR -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11
  else
    cmake ../../ -DCMAKE_BUILD_TYPE=Release -DTARGET_ARCH=$ARCH -DQT_BASE_DIR=$QT_BASE_DIR
  fi
  cmake --build . --target "pcbu_auth" --config Release -- -j3
  cmake --build . --target "pam_pcbiounlock" --config Release -- -j3
  cmake --build . --target "pcbu_desktop" --config Release -- -j3
fi

# Package
if [[ "$PLATFORM" == "win" ]]; then
  mkdir -p installer_dir || true
  cp desktop/Release/pcbu_desktop.exe installer_dir/
  "/c/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/mt.exe" -manifest ../win/requireAdmin.manifest -outputresource:installer_dir/pcbu_desktop.exe

  "/c/Qt/6.8.0/msvc2022_64/bin/windeployqt" --qmldir ../../desktop/qml installer_dir/pcbu_desktop.exe
  if [[ "$ARCH" == "arm64" ]]; then # ToDo: Workaround for no windeployqt on arm64
    find "installer_dir/" -type f -name "*.dll" | while read -r dll_file; do
      file_name=$(basename "$dll_file")
      if [[ "$file_name" != "D3Dcompiler_47.dll" ]] && [[ "$file_name" != "opengl32sw.dll" ]]; then
        replacement_file=$(find "/c/Qt/6.8.0/msvc2022_arm64/" -type f -name "$file_name" | head -n 1)
        if [ -f "$replacement_file" ]; then
          echo "Replacing $dll_file with $replacement_file"
          cp "$replacement_file" "$dll_file"
        else
          echo "No replacement found for $dll_file"
          exit 1
        fi
      fi
    done
    rm installer_dir/D3Dcompiler_47.dll
    rm installer_dir/opengl32sw.dll
  fi

  iscc ../win/installer.iss
  mv mysetup.exe PCBioUnlock-Setup-$ARCH.exe
elif [[ "$PLATFORM" == "linux" ]]; then
  wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$LINUX_ARCH.AppImage" && chmod +x ./linuxdeploy-$LINUX_ARCH.AppImage
  wget "https://github.com/darealshinji/linuxdeploy-plugin-checkrt/releases/download/continuous/linuxdeploy-plugin-checkrt.sh" && chmod +x ./linuxdeploy-plugin-checkrt.sh

  rm -Rf appimage_dir/ || true
  mkdir -p appimage_dir/usr/bin || true
  mkdir -p appimage_dir/usr/share/icons/hicolor/256x256/apps || true
  cp desktop/pcbu_desktop appimage_dir/usr/bin/
  cp ../linux/run-app.sh appimage_dir/usr/bin/
  cp ../../desktop/res/icons/icon.png appimage_dir/usr/share/icons/hicolor/256x256/apps/PCBioUnlock.png
  chmod +x appimage_dir/usr/bin/run-app.sh

  export QML_SOURCES_PATHS=../../desktop/qml
  export EXTRA_QT_MODULES=svg;
  if [[ "$ARCH" == "arm64" ]] && [[ "$DOCKER_BUILD" == "1" ]]; then
    qemu-aarch64-static ./linuxdeploy-$LINUX_ARCH.AppImage --appdir appimage_dir --plugin checkrt --desktop-file ../linux/PCBioUnlock.desktop
    wget "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$LINUX_ARCH.AppImage" && chmod +x ./linuxdeploy-plugin-qt-$LINUX_ARCH.AppImage
    qemu-aarch64-static ./linuxdeploy-plugin-qt-$LINUX_ARCH.AppImage --appdir appimage_dir
    rm ./linuxdeploy-plugin-qt-$LINUX_ARCH.AppImage
    qemu-aarch64-static ./linuxdeploy-$LINUX_ARCH.AppImage --appdir appimage_dir --plugin checkrt --output appimage --desktop-file ../linux/PCBioUnlock.desktop
  else
    wget "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$LINUX_ARCH.AppImage" && chmod +x ./linuxdeploy-plugin-qt-$LINUX_ARCH.AppImage
    ./linuxdeploy-$LINUX_ARCH.AppImage --appdir appimage_dir --plugin qt --plugin checkrt --output appimage --desktop-file ../linux/PCBioUnlock.desktop
  fi
  mv PC_Bio_Unlock*.AppImage PCBioUnlock.AppImage
  chmod +x PCBioUnlock.AppImage
elif [[ "$PLATFORM" == "mac" ]]; then
  "$QT_BASE_DIR/macos/bin/macdeployqt" desktop/pcbu_desktop.app -qmldir=../../desktop/qml
  find "desktop/pcbu_desktop.app" -type f -perm +111 | while read -r file; do
    echo "Signing $file"
    codesign --force --deep -s - "$file"
  done

  rm -Rf dmg_dir/ || true
  mkdir -p dmg_dir/ || true
  cp -R desktop/pcbu_desktop.app dmg_dir/PCBioUnlock.app
  ln -s /Applications dmg_dir/Applications

  if [[ "$CI_BUILD" == "1" ]]; then
    echo "Killing XProtect..."; sudo pkill -9 XProtect >/dev/null || true;
    echo "Waiting for XProtect..."; while pgrep XProtect; do sleep 3; done;
  fi
  hdiutil create -volname "PC Bio Unlock" -srcfolder dmg_dir/ -ov -format UDZO ./PCBioUnlock-$ARCH.dmg
fi
