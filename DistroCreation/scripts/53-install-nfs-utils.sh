#!/bin/sh

# ============================================================
# LVGD486 - Install Portmap and NFS Utilities
# Install built NFS utilities and portmap into the staging tree.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCE_DIR="/home/user/sources/nfs-utils"
PORTMAP_SOURCE_DIR="/home/user/sources/portmap"
DISTRODEV_ROOT="/home/user/distrodev"

NFS_SBIN_DIR="${DISTRODEV_ROOT}/usr/sbin"
NFS_BIN_DIR="${DISTRODEV_ROOT}/usr/bin"
SYSTEM_SBIN_DIR="${DISTRODEV_ROOT}/sbin"

NFS_VERSION="1.0.6"
PORTMAP_VERSION="5"

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

run()
{
    echo "RUN: $*"
    "$@" || fail "Command failed: $*"
}

# ============================================================
# Prerequisites
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Development staging root does not exist: $DISTRODEV_ROOT"

[ -d "$SOURCE_DIR" ] ||
    fail "NFS utilities source directory not found: $SOURCE_DIR"

[ -x "$SOURCE_DIR/utils/nfsd/nfsd" ] ||
    fail "Built NFS server executable not found."

[ -x "$SOURCE_DIR/utils/mountd/mountd" ] ||
    fail "Built NFS mount daemon not found."

[ -x "$SOURCE_DIR/utils/exportfs/exportfs" ] ||
    fail "Built NFS export utility not found."

[ -x "$PORTMAP_SOURCE_DIR/portmap" ] ||
    fail "Built portmap executable not found. Run 22-build-portmap.sh first."

echo "NFS utilities version: ${NFS_VERSION}"
echo "Portmap version: ${PORTMAP_VERSION}"
echo "NFS source: ${SOURCE_DIR}"
echo "Portmap source: ${PORTMAP_SOURCE_DIR}"
echo "Staging root: ${DISTRODEV_ROOT}"
echo

# ============================================================
# Create staging directories
# ============================================================

echo
echo "============================================================"
echo "Preparing staging directories"
echo "============================================================"

run mkdir -p "$NFS_SBIN_DIR"
run mkdir -p "$NFS_BIN_DIR"
run mkdir -p "$SYSTEM_SBIN_DIR"

# ============================================================
# Install portmap
# ============================================================

echo
echo "============================================================"
echo "Installing portmap"
echo "============================================================"

run install -m 755 \
    "$PORTMAP_SOURCE_DIR/portmap" \
    "$SYSTEM_SBIN_DIR/portmap"

# ============================================================
# Install NFS server utilities
# ============================================================

echo
echo "============================================================"
echo "Installing NFS server utilities"
echo "============================================================"

run install -m 755 \
    "$SOURCE_DIR/utils/nfsd/nfsd" \
    "$NFS_SBIN_DIR/nfsd"

run install -m 755 \
    "$SOURCE_DIR/utils/mountd/mountd" \
    "$NFS_SBIN_DIR/mountd"

run install -m 755 \
    "$SOURCE_DIR/utils/exportfs/exportfs" \
    "$NFS_SBIN_DIR/exportfs"

# ============================================================
# Install additional NFS utilities
# ============================================================

echo
echo "============================================================"
echo "Installing additional NFS utilities"
echo "============================================================"

install_optional()
{
    SOURCE_FILE="$1"
    DEST_DIR="$2"
    DEST_NAME="$3"

    if [ -x "$SOURCE_FILE" ]; then
        run install -m 755 "$SOURCE_FILE" "${DEST_DIR}/${DEST_NAME}"
        echo "Installed: ${DEST_DIR}/${DEST_NAME}"
    else
        echo "Not built; skipping: ${SOURCE_FILE}"
    fi
}

# Daemons and server administration tools.
install_optional \
    "$SOURCE_DIR/utils/statd/statd" \
    "$NFS_SBIN_DIR" \
    "statd"

install_optional \
    "$SOURCE_DIR/utils/rquotad/rquotad" \
    "$NFS_SBIN_DIR" \
    "rquotad"

# Client-facing and diagnostic utilities.
install_optional \
    "$SOURCE_DIR/utils/nfsstat/nfsstat" \
    "$NFS_BIN_DIR" \
    "nfsstat"

install_optional \
    "$SOURCE_DIR/utils/showmount/showmount" \
    "$NFS_BIN_DIR" \
    "showmount"

install_optional \
    "$SOURCE_DIR/utils/nhfsstone/nhfsstone" \
    "$NFS_BIN_DIR" \
    "nhfsstone"

# ============================================================
# Verify essential installed utilities
# ============================================================

echo
echo "============================================================"
echo "Verifying essential utilities"
echo "============================================================"

for TOOL_PATH in \
    "${SYSTEM_SBIN_DIR}/portmap" \
    "${NFS_SBIN_DIR}/nfsd" \
    "${NFS_SBIN_DIR}/mountd" \
    "${NFS_SBIN_DIR}/exportfs"
do
    [ -x "$TOOL_PATH" ] ||
        fail "Required utility not installed: ${TOOL_PATH}"

    echo "Verified: ${TOOL_PATH}"
done

# ============================================================
# Verify optional utilities when installed
# ============================================================

echo
echo "============================================================"
echo "Verifying additional NFS utilities"
echo "============================================================"

for TOOL in statd rquotad
do
    TOOL_PATH="${NFS_SBIN_DIR}/${TOOL}"

    if [ -x "$TOOL_PATH" ]; then
        echo "Verified: ${TOOL_PATH}"
    fi
done

for TOOL in nfsstat showmount nhfsstone
do
    TOOL_PATH="${NFS_BIN_DIR}/${TOOL}"

    if [ -x "$TOOL_PATH" ]; then
        echo "Verified: ${TOOL_PATH}"
    fi
done

# ============================================================
# Inspect executable formats
# ============================================================

echo
echo "============================================================"
echo "Checking installed executable formats"
echo "============================================================"

file "${SYSTEM_SBIN_DIR}/portmap"
file "${NFS_SBIN_DIR}/nfsd"
file "${NFS_SBIN_DIR}/mountd"
file "${NFS_SBIN_DIR}/exportfs"

for TOOL in statd rquotad
do
    TOOL_PATH="${NFS_SBIN_DIR}/${TOOL}"

    if [ -x "$TOOL_PATH" ]; then
        file "$TOOL_PATH"
    fi
done

for TOOL in nfsstat showmount nhfsstone
do
    TOOL_PATH="${NFS_BIN_DIR}/${TOOL}"

    if [ -x "$TOOL_PATH" ]; then
        file "$TOOL_PATH"
    fi
done

# ============================================================
# Installation summary
# ============================================================

echo
echo "============================================================"
echo "Installed NFS and RPC utilities"
echo "============================================================"

echo
echo "System utilities:"
ls -l "$SYSTEM_SBIN_DIR"

echo
echo "NFS server utilities:"
ls -l "$NFS_SBIN_DIR"

echo
echo "NFS user utilities:"
ls -l "$NFS_BIN_DIR"

echo
echo "SUCCESS: Portmap ${PORTMAP_VERSION} and NFS utilities ${NFS_VERSION}"
echo "staged in ${DISTRODEV_ROOT}"

exit 0