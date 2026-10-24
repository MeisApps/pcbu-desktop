#!/usr/bin/env bash
set -e
cd ..
mkdir build || true
cd ..

if [[ "$TARGET" == "cli" ]]; then
  if [[ "$ARCH" != "x64" && "$ARCH" != "arm64" ]]; then
    echo 'Invalid architecture.'
    exit 1
  fi
  docker container prune -f
  docker build -t pcbu_cli_builder -f pkg/linux/Dockerfile_cli --build-arg ARCH="$ARCH" .
  docker run --name pcbu_cli_build_container pcbu_cli_builder
  docker cp pcbu_cli_build_container:/project/pkg/build/PulseUnlock-CLI-"$ARCH".tar.gz ./pkg/build/PulseUnlock-CLI-"$ARCH".tar.gz
  docker container rm pcbu_cli_build_container
  exit 0
fi

if [[ "$ARCH" == "x64" ]]; then
  docker container prune -f
  docker build -t pcbu_builder -f pkg/linux/Dockerfile_x64 --build-arg QT="${QT_VERSION}" .
  docker run --name pcbu_build_container --cap-add=SYS_ADMIN --security-opt apparmor:unconfined --device /dev/fuse pcbu_builder
  docker cp pcbu_build_container:/project/pkg/build/PulseUnlock.AppImage ./pkg/build/PulseUnlock-x64.AppImage
  docker container rm pcbu_build_container
elif [[ "$ARCH" == "arm64" ]]; then
  docker container prune -f
  docker build -t pcbu_builder -f pkg/linux/Dockerfile_arm64 --build-arg QT="${QT_VERSION}" .
  docker run --name pcbu_build_container --cap-add=SYS_ADMIN --security-opt apparmor:unconfined --device /dev/fuse pcbu_builder
  docker cp pcbu_build_container:/project/pkg/build/PulseUnlock.AppImage ./pkg/build/PulseUnlock-arm64.AppImage
  docker container rm pcbu_build_container
else
  echo 'Invalid architecture.'
  exit 1
fi

chmod 777 ./pkg/build/PulseUnlock*.AppImage
chmod +x ./pkg/build/PulseUnlock*.AppImage
