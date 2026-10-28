#!/bin/sh

# ============================================================
# LVGD486 - Extract Source Packages
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

DOWNLOAD_DIR="/home/user/download"
SOURCES_DIR="/home/user/sources"

KERNEL_VERSION="2.4.37.11"
BUSYBOX_VERSION="1.00"
MAKE_VERSION="3.81"
BINUTILS_VERSION="2.15"
GCC4_VERSION="4.0.2"
GCC5_VERSION="5.4.0"
GDB_VERSION="6.3"
GLIBC_VERSION="2.3.2"
NFS_UTILS_VERSION="1.0.6"
PORTMAP_VERSION="5"
PORTMAP_DEBIAN_VERSION="5-9"
NCURSES_VERSION="5.4"
BASH_VERSION="2.05b"
KBD_VERSION="1.12"
MC_VERSION="4.6.1"

# ============================================================
# Archive filenames
# ============================================================

KERNEL_ARCHIVE="linux-${KERNEL_VERSION}.tar.bz2"
BUSYBOX_ARCHIVE="busybox-${BUSYBOX_VERSION}.tar.bz2"
MAKE_ARCHIVE="make-${MAKE_VERSION}.tar.gz"
BINUTILS_ARCHIVE="binutils-${BINUTILS_VERSION}.tar.bz2"
GCC4_ARCHIVE="gcc-${GCC4_VERSION}.tar.bz2"
GCC5_ARCHIVE="gcc-${GCC5_VERSION}.tar.bz2"
GDB_ARCHIVE="gdb_${GDB_VERSION}.orig.tar.gz"
GLIBC_ARCHIVE="glibc-${GLIBC_VERSION}.tar.gz"
GLIBC_THREADS_ARCHIVE="glibc-linuxthreads-${GLIBC_VERSION}.tar.gz"
NFS_UTILS_ARCHIVE="nfs-utils_${NFS_UTILS_VERSION}.orig.tar.gz"
PORTMAP_ARCHIVE="portmap_${PORTMAP_VERSION}.orig.tar.gz"
PORTMAP_PATCH="portmap_${PORTMAP_DEBIAN_VERSION}.diff.gz"
NCURSES_ARCHIVE="ncurses_${NCURSES_VERSION}.orig.tar.gz"
BASH_ARCHIVE="bash_${BASH_VERSION}-2.orig.tar.gz"
BASH_NESTED_ARCHIVE="bash-${BASH_VERSION}.tar.gz"
KBD_ARCHIVE="kbd_${KBD_VERSION}.orig.tar.gz"
MC_ARCHIVE="mc_${MC_VERSION}.orig.tar.gz"

# ============================================================
# Initialization
# ============================================================

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

[ -d "$DOWNLOAD_DIR" ] ||
    fail "Download directory not found: $DOWNLOAD_DIR"

mkdir -p "$SOURCES_DIR"

cd "$DOWNLOAD_DIR" ||
    fail "Cannot enter download directory: $DOWNLOAD_DIR"

# ============================================================
# Validate required source archives and patches
# ============================================================

for archive in \
    "$KERNEL_ARCHIVE" \
    "$BUSYBOX_ARCHIVE" \
    "$MAKE_ARCHIVE" \
    "$BINUTILS_ARCHIVE" \
    "$GCC4_ARCHIVE" \
    "$GCC5_ARCHIVE" \
    "$GDB_ARCHIVE" \
    "$GLIBC_ARCHIVE" \
    "$GLIBC_THREADS_ARCHIVE" \
    "$NFS_UTILS_ARCHIVE" \
    "$PORTMAP_ARCHIVE" \
    "$PORTMAP_PATCH" \
    "$NCURSES_ARCHIVE" \
    "$BASH_ARCHIVE" \
    "$KBD_ARCHIVE" \
    "$MC_ARCHIVE"
do
    [ -s "$archive" ] ||
        fail "Required archive missing or empty: $DOWNLOAD_DIR/$archive"
done

# ============================================================
# Extract source archives
# ============================================================

echo "Extracting Linux kernel..."
tar -xjf "$KERNEL_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting BusyBox..."
tar -xjf "$BUSYBOX_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting GNU Make..."
tar -xzf "$MAKE_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting GNU Binutils..."
tar -xjf "$BINUTILS_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting GCC 4..."
tar -xjf "$GCC4_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting GCC 5..."
tar -xjf "$GCC5_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting GDB..."
tar -xzf "$GDB_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting glibc..."
tar -xzf "$GLIBC_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting NFS utilities..."
tar -xzf "$NFS_UTILS_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting ncurses..."
tar -xzf "$NCURSES_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting portmap..."
tar -xzf "$PORTMAP_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting KBD..."
tar -xzf "$KBD_ARCHIVE" -C "$SOURCES_DIR"

echo "Extracting Midnight Commander..."
tar -xzf "$MC_ARCHIVE" -C "$SOURCES_DIR"

# ============================================================
# Extract Bash source
# ============================================================

echo "Extracting Bash original archive..."

BASH_OUTER_DIR="$SOURCES_DIR/bash-${BASH_VERSION}-2.orig"
BASH_NESTED_PATH="$SOURCES_DIR/$BASH_NESTED_ARCHIVE"

