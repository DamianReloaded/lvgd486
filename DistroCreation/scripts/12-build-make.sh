#!/bin/sh

# ============================================================
# LVGD486 - Build GNU Make (Static)
# ============================================================

set -e

# Constants
SOURCES_DIR="/home/user/sources"
BUILD_DIR="${SOURCES_DIR}/build/make"
SOURCE_DIR="${SOURCES_DIR}/make"
INSTALL_PREFIX="/usr/local"
BIN_DIR="${INSTALL_PREFIX}/bin"

MAKE_VERSION="3.81"
CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"
MAKE_EXECUTABLE="${BIN_DIR}/make"

# Force static linking.
LDFLAGS="-static"
export LDFLAGS

# Verify permissions before modifying files.
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run this script as root to install GNU Make system-wide."
    exit 1
fi

# ============================================================
# Start from a clean build directory
# ============================================================

# Safety check: only delete the expected GNU Make build directory.
if [ "$BUILD_DIR" != "/home/user/sources/build/make" ]; then
    echo "Error: Unexpected build directory path: ${BUILD_DIR}"
    exit 1
fi

echo "Removing previous GNU Make build directory..."

if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR" "$BIN_DIR"

echo "Fresh build directory created: ${BUILD_DIR}"

# ============================================================
# Detect available processors
# ============================================================

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)

case "$JOBS" in
    ''|*[!0-9]*|0)
        JOBS=$(grep -c '^processor[[:space:]]*:' /proc/cpuinfo 2>/dev/null || true)
        ;;
esac

case "$JOBS" in
    ''|*[!0-9]*|0)
        JOBS=1
        ;;
esac

echo "Available processors: ${JOBS}"
echo "Parallel build jobs: ${JOBS}"
echo "Static-linking flags: ${LDFLAGS}"

# ============================================================
# Verify source tree
# ============================================================

if [ ! -x "$CONFIGURE_SCRIPT" ]; then
    echo "Error: GNU Make ${MAKE_VERSION} configure script not found."
    echo "Expected: ${CONFIGURE_SCRIPT}"
    echo "Run 00-download-packages.sh first."
    exit 1
fi

# ============================================================
# Configure
# ============================================================

cd "$BUILD_DIR"

LDFLAGS="$LDFLAGS" \
"${CONFIGURE_SCRIPT}" \
    --prefix="$INSTALL_PREFIX" || {
        echo "Error: GNU Make configuration failed."
        exit 1
    }

# ============================================================
# Build using all available processors
# ============================================================

make -j"$JOBS"

# ============================================================
# Install GNU Make
# ============================================================

make install

# ============================================================
# Verify installation
# ============================================================

if [ ! -x "$MAKE_EXECUTABLE" ]; then
    echo "Error: GNU Make installation failed."
    exit 1
fi

# Verify version
"$MAKE_EXECUTABLE" --version

# Verify static linking
if command -v file >/dev/null 2>&1; then
    file "$MAKE_EXECUTABLE"
fi

if command -v readelf >/dev/null 2>&1; then
    if readelf -l "$MAKE_EXECUTABLE" | grep -q 'INTERP'; then
        echo "Error: GNU Make still has a dynamic ELF interpreter."
        exit 1
    fi
fi

echo
echo "GNU Make ${MAKE_VERSION} installed successfully."
echo "Executable: ${MAKE_EXECUTABLE}"
echo "Parallel jobs used: ${JOBS}"
echo "Link mode: static"
echo "Use this version explicitly for subsequent builds."
