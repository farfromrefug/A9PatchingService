#!/bin/bash
# Packages a Magisk module that replaces just /system/bin/a9_eink_server, so the daemon can be
# updated without reflashing the system image.
#
# The init service that starts it lives in /system/etc/init/vndk.rc, which is baked into the
# patched image. init parses that file long before Magisk mounts anything, so a module can not
# add the service itself - it can only swap the binary that the existing service execs. That is
# enough as long as the device already runs a patched image.
#
# Usage: make_magisk_module.sh --daemon <a9_eink_server> --out <module.zip> [--version <name>]
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

daemon=""
out=""
version="dev"

while [ $# -gt 0 ]; do
    case "$1" in
        --daemon)  daemon="$2"; shift 2 ;;
        --out)     out="$2";    shift 2 ;;
        --version) version="$2"; shift 2 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

if [ -z "$daemon" ] || [ -z "$out" ]; then
    echo "Usage: $0 --daemon <a9_eink_server> --out <module.zip> [--version <name>]" >&2
    exit 1
fi

if [ ! -f "$daemon" ]; then
    echo "Daemon binary not found: $daemon" >&2
    exit 1
fi

out="$(cd "$(dirname "$out")" && pwd)/$(basename "$out")"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Reuse the Magisk installer scripts from the full module, but NOT its module.prop: that module
# is "hisense_a9_augmented" and carries the SystemUI/framework/services patches. Installing this
# one under the same id would replace it and silently drop those.
cp -R "$script_dir/magisk_module/META-INF" "$work/"

mkdir -p "$work/system/bin"
cp "$daemon" "$work/system/bin/a9_eink_server"
chmod 0755 "$work/system/bin/a9_eink_server"

cat > "$work/module.prop" <<EOF
id=hisense_a9_eink_daemon
name=Hisense A9 e-ink daemon
version=$version
versionCode=$(date +%Y%m%d%H%M)
author=farfromrefug
description=Replaces /system/bin/a9_eink_server. Requires an already patched system image.
EOF

cat > "$work/customize.sh" <<'EOF'
set -eu

ui_print ""
ui_print "  Hisense A9 e-ink daemon"
ui_print ""

if [ ! -f /system/etc/init/vndk.rc ] || ! grep -q "a9_eink_server" /system/etc/init/vndk.rc; then
  ui_print "[-] No a9_eink_server service found in /system/etc/init/vndk.rc."
  ui_print "[-] This module only swaps the binary, it cannot register the service."
  ui_print "[-] Flash a patched system image first."
  abort "[-] Aborting."
fi

# init transitions to the phhsu_daemon domain based on this label. Without it the service
# fails to start and the e-ink controls go dead.
set_perm "$MODPATH/system/bin/a9_eink_server" 0 2000 0755 u:object_r:phhsu_exec:s0

ui_print "[+] Installed. Reboot to start the new daemon."

set +eu
EOF

(cd "$work" && zip -qr "$out" .)
echo "Wrote $out"