rm -rf "$BASH_OUTER_DIR"
rm -rf "$SOURCES_DIR/bash"
rm -rf "$SOURCES_DIR/bash-${BASH_VERSION}"
rm -f "$BASH_NESTED_PATH"

tar -xzf "$BASH_ARCHIVE" -C "$SOURCES_DIR"

[ -s "$BASH_OUTER_DIR/$BASH_NESTED_ARCHIVE" ] ||
    fail "Nested Bash archive not found: $BASH_OUTER_DIR/$BASH_NESTED_ARCHIVE"

mv "$BASH_OUTER_DIR/$BASH_NESTED_ARCHIVE" "$BASH_NESTED_PATH"
rm -rf "$BASH_OUTER_DIR"

tar -xzf "$BASH_NESTED_PATH" -C "$SOURCES_DIR"
rm -f "$BASH_NESTED_PATH"

[ -d "$SOURCES_DIR/bash-${BASH_VERSION}" ] ||
    fail "Extracted Bash source directory not found: $SOURCES_DIR/bash-${BASH_VERSION}"

mv "$SOURCES_DIR/bash-${BASH_VERSION}" "$SOURCES_DIR/bash"

[ -f "$SOURCES_DIR/bash/configure" ] ||
    fail "Bash configure not found at $SOURCES_DIR/bash/configure"

echo "Bash source extracted to $SOURCES_DIR/bash"

# ============================================================
# Rename source directories to stable project paths
# ============================================================

mv "$SOURCES_DIR/linux-${KERNEL_VERSION}" "$SOURCES_DIR/linux"
mv "$SOURCES_DIR/busybox-${BUSYBOX_VERSION}" "$SOURCES_DIR/busybox"
mv "$SOURCES_DIR/make-${MAKE_VERSION}" "$SOURCES_DIR/make"
mv "$SOURCES_DIR/binutils-${BINUTILS_VERSION}" "$SOURCES_DIR/binutils"
mv "$SOURCES_DIR/gdb-${GDB_VERSION}" "$SOURCES_DIR/gdb"
mv "$SOURCES_DIR/glibc-${GLIBC_VERSION}" "$SOURCES_DIR/glibc"
mv "$SOURCES_DIR/nfs-utils-${NFS_UTILS_VERSION}.orig" "$SOURCES_DIR/nfs-utils"

# ============================================================
# Rename ncurses source directory
# ============================================================

NCURSES_SOURCE_DIR=$(
    find "$SOURCES_DIR" -maxdepth 1 -type d \
        -name 'ncurses*' ! -name 'ncurses' | head -n 1
)

if [ -n "$NCURSES_SOURCE_DIR" ]; then
    mv "$NCURSES_SOURCE_DIR" "$SOURCES_DIR/ncurses"
else
    fail "Could not locate extracted ncurses source directory."
fi

# ============================================================
# Rename portmap source directory
# ============================================================

PORTMAP_SOURCE_DIR=$(
    find "$SOURCES_DIR" -maxdepth 1 -type d \
        -name 'portmap*' ! -name 'portmap' | head -n 1
)

if [ -n "$PORTMAP_SOURCE_DIR" ]; then
    mv "$PORTMAP_SOURCE_DIR" "$SOURCES_DIR/portmap"
else
    fail "Could not locate extracted portmap source directory."
fi

# ============================================================
# Rename KBD source directory
# ============================================================

KBD_SOURCE_DIR=$(
    find "$SOURCES_DIR" -maxdepth 1 -type d \
        -name 'kbd*' ! -name 'kbd' | head -n 1
)

if [ -n "$KBD_SOURCE_DIR" ]; then
    mv "$KBD_SOURCE_DIR" "$SOURCES_DIR/kbd"
else
    fail "Could not locate extracted KBD source directory."
fi

# ============================================================
# Rename Midnight Commander source directory
# ============================================================

MC_SOURCE_DIR=$(
    find "$SOURCES_DIR" -maxdepth 1 -type d \
        -name 'mc-*' ! -name 'mc' | head -n 1
)

if [ -n "$MC_SOURCE_DIR" ]; then
    mv "$MC_SOURCE_DIR" "$SOURCES_DIR/mc"
else
    fail "Could not locate extracted Midnight Commander source directory."
fi

# ============================================================
# Apply Debian portmap patch
# ============================================================

echo "Applying Debian portmap patch..."

(
    cd "$SOURCES_DIR/portmap"
    gunzip -c "$DOWNLOAD_DIR/$PORTMAP_PATCH" | patch -p1
)

# ============================================================
# Extract LinuxThreads into the glibc source tree
# ============================================================

echo "Extracting LinuxThreads..."

tar -xzf "$GLIBC_THREADS_ARCHIVE" -C "$SOURCES_DIR/glibc"

# ============================================================
# Final report
# ============================================================

echo
echo "All packages extracted successfully."
echo "Source directory: $SOURCES_DIR"
echo
echo "Bash source: $SOURCES_DIR/bash"
echo "Bash configure: $SOURCES_DIR/bash/configure"
echo "Portmap source: $SOURCES_DIR/portmap"
echo "Portmap Debian patch applied: $PORTMAP_PATCH"
echo "KBD source: $SOURCES_DIR/kbd"
echo "Midnight Commander source: $SOURCES_DIR/mc"