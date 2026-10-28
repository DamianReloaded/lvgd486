#!/bin/sh

# ============================================================
# LVGD486 - Build Midnight Commander (Static)
#
# Builds Midnight Commander 4.6.1 using GCC 3.3.
# Static linking is requested.
# No installation or staging is performed.
# ============================================================

set -e

# ============================================================
# Logging
# ============================================================

LOG_DIR="/home/user/scripts"
LOG_FILE="${LOG_DIR}/log.txt"

mkdir -p "$LOG_DIR" || exit 1
touch "$LOG_FILE" || exit 1

exec > "$LOG_FILE" 2>&1

echo "LVGD486 - Midnight Commander static build started"
echo "Log: ${LOG_FILE}"
echo

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/mc"
BUILD_DIR="${SOURCES_DIR}/build/mc"

GCC_CC="/usr/bin/gcc-3.3"
MAKE_EXECUTABLE="/usr/bin/make"

INSTALL_PREFIX="/usr"
BIN_DIR="${INSTALL_PREFIX}/bin"
LIB_DIR="${INSTALL_PREFIX}/lib"
DATA_DIR="${INSTALL_PREFIX}/share"

MC_VERSION="4.6.1"
CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"

STATIC_CFLAGS="-O2 -gdwarf-2"
STATIC_LDFLAGS="-static"

# ============================================================
# Failure handler
# ============================================================

fail()
{
    echo
    echo "ERROR: $*"
    echo "Build stopped. Log: ${LOG_FILE}"
    exit 1
}

# ============================================================
# Verify permissions and tools
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -x "$GCC_CC" ] ||
    fail "GCC 3.3 not found: ${GCC_CC}"

GCC_VERSION_FOUND=$("$GCC_CC" -dumpversion 2>/dev/null) ||
    fail "Unable to determine GCC version."

case "$GCC_VERSION_FOUND" in
    3.3|3.3.*)
        ;;
    *)
        fail "Expected GCC 3.3; found ${GCC_VERSION_FOUND}"
        ;;
esac

[ -x "$MAKE_EXECUTABLE" ] ||
    fail "GNU Make not found: ${MAKE_EXECUTABLE}"

MAKE_VERSION_FOUND=$("$MAKE_EXECUTABLE" --version |
    sed -n '1s/.* //p')

echo "Compiler: ${GCC_CC}"
echo "Compiler version: ${GCC_VERSION_FOUND}"
echo "Make: ${MAKE_EXECUTABLE}"
echo "Make version: ${MAKE_VERSION_FOUND}"
echo "CFLAGS: ${STATIC_CFLAGS}"
echo "LDFLAGS: ${STATIC_LDFLAGS}"
echo

# ============================================================
# Verify source tree
# ============================================================

[ "$SOURCE_DIR" = "/home/user/sources/mc" ] ||
    fail "Unexpected source directory: ${SOURCE_DIR}"

[ "$BUILD_DIR" = "/home/user/sources/build/mc" ] ||
    fail "Unexpected build directory: ${BUILD_DIR}"

[ -f "$CONFIGURE_SCRIPT" ] ||
    fail "Midnight Commander ${MC_VERSION} configure script not found: ${CONFIGURE_SCRIPT}"

[ -f "${SOURCE_DIR}/configure.in" ] ||
    echo "WARNING: configure.in not found; continuing with existing configure script."

# ============================================================
# Clean inherited build environment
# ============================================================

unset LIBRARY_PATH
unset CPATH
unset C_INCLUDE_PATH
unset CPLUS_INCLUDE_PATH
unset PKG_CONFIG_PATH
unset CC
unset CXX
unset CFLAGS
unset CXXFLAGS
unset CPPFLAGS
unset LDFLAGS

# ============================================================
# Determine parallel job count
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

echo "Parallel build jobs: ${JOBS}"

# ============================================================
# Prepare clean build directory
# ============================================================

echo
echo "Preparing clean build directory..."

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" ||
    fail "Cannot create build directory: ${BUILD_DIR}"

