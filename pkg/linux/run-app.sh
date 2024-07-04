#!/bin/sh
set -e
curr_uid=$(id -u)

ssl_cert_paths="
/etc/ssl/certs/ca-certificates.crt
/etc/pki/tls/certs/ca-bundle.crt
/etc/ssl/ca-bundle.pem
/etc/pki/tls/cacert.pem
/etc/ssl/certs
/usr/share/ca-certs/.prebuilt-store/
/system/etc/security/cacerts
/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem
/etc/ssl/cert.pem
"
for path in $ssl_cert_paths; do
  if [ -e "$path" ]; then
    export SSL_CERT_FILE="$path"
    break
  fi
done

if which pkexec >/dev/null 2>&1 && [ "$curr_uid" -ne 0 ]; then
  pkexec env DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" WAYLAND_DISPLAY="$WAYLAND_DISPLAY" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" APPIMAGE="$APPIMAGE" PKEXEC_UID="$curr_uid" "$APPIMAGE"
else
  this_dir="$(readlink -f "$(dirname "$0")")"
  exec "$this_dir/usr/bin/pcbu_desktop"
fi
