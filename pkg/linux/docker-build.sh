#!/usr/bin/env bash
set -e
cd ..
mkdir build || true
cd ..

if [[ "$ARCH" == "x64" ]]; then
  docker container prune -f
  docker build -t pcbu_builder -f pkg/linux/Dockerfile_x64 --build-arg QT="${QT_VERSION}" .
  docker run --name pcbu_build_container --cap-add=SYS_ADMIN --security-opt apparmor:unconfined --device /dev/fuse pcbu_builder
  docker cp pcbu_build_container:/project/pkg/build/PCBioUnlock.AppImage ./pkg/build/PCBioUnlock-x64.AppImage
  docker container rm pcbu_build_container
elif [[ "$ARCH" == "arm64" ]]; then
  docker container prune -f
  docker build -t pcbu_builder -f pkg/linux/Dockerfile_arm64 --build-arg QT="${QT_VERSION}" .
  docker run --name pcbu_build_container --cap-add=SYS_ADMIN --security-opt apparmor:unconfined --device /dev/fuse pcbu_builder
  docker cp pcbu_build_container:/project/pkg/build/PCBioUnlock.AppImage ./pkg/build/PCBioUnlock-arm64.AppImage
  docker container rm pcbu_build_container
else
  echo 'Invalid architecture.'
  exit 1
fi

chmod 777 ./pkg/build/PCBioUnlock*.AppImage
chmod +x ./pkg/build/PCBioUnlock*.AppImage