echo "Build directory: ${BUILD_DIR}"

# ============================================================
# Configure
# ============================================================

echo
echo "============================================================"
echo "Configuring Midnight Commander ${MC_VERSION}"
echo "============================================================"

cd "$BUILD_DIR" ||
    fail "Cannot enter build directory."

env \
    CC="$GCC_CC" \
    CFLAGS="$STATIC_CFLAGS" \
    CPPFLAGS="" \
    LDFLAGS="$STATIC_LDFLAGS" \
    "$CONFIGURE_SCRIPT" \
        --prefix="$INSTALL_PREFIX" \
        --bindir="$BIN_DIR" \
        --libdir="$LIB_DIR" \
        --with-ncurses \
        --without-gpm \
        --without-samba \
        --without-vfs \
        --disable-nls ||
    fail "Midnight Commander configuration failed."

[ -f Makefile ] ||
    fail "Configure did not generate the top-level Makefile."

[ -f config.log ] ||
    echo "WARNING: configure did not generate config.log."

# ============================================================
# Verify configured compiler
# ============================================================

echo
echo "============================================================"
echo "Checking configured build settings"
echo "============================================================"

if grep -E '^(CC|CFLAGS|LDFLAGS)[[:space:]]*=' Makefile; then
    :
else
    echo "WARNING: Could not find expected compiler settings in Makefile."
fi

# ============================================================
# Build
# ============================================================

echo
echo "============================================================"
echo "Building Midnight Commander"
echo "============================================================"

"$MAKE_EXECUTABLE" \
    -j"$JOBS" \
    "CC=${GCC_CC}" \
    "CFLAGS=${STATIC_CFLAGS}" \
    "LDFLAGS=${STATIC_LDFLAGS}" ||
    fail "Midnight Commander compilation failed."

# ============================================================
# Verify build output
# ============================================================

echo
echo "============================================================"
echo "Verifying build output"
echo "============================================================"

MC_BUILD_EXECUTABLE="${BUILD_DIR}/src/mc"

if [ ! -x "$MC_BUILD_EXECUTABLE" ]; then
    echo "Searching build tree for the mc executable..."
    find "$BUILD_DIR" -type f -name mc -print
    fail "Expected executable not found: ${MC_BUILD_EXECUTABLE}"
fi

echo "Built executable: ${MC_BUILD_EXECUTABLE}"

# ============================================================
# Verify static linking
# ============================================================

echo
echo "============================================================"
echo "Verifying static linking"
echo "============================================================"

if command -v file >/dev/null 2>&1; then
    FILE_DESCRIPTION=$(file "$MC_BUILD_EXECUTABLE") ||
        fail "Cannot inspect built executable."

    echo "$FILE_DESCRIPTION"

    case "$FILE_DESCRIPTION" in
        *"statically linked"*)
            ;;
        *)
            fail "The built executable is not reported as statically linked."
            ;;
    esac
fi

if command -v readelf >/dev/null 2>&1; then
    if readelf -l "$MC_BUILD_EXECUTABLE" 2>/dev/null |
        grep -q 'INTERP'; then
        fail "The executable has a dynamic ELF interpreter."
    fi

    if readelf -d "$MC_BUILD_EXECUTABLE" 2>/dev/null |
        grep -q '(NEEDED)'; then
        fail "The executable has dynamic library dependencies."
    fi
fi

# ============================================================
# Success
# ============================================================

echo
echo "============================================================"
echo "Build summary"
echo "============================================================"

echo "Midnight Commander: ${MC_VERSION}"
echo "Compiler: ${GCC_CC}"
echo "Compiler version: ${GCC_VERSION_FOUND}"
echo "Executable: ${MC_BUILD_EXECUTABLE}"
echo "Parallel jobs: ${JOBS}"
echo "Link mode: static"
echo "Installation: not performed"
echo "Staging: not performed"
echo "Log: ${LOG_FILE}"

echo
echo "SUCCESS: Midnight Commander ${MC_VERSION} build completed."

exit 0