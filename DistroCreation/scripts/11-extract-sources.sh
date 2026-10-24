#!/bin/sh

# ============================================================
# LVGD486 - Download and Extract Source Packages
# Script: 00-download-packages.sh
# ============================================================

set -e

# Constants
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

# Create directories
mkdir -p "$SOURCES_DIR"
cd "$DOWNLOAD_DIR" || exit 1

# Extract sources
tar -xjf "$KERNEL_ARCHIVE" -C "$SOURCES_DIR"
tar -xjf "$BUSYBOX_ARCHIVE" -C "$SOURCES_DIR"
tar -xzf "$MAKE_ARCHIVE" -C "$SOURCES_DIR"
tar -xjf "$BINUTILS_ARCHIVE" -C "$SOURCES_DIR"
tar -xjf "$GCC4_ARCHIVE" -C "$SOURCES_DIR"
tar -xjf "$GCC5_ARCHIVE" -C "$SOURCES_DIR"
tar -xzf "$GDB_ARCHIVE" -C "$SOURCES_DIR"
tar -xzf "$GLIBC_ARCHIVE" -C "$SOURCES_DIR"
tar -xzf "$NFS_UTILS_ARCHIVE" -C "$SOURCES_DIR"

# Extract portmap source
tar -xzf "$PORTMAP_ARCHIVE" -C "$SOURCES_DIR"

# Rename source directories to stable project paths
mv "$SOURCES_DIR/linux-${KERNEL_VERSION}" "$SOURCES_DIR/linux"
mv "$SOURCES_DIR/busybox-${BUSYBOX_VERSION}" "$SOURCES_DIR/busybox"
mv "$SOURCES_DIR/make-${MAKE_VERSION}" "$SOURCES_DIR/make"
mv "$SOURCES_DIR/binutils-${BINUTILS_VERSION}" "$SOURCES_DIR/binutils"
mv "$SOURCES_DIR/gdb-${GDB_VERSION}" "$SOURCES_DIR/gdb"
mv "$SOURCES_DIR/glibc-${GLIBC_VERSION}" "$SOURCES_DIR/glibc"
mv "$SOURCES_DIR/nfs-utils-${NFS_UTILS_VERSION}.orig" "$SOURCES_DIR/nfs-utils"

# Rename portmap directory to stable project path.
# The original archive's top-level directory name may vary.
PORTMAP_SOURCE_DIR=$(find "$SOURCES_DIR" -maxdepth 1 -type d -name 'portmap*' ! -name 'portmap' | head -n 1)

if [ -n "$PORTMAP_SOURCE_DIR" ]; then
    mv "$PORTMAP_SOURCE_DIR" "$SOURCES_DIR/portmap"
else
    echo "ERROR: Could not locate extracted portmap source directory."
    exit 1
fi

# Apply Debian packaging patch
if [ -f "$PORTMAP_PATCH" ]; then
    gunzip -c "$PORTMAP_PATCH" | (
        cd "$SOURCES_DIR/portmap"
        patch -p1
    )
else
    echo "WARNING: Debian portmap patch not found: $PORTMAP_PATCH"
fi

# Extract LinuxThreads into the glibc source tree
tar -xzf "$GLIBC_THREADS_ARCHIVE" -C "$SOURCES_DIR/glibc"

echo "All packages extracted successfully."