#!/bin/sh

# ============================================================
# LVGD486 - Build GCC 5.4.0
# Script: 15-build-gcc5.sh
# ============================================================

set -e

unset LIBRARY_PATH
unset CPATH
unset C_INCLUDE_PATH
unset CPLUS_INCLUDE_PATH
unset PKG_CONFIG_PATH

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/gcc-5.4.0"
BUILD_DIR="${SOURCES_DIR}/build/gcc-5.4.0"

GCC4_PREFIX="/usr/local/gcc-4.0.2"
GCC4_CC="${GCC4_PREFIX}/bin/gcc"
GCC4_CXX="${GCC4_PREFIX}/bin/g++"

INSTALL_PREFIX="/usr/local"
MAKE_EXECUTABLE="/usr/local/bin/make"

GCC_VERSION="5.4.0"
CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"

# Request static linking for GCC build executables.
LDFLAGS="-static"
STAGE1_LDFLAGS="-static"
BOOT_LDFLAGS="-static"

export LDFLAGS
export STAGE1_LDFLAGS
export BOOT_LDFLAGS

# ============================================================
# Verify root privileges
# ============================================================

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run this script as root."
    exit 1
fi

# ============================================================
# Verify required commands
# ============================================================

for COMMAND in \
    id sed find head dirname mkdir getconf grep \
    readelf rm file
do
    if ! command -v "$COMMAND" >/dev/null 2>&1; then
        echo "Error: Required command not found: ${COMMAND}"
        exit 1
    fi
done

# ============================================================
# Verify GNU Make 3.81
# ============================================================

if [ ! -x "$MAKE_EXECUTABLE" ]; then
    echo "Error: GNU Make 3.81 not found at ${MAKE_EXECUTABLE}"
    echo "Run 12-build-make.sh first."
    exit 1
fi

MAKE_VERSION=$("$MAKE_EXECUTABLE" --version | sed -n '1s/.* //p')

if [ "$MAKE_VERSION" != "3.81" ]; then
    echo "Error: Expected GNU Make 3.81, found ${MAKE_VERSION}"
    exit 1
fi

# ============================================================
# Verify GCC 4.0.2 toolchain
# ============================================================

if [ ! -x "$GCC4_CC" ] || [ ! -x "$GCC4_CXX" ]; then
    echo "Error: GCC 4.0.2 toolchain not found."
    echo "Expected: ${GCC4_CC}"
    echo "Expected: ${GCC4_CXX}"
    echo "Run 14-build-gcc4.sh first."
    exit 1
fi

# ============================================================
# Locate GCC 4.0.2 C++ runtime library
# ============================================================

GCC4_LIBSTDCPP=$(find "$GCC4_PREFIX" \
    -name 'libstdc++.so.6*' -type f -print | head -n 1)

if [ -z "$GCC4_LIBSTDCPP" ]; then
    GCC4_LIBSTDCPP=$(find "$GCC4_PREFIX" \
        -name 'libstdc++.so.6*' -type l -print | head -n 1)
fi

if [ -z "$GCC4_LIBSTDCPP" ]; then
    echo "Error: libstdc++.so.6 not found under ${GCC4_PREFIX}"
    exit 1
fi

GCC4_LIB_DIR=$(dirname "$GCC4_LIBSTDCPP")

LD_LIBRARY_PATH="${GCC4_LIB_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

echo "GCC 4.0.2 C++ runtime: ${GCC4_LIBSTDCPP}"
echo "Library search path: ${LD_LIBRARY_PATH}"

# ============================================================
# Verify GCC 5.4.0 source tree
# ============================================================

if [ ! -x "$CONFIGURE_SCRIPT" ]; then
    echo "Error: GCC ${GCC_VERSION} configure script not found."
    echo "Expected: ${CONFIGURE_SCRIPT}"
    echo "Run 00-download-packages.sh first."
    exit 1
fi

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

echo "Static-linking flags: ${LDFLAGS}"
echo "Stage 1 linker flags: ${STAGE1_LDFLAGS}"
echo "Bootstrap linker flags: ${BOOT_LDFLAGS}"
echo "Available processors: ${JOBS}"
echo "Parallel build jobs: ${JOBS}"

# ============================================================
# Start from a clean build directory
# ============================================================

if [ "$BUILD_DIR" != "/home/user/sources/build/gcc-5.4.0" ]; then
    echo "Error: Unexpected build directory path: ${BUILD_DIR}"
    exit 1
fi

echo "Removing previous GCC build directory..."

if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

echo "Fresh build directory created: ${BUILD_DIR}"

# ============================================================
# Configure GCC using GCC 4.0.2
# ============================================================

cd "$BUILD_DIR"

CC="$GCC4_CC" \
CXX="$GCC4_CXX" \
LDFLAGS="$LDFLAGS" \
"$CONFIGURE_SCRIPT" \
    --prefix="$INSTALL_PREFIX" \
    --enable-languages=c,c++ \
    --disable-nls \
    --disable-multilib \
    --disable-shared \
    --with-stage1-ldflags="$STAGE1_LDFLAGS" \
    --with-boot-ldflags="$BOOT_LDFLAGS" \
    --with-arch=i486 \
    --with-tune=i486 \
    --disable-libsanitizer \
    --disable-libcilkrts

# ============================================================
# Build GCC
# ============================================================

"$MAKE_EXECUTABLE" -j"$JOBS"

# ============================================================
# Verify build outputs before installation
# ============================================================

if [ ! -x "${BUILD_DIR}/gcc/xgcc" ]; then
    echo "Error: GCC bootstrap compiler was not produced."
    exit 1
fi

# ============================================================
# Install GCC
# ============================================================

"$MAKE_EXECUTABLE" install

# ============================================================
# Verify installation
# ============================================================

GCC_BINARY="${INSTALL_PREFIX}/bin/gcc"
GXX_BINARY="${INSTALL_PREFIX}/bin/g++"

if [ ! -x "$GCC_BINARY" ] || [ ! -x "$GXX_BINARY" ]; then
    echo "Error: GCC installation verification failed."
    exit 1
fi

# ============================================================
# Verify static linking of GCC and G++
# ============================================================

for COMPILER in "$GCC_BINARY" "$GXX_BINARY"; do
    echo
    echo "Checking ${COMPILER}..."

    file "$COMPILER"

    if readelf -l "$COMPILER" 2>/dev/null | grep -q 'INTERP'; then
        echo "Error: ${COMPILER} contains a dynamic ELF interpreter."
        echo "Static linking was not successful."
        exit 1
    fi

    if readelf -d "$COMPILER" 2>/dev/null | grep -q 'NEEDED'; then
        echo "Error: ${COMPILER} has dynamic library dependencies."
        exit 1
    fi

    echo "Static ELF checks passed: ${COMPILER}"
done

# ============================================================
# Verify compiler versions
# ============================================================

"$GCC_BINARY" --version
"$GXX_BINARY" --version

# ============================================================
# Refresh shared library cache
# ============================================================

if [ -x /sbin/ldconfig ]; then
    /sbin/ldconfig
fi

# ============================================================
# Build summary
# ============================================================

echo
echo "GCC ${GCC_VERSION} built and installed successfully."
echo "GCC: ${GCC_BINARY}"
echo "G++: ${GXX_BINARY}"
echo "Parallel jobs used: ${JOBS}"
echo "Static ELF checks passed for both compiler drivers."
