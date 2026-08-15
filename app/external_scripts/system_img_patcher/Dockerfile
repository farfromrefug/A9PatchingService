# Runtime for patch_system_img.py.
#
# The patcher loop-mounts an ext4 image, sets numeric ownership and writes SELinux labels into
# security.selinux extended attributes. macOS can do none of those, so on non-Linux hosts
# patch_system_img.sh runs the patcher in this image instead. Needs --privileged for /dev/loop.
FROM debian:bookworm-slim

# mount/umount come from util-linux in the base image.
#   e2fsprogs  e2fsck, resize2fs
#   attr       setfattr
#   zipalign   apk alignment before signing
#   apksigner  signing patched system apks with platform.pk8
#   jre        apktool
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      python3 \
      default-jre-headless \
      e2fsprogs \
      attr \
      zip \
      zipalign \
      apksigner \
 && rm -rf /var/lib/apt/lists/*

# The repo ships its own apktool jar plus the launcher script, both bind-mounted at /work.
ENV PATH="/work:${PATH}"

WORKDIR /work
