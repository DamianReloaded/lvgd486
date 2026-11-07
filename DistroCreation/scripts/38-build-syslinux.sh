#!/bin/sh

# ============================================================
# LVGD486 - Build SYSLINUX (2.11)
#
# Compile the SYSLINUX 2.11 Linux installer.
#
# Required tools:
#   gcc
#   make
#   nasm >= 0.98.34
#
# Source directory:
#   /home/user/sources/syslinux
#
# Build directory:
#   /home/user/sources/build/syslinux
#
# Required output:
#   syslinux
#
# The installer runs on the build host. It is not a target
# executable for the 486 system.
#
# The bootloader data is included in the installer.
# LDLINUX.SYS is created when the installer is run against
# the formatted floppy.
#
# Only the syslinux target is built. The default installer
# target also builds graphical samples, which are unnecessary
# for this project.
#
# Installation is handled by a separate script.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/syslinux"
BUILD_ROOT="${SOURCES_DIR}/build"
BUILD_DIR="${BUILD_ROOT}/syslinux"

SYSLINUX_VERSION="2.11"
CC="gcc"

SYSLINUX_EXECUTABLE="${BUILD_DIR}/syslinux"

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

# ============================================================
# Verify permissions
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root to build SYSLINUX."

# ============================================================
# Verify expected paths
# ============================================================

[ "$SOURCE_DIR" = "/home/user/sources/syslinux" ] ||
    fail "Unexpected source directory: ${SOURCE_DIR}"

[ "$BUILD_ROOT" = "/home/user/sources/build" ] ||
    fail "Unexpected build root: ${BUILD_ROOT}"

[ "$BUILD_DIR" = "/home/user/sources/build/syslinux" ] ||
    fail "Unexpected build directory: ${BUILD_DIR}"

# ============================================================
# Verify required tools
# ============================================================

for tool in \
    id make cp file grep readelf rm mkdir \
    gcc nasm sed cut
do
    command -v "$tool" >/dev/null 2>&1 ||
        fail "Required build tool not found: ${tool}"
done

CC_PATH=$(command -v "$CC" 2>/dev/null) ||
    fail "Required compiler '${CC}' not found in PATH."

NASM_PATH=$(command -v nasm 2>/dev/null) ||
    fail "NASM was not found in PATH."

# ============================================================
# Verify compiler
# ============================================================

echo "============================================================"
echo "Checking compiler"
echo "============================================================"

GCC_VERSION=$("$CC" -dumpversion 2>/dev/null) ||
    fail "Could not determine GCC version."

GCC_TARGET=$("$CC" -dumpmachine 2>/dev/null) ||
    fail "Could not determine GCC target architecture."

echo "Compiler command: ${CC}"
echo "Compiler path: ${CC_PATH}"
echo "GCC version: ${GCC_VERSION}"
echo "Compiler target: ${GCC_TARGET}"

case "$GCC_TARGET" in
    i?86-*)
        ;;
    *)
        fail "Expected an i386-family compiler target, got: ${GCC_TARGET}"
        ;;
esac

# ============================================================
# Verify NASM version
# ============================================================

echo
echo "============================================================"
echo "Checking NASM"
echo "============================================================"

NASM_VERSION=$(nasm -v 2>&1) ||
    fail "Could not determine NASM version."

echo "NASM path: ${NASM_PATH}"
echo "NASM version: ${NASM_VERSION}"

NASM_NUMERIC_VERSION=$(echo "$NASM_VERSION" |
    sed 's/.*version[[:space:]]*//' |
    sed 's/[^0-9.].*//')

[ -n "$NASM_NUMERIC_VERSION" ] ||
    fail "Could not parse NASM version."

NASM_MAJOR=$(echo "$NASM_NUMERIC_VERSION" | cut -d. -f1)
NASM_MINOR=$(echo "$NASM_NUMERIC_VERSION" | cut -d. -f2)
NASM_PATCH=$(echo "$NASM_NUMERIC_VERSION" | cut -d. -f3)

NASM_PATCH=${NASM_PATCH:-0}

if [ "$NASM_MAJOR" -lt 0 ] ||
   { [ "$NASM_MAJOR" -eq 0 ] && [ "$NASM_MINOR" -lt 98 ]; } ||
   { [ "$NASM_MAJOR" -eq 0 ] && [ "$NASM_MINOR" -eq 98 ] && [ "$NASM_PATCH" -lt 34 ]; }
then
    fail "SYSLINUX ${SYSLINUX_VERSION} requires NASM 0.98.34 or later."
fi

# ============================================================
# Verify source tree
# ============================================================

echo
echo "============================================================"
echo "Checking SYSLINUX source tree"
echo "============================================================"

[ -d "$SOURCE_DIR" ] ||
    fail "SYSLINUX source directory not found: ${SOURCE_DIR}"

