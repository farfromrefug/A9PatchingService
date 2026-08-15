#!/bin/bash
# Usage:
#   Linux:  sudo bash patch_system_img.sh /path/to/system.img
#   macOS:  bash patch_system_img.sh /path/to/system.img     (runs in Docker, no sudo)
#
# The patcher needs to loop-mount ext4 and write security.selinux xattrs, so on anything that
# is not Linux it re-runs itself inside the container built from ./Dockerfile.
#
# Override the choice with PATCHER_DOCKER=1 (always containerise) or PATCHER_DOCKER=0 (never).
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -ne 1 ]; then
    echo "Usage: $0 /path/to/system.img" >&2
    exit 1
fi

if [ ! -f "$1" ]; then
    echo "File not found: $1" >&2
    exit 1
fi

image_path="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"

use_docker="${PATCHER_DOCKER:-}"
if [ -z "$use_docker" ]; then
    if [ "$(uname -s)" = "Linux" ]; then use_docker=0; else use_docker=1; fi
fi

if [ "$use_docker" = "1" ]; then
    if ! command -v docker >/dev/null 2>&1; then
        echo "docker is required to patch images on $(uname -s), but was not found in PATH." >&2
        echo "Install Docker Desktop or OrbStack, or run this script on Linux." >&2
        exit 1
    fi

    image_tag="a9-system-img-patcher"
    echo "Building $image_tag..."
    docker build -q -t "$image_tag" "$script_dir" >/dev/null

    # The script directory is the working directory and also where system_patched.img and
    # hisensea9_magisk_module.zip are written, so it is bind-mounted read-write. The source
    # image is only ever copied from, so it goes in read-only.
    echo "Running the patcher in $image_tag..."
    exec docker run --rm --privileged \
        -v "$script_dir:/work" \
        -v "$image_path:/input/system.img:ro" \
        -e "APK_TOOL=${APK_TOOL:-/work/apktool_2.11.0.jar}" \
        -e PATCHER_DOCKER=0 \
        "$image_tag" \
        bash /work/patch_system_img.sh /input/system.img
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root (use sudo)." >&2
    exit 1
fi

python="$(command -v python || command -v python3 || true)"
if [ -z "$python" ]; then
    echo "python not found in PATH." >&2
    exit 1
fi

cd "$script_dir"
"$python" patch_system_img.py "$image_path"
