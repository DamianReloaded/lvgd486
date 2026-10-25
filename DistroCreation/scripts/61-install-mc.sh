#!/bin/sh

# ============================================================
# LVGD486 - Install Midnight Commander
# Script: 27-install-mc.sh
# Install the built Midnight Commander into the staging tree.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCE_DIR="/home/user/sources/mc"
BUILD_DIR="/home/user/sources/build/mc"
DISTRODEV_ROOT="/home/user/distrodev"

MC_VERSION="4.6.1"

MC_SOURCE_EXECUTABLE="${BUILD_DIR}/src/mc"

MC_BIN_DIR="${DISTRODEV_ROOT}/usr/bin"
MC_LIB_DIR="${DISTRODEV_ROOT}/usr/lib"
MC_DATA_DIR="${DISTRODEV_ROOT}/usr/share"
MC_SHARE_DIR="${MC_DATA_DIR}/mc"
MC_LOCALE_DIR="${MC_DATA_DIR}/locale"

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
    fail "Development staging root does not exist: ${DISTRODEV_ROOT}"

[ -d "$SOURCE_DIR" ] ||
    fail "Midnight Commander source directory not found: ${SOURCE_DIR}"

[ -d "$BUILD_DIR" ] ||
    fail "Midnight Commander build directory not found: ${BUILD_DIR}. Run 26-build-mc.sh first."

[ -x "$MC_SOURCE_EXECUTABLE" ] ||
    fail "Built Midnight Commander executable not found: ${MC_SOURCE_EXECUTABLE}"

echo "Midnight Commander version: ${MC_VERSION}"
echo "Source directory: ${SOURCE_DIR}"
echo "Build directory: ${BUILD_DIR}"
echo "Built executable: ${MC_SOURCE_EXECUTABLE}"
echo "Staging root: ${DISTRODEV_ROOT}"
echo

# ============================================================
# Create staging directories
# ============================================================

echo
echo "============================================================"
echo "Preparing staging directories"
echo "============================================================"

run mkdir -p "$MC_BIN_DIR"
run mkdir -p "$MC_LIB_DIR"
run mkdir -p "$MC_SHARE_DIR"
run mkdir -p "$MC_LOCALE_DIR"

# ============================================================
# Install Midnight Commander executable
# ============================================================

echo
echo "============================================================"
echo "Installing Midnight Commander executable"
echo "============================================================"

run install -m 755 \
    "$MC_SOURCE_EXECUTABLE" \
    "${MC_BIN_DIR}/mc"

# ============================================================
# Install supporting data files
# ============================================================

echo
echo "============================================================"
echo "Installing Midnight Commander data files"
echo "============================================================"

# MC 4.6.1 normally keeps its supporting data in the source
# tree's mc.ext, mc.hint, and related files. Only stage files
# that actually exist in this source tree.

install_optional_file()
{
    SOURCE_FILE="$1"
    DEST_FILE="$2"

    if [ -f "$SOURCE_FILE" ]; then
        run install -m 644 "$SOURCE_FILE" "$DEST_FILE"
        echo "Installed: ${DEST_FILE}"
    else
        echo "Not found; skipping: ${SOURCE_FILE}"
    fi
}

# Extension configuration and hint data, when present.
install_optional_file \
    "${SOURCE_DIR}/misc/mc.ext" \
    "${MC_SHARE_DIR}/mc.ext"

install_optional_file \
    "${SOURCE_DIR}/misc/mc.hint" \
    "${MC_SHARE_DIR}/mc.hint"

install_optional_file \
    "${SOURCE_DIR}/misc/mc.menu" \
    "${MC_SHARE_DIR}/mc.menu"

# ============================================================
# Install compiled support data if generated
# ============================================================

echo
echo "============================================================"
echo "Checking generated support data"
echo "============================================================"

if [ -d "${BUILD_DIR}/lib" ]; then
    find "${BUILD_DIR}/lib" -type f -name '*.so*' -print
fi

if [ -d "${BUILD_DIR}/misc" ]; then
    find "${BUILD_DIR}/misc" -type f -print
fi

# ============================================================
# Verify installed executable
# ============================================================

echo
echo "============================================================"
echo "Verifying installed executable"
echo "============================================================"

MC_INSTALLED_EXECUTABLE="${MC_BIN_DIR}/mc"

[ -x "$MC_INSTALLED_EXECUTABLE" ] ||
    fail "Midnight Commander was not installed: ${MC_INSTALLED_EXECUTABLE}"

echo "Verified: ${MC_INSTALLED_EXECUTABLE}"

# ============================================================
# Verify static linking
# ============================================================

echo
echo "============================================================"
echo "Verifying static linking"
echo "============================================================"

if command -v file >/dev/null 2>&1; then
    FILE_DESCRIPTION=$(file "$MC_INSTALLED_EXECUTABLE") ||
        fail "Cannot inspect installed executable."

    echo "$FILE_DESCRIPTION"

    case "$FILE_DESCRIPTION" in
        *"statically linked"*)
            ;;
        *)
            fail "Installed executable is not reported as statically linked."
            ;;
    esac
fi

if command -v readelf >/dev/null 2>&1; then
    if readelf -l "$MC_INSTALLED_EXECUTABLE" 2>/dev/null |
        grep -q 'INTERP'; then
        fail "Installed executable has a dynamic ELF interpreter."
    fi

    if readelf -d "$MC_INSTALLED_EXECUTABLE" 2>/dev/null |
        grep -q '(NEEDED)'; then
        fail "Installed executable has dynamic library dependencies."
    fi
fi

# ============================================================
# Inspect installed executable format
# ============================================================

echo
echo "============================================================"
echo "Inspecting installed executable"
echo "============================================================"

file "$MC_INSTALLED_EXECUTABLE"

# ============================================================
# Installation summary
# ============================================================

echo
echo "============================================================"
echo "Installed Midnight Commander"
echo "============================================================"

echo
echo "Executable:"
ls -l "$MC_INSTALLED_EXECUTABLE"

echo
echo "Supporting data directory:"
ls -la "$MC_SHARE_DIR"

echo
echo "SUCCESS: Midnight Commander ${MC_VERSION}"
echo "staged in ${DISTRODEV_ROOT}"

exit 0