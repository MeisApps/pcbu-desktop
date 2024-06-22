#!/bin/sh
set -e
curr_uid=$(id -u)
if which pkexec >/dev/null 2>&1 && [ "$curr_uid" -ne 0 ]; then
  pkexec env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY WAYLAND_DISPLAY=$WAYLAND_DISPLAY XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR APPIMAGE=$APPIMAGE PKEXEC_UID=$curr_uid $APPIMAGE
else
  this_dir="$(readlink -f "$(dirname "$0")")"
  exec "$this_dir/usr/bin/pcbu_desktop"
fi