[ -f "${SOURCE_DIR}/Makefile" ] ||
    fail "Top-level SYSLINUX Makefile not found. Run 11-extract-sources.sh first."

[ -f "${SOURCE_DIR}/ldlinux.asm" ] ||
    fail "SYSLINUX bootloader assembly source not found."

[ -f "${SOURCE_DIR}/NEWS" ] ||
    fail "SYSLINUX NEWS file not found."

grep -q 'Changes in 2.11' "${SOURCE_DIR}/NEWS" ||
    fail "Source tree does not appear to be SYSLINUX ${SYSLINUX_VERSION}."

grep -Eq '^syslinux[[:space:]]*:' "${SOURCE_DIR}/Makefile" ||
    fail "Top-level Makefile does not contain the expected syslinux target."

echo "SYSLINUX version: ${SYSLINUX_VERSION}"
echo "Source directory: ${SOURCE_DIR}"
echo "Build directory: ${BUILD_DIR}"
echo "Compiler: ${CC}"
echo "Compiler path: ${CC_PATH}"
echo "Compiler target: ${GCC_TARGET}"
echo "NASM: ${NASM_VERSION}"

# ============================================================
# Clean previous build completely
# ============================================================

echo
echo "============================================================"
echo "Cleaning previous SYSLINUX build"
echo "============================================================"

[ "$BUILD_ROOT" = "/home/user/sources/build" ] ||
    fail "Unexpected build root: ${BUILD_ROOT}"

[ "$BUILD_DIR" = "${BUILD_ROOT}/syslinux" ] ||
    fail "Unsafe build directory: ${BUILD_DIR}"

if [ -e "$BUILD_DIR" ] || [ -L "$BUILD_DIR" ]; then
    echo "Removing: ${BUILD_DIR}"

    rm -rf "$BUILD_DIR" ||
        fail "Could not remove previous build directory."
fi

[ ! -e "$BUILD_DIR" ] && [ ! -L "$BUILD_DIR" ] ||
    fail "Previous build directory still exists."

mkdir -p "$BUILD_DIR" ||
    fail "Could not create fresh build directory."

echo "Fresh build directory created."

# ============================================================
# Copy pristine source tree
# ============================================================

echo
echo "============================================================"
echo "Copying SYSLINUX source tree"
echo "============================================================"

cp -R "${SOURCE_DIR}/." "$BUILD_DIR/" ||
    fail "Could not copy SYSLINUX source tree."

# ============================================================
# Build SYSLINUX installer
#
# Build only the syslinux target.
#
# Do not use:
#   make all
#   make installer
#
# Those targets also build graphical samples, which require
# external image conversion utilities.
#
# The syslinux executable contains the bootloader data needed
# to install SYSLINUX onto the floppy.
# ============================================================

echo
echo "============================================================"
echo "Building SYSLINUX ${SYSLINUX_VERSION} installer"
echo "============================================================"

cd "$BUILD_DIR"

make syslinux ||
    fail "SYSLINUX installer build failed."

# ============================================================
# Verify installer output
# ============================================================

echo
echo "============================================================"
echo "Verifying SYSLINUX installer"
echo "============================================================"

[ -f "$SYSLINUX_EXECUTABLE" ] ||
    fail "SYSLINUX installer was not produced: ${SYSLINUX_EXECUTABLE}"

[ -s "$SYSLINUX_EXECUTABLE" ] ||
    fail "SYSLINUX installer is empty."

[ -x "$SYSLINUX_EXECUTABLE" ] ||
    fail "SYSLINUX installer is not executable."

FILE_OUTPUT=$(file "$SYSLINUX_EXECUTABLE" 2>&1) ||
    fail "Could not inspect SYSLINUX installer."

echo "$FILE_OUTPUT"

echo "$FILE_OUTPUT" | grep -q 'ELF ' ||
    fail "SYSLINUX installer is not an ELF executable."

readelf -h "$SYSLINUX_EXECUTABLE" >/dev/null 2>&1 ||
    fail "SYSLINUX installer has an invalid ELF header."

# ============================================================
# Final report
# ============================================================

echo
echo "============================================================"
echo "SYSLINUX ${SYSLINUX_VERSION} build completed successfully"
echo "============================================================"

echo "Build directory: ${BUILD_DIR}"
echo "SYSLINUX installer: ${SYSLINUX_EXECUTABLE}"
echo "Compiler: ${CC}"
echo "Compiler path: ${CC_PATH}"
echo "GCC version: ${GCC_VERSION}"
echo "Compiler target: ${GCC_TARGET}"
echo "NASM: ${NASM_VERSION}"

echo
echo "Build started from a clean directory."
echo "Only the SYSLINUX installer target was built."
echo "Graphical samples were not built."
echo "No installation was performed."
echo "The source tree and distro staging tree were not modified."
