#!/bin/sh

# ============================================================
# LVGD486 - Build BusyBox 1.00
# Script: 17-build-busybox.sh
# ============================================================

set -e

# Constants
SOURCE_DIR="/home/user/sources/busybox"
CONFIG_FILE="/home/user/doc/busybox-1.0-config/distrodev/.config"
MAKE_EXECUTABLE="/usr/local/bin/make"
BUSYBOX_VERSION="1.00"

# ------------------------------------------------------------
# Verify prerequisites
# ------------------------------------------------------------

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run this script as root."
    exit 1
fi

if [ ! -f "${SOURCE_DIR}/Makefile" ]; then
    echo "Error: BusyBox source not found at ${SOURCE_DIR}"
    exit 1
fi

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: BusyBox configuration not found at ${CONFIG_FILE}"
    exit 1
fi

if [ ! -x "$MAKE_EXECUTABLE" ]; then
    echo "Error: GNU Make not found at ${MAKE_EXECUTABLE}"
    exit 1
fi

# ------------------------------------------------------------
# Install BusyBox configuration
# ------------------------------------------------------------

echo "Installing BusyBox configuration..."
cp "$CONFIG_FILE" "${SOURCE_DIR}/.config"

cd "$SOURCE_DIR"

# ------------------------------------------------------------
# Patch gzip.c build_bl_tree declaration for compiler
# compatibility
# ------------------------------------------------------------

GZIP_SOURCE="${SOURCE_DIR}/archival/gzip.c"

if [ ! -f "$GZIP_SOURCE" ]; then
    echo "Error: gzip.c not found at ${GZIP_SOURCE}"
    exit 1
fi

echo "Applying gzip.c compatibility patch..."

if grep -q '^static const int build_bl_tree()' "$GZIP_SOURCE"; then
    sed -i \
        's/^static const int build_bl_tree()/static int build_bl_tree(void)/' \
        "$GZIP_SOURCE"
fi

if grep -q '^static const int build_bl_tree()' "$GZIP_SOURCE"; then
    echo "Error: gzip.c compatibility patch failed."
    exit 1
fi

if ! grep -q '^static int build_bl_tree(void)' "$GZIP_SOURCE"; then
    echo "Error: Expected build_bl_tree declaration not found."
    echo "Inspect ${GZIP_SOURCE} before rebuilding."
    exit 1
fi

echo "gzip.c compatibility patch verified."

# ------------------------------------------------------------
# Verify BusyBox 1.00 NFS mount-data argument handling
#
# nfsmount() receives pointers to two different values:
#   extra_opts   - textual mount options
#   string_flags - binary NFS mount-data structure
#
# mount_one() passes its fifth argument to do_mount(), which
# passes that argument to the mount() system call.
#
# Correct call:
#   Argument 5: string_flags (binary NFS mount data)
#   Argument 8: extra_opts (textual mtab options)
#
# Restore the correct argument order if a previous patch
# changed it.
# ------------------------------------------------------------

MOUNT_SOURCE="${SOURCE_DIR}/util-linux/mount.c"

if [ ! -f "$MOUNT_SOURCE" ]; then
    echo "Error: mount.c not found at ${MOUNT_SOURCE}"
    exit 1
fi

echo "Verifying NFS mount-data argument handling..."

# Restore argument 5 if the previous patch changed it.
sed -i \
    's/(device, directory, filesystemType, flags, extra_opts,/(device, directory, filesystemType, flags, string_flags,/' \
    "$MOUNT_SOURCE"

# Restore argument 8 if the previous patch changed it.
sed -i \
    's/useMtab, fakeIt, string_flags, TRUE, all)) {/useMtab, fakeIt, extra_opts, TRUE, all)) {/' \
    "$MOUNT_SOURCE"

# Verify argument 5.
if ! grep -Fq \
    '(device, directory, filesystemType, flags, string_flags,' \
    "$MOUNT_SOURCE"; then
    echo "Error: NFS mount-data argument verification failed."
    echo "Expected string_flags as the fifth mount_one() argument."
    exit 1
fi

# Verify argument 8.
if ! grep -Fq \
    'useMtab, fakeIt, extra_opts, TRUE, all)) {' \
    "$MOUNT_SOURCE"; then
    echo "Error: NFS mtab argument verification failed."
    echo "Expected extra_opts as the eighth mount_one() argument."
    exit 1
fi

echo "NFS mount-data arguments verified."

# ------------------------------------------------------------
# Build BusyBox
# ------------------------------------------------------------

echo "Building BusyBox ${BUSYBOX_VERSION}..."
"$MAKE_EXECUTABLE" -j1

# ------------------------------------------------------------
# Verify build output
# ------------------------------------------------------------

if [ ! -x "${SOURCE_DIR}/busybox" ]; then
    echo "Error: BusyBox build finished without producing the executable."
    exit 1
fi

echo "BusyBox ${BUSYBOX_VERSION} built successfully."
echo "Executable: ${SOURCE_DIR}/busybox"
