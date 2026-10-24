#!/bin/sh
set -e

if [ "$(id -u)" -ne 0 ]; then
  echo "Please run as root, e.g. 'sudo ./install.sh'." >&2
  exit 1
fi

this_dir="$(cd "$(dirname "$0")" && pwd)"
install -m 755 "$this_dir/pulseunlock" /usr/bin/pulseunlock

echo "Installed /usr/bin/pulseunlock."
echo "Next, run 'sudo pulseunlock install' (or 'sudo pulseunlock reinstall' when updating) and then 'sudo pulseunlock pair'."
