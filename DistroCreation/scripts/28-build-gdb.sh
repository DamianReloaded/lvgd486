#!/bin/sh

# ============================================================
# LVGD486 - Build GNU GDB and GDB Server (Static)
# ============================================================

set -e

# Constants
SOURCES_DIR="/home/user/sources"
BUILD_DIR="${SOURCES_DIR}/build/gdb"
SERVER_BUILD_DIR="${BUILD_DIR}/gdbserver"
SOURCE_DIR="${SOURCES_DIR}/gdb"
SERVER_SOURCE_DIR="${SOURCE_DIR}/gdb/gdbserver"
INSTALL_PREFIX="/usr/local"
BIN_DIR="${INSTALL_PREFIX}/bin"

GDB_VERSION="6.3"
CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"
SERVER_CONFIGURE_SCRIPT="${SERVER_SOURCE_DIR}/configure"
GDB_EXECUTABLE="${BUILD_DIR}/gdb/gdb"
GDBSERVER_EXECUTABLE="${SERVER_BUILD_DIR}/gdbserver"

# Force static linking.
LDFLAGS="-static"
export LDFLAGS

fail()
{
    echo "Error: $*" >&2
    exit 1
}

# ============================================================
# Verify permissions
# ============================================================

if [ "$(id -u)" -ne 0 ]; then
    fail "Run this script as root to install GDB system-wide."
fi

# ============================================================
# Safety checks
# ============================================================

if [ "$BUILD_DIR" != "/home/user/sources/build/gdb" ]; then
    fail "Unexpected build directory path: ${BUILD_DIR}"
fi

if [ "$SERVER_BUILD_DIR" != "/home/user/sources/build/gdb/gdbserver" ]; then
    fail "Unexpected GDB Server build directory path: ${SERVER_BUILD_DIR}"
fi

# ============================================================
# Verify source tree
# ============================================================

if [ ! -x "$CONFIGURE_SCRIPT" ]; then
    fail "GDB ${GDB_VERSION} configure script not found: ${CONFIGURE_SCRIPT}. Run 00-download-packages.sh first."
fi

if [ ! -d "$SERVER_SOURCE_DIR" ]; then
    fail "GDB Server source directory not found: ${SERVER_SOURCE_DIR}"
fi

if [ ! -x "$SERVER_CONFIGURE_SCRIPT" ]; then
    fail "GDB Server configure script not found: ${SERVER_CONFIGURE_SCRIPT}. Check the GDB 6.3 server build instructions."
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

echo "GDB version: ${GDB_VERSION}"
echo "Available processors: ${JOBS}"
echo "Parallel build jobs: ${JOBS}"
echo "Static-linking flags: ${LDFLAGS}"

# ============================================================
# Start from clean build directories
# ============================================================

echo
echo "Removing previous GDB build directory..."

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$BIN_DIR"

echo "Fresh build directory created: ${BUILD_DIR}"

# ============================================================
# Configure GDB
# ============================================================

echo
echo "============================================================"
echo "Configuring GNU GDB ${GDB_VERSION}"
echo "============================================================"

cd "$BUILD_DIR"

LDFLAGS="-static" \
"$CONFIGURE_SCRIPT" \
    --prefix="$INSTALL_PREFIX" \
    --disable-nls \
    --disable-werror || fail "GDB configuration failed."

# ============================================================
# Build GDB statically
# ============================================================

echo
echo "============================================================"
echo "Building GNU GDB ${GDB_VERSION} (static)"
echo "============================================================"

make -j"$JOBS" LDFLAGS="-static"

# ============================================================
# Verify GDB build and static linking
# ============================================================

if [ ! -x "$GDB_EXECUTABLE" ]; then
    fail "GDB build failed. Expected executable: ${GDB_EXECUTABLE}"
fi

if command -v readelf >/dev/null 2>&1; then
    if readelf -l "$GDB_EXECUTABLE" | grep -q 'INTERP'; then
        fail "GDB is dynamically linked: ${GDB_EXECUTABLE}"
    fi
fi

# ============================================================
# Configure GDB Server separately
# ============================================================

echo
echo "============================================================"
echo "Configuring GDB Server ${GDB_VERSION}"
echo "============================================================"

mkdir -p "$SERVER_BUILD_DIR"
cd "$SERVER_BUILD_DIR"

LDFLAGS="-static" \
"$SERVER_CONFIGURE_SCRIPT" \
    --prefix="$INSTALL_PREFIX" \
    --disable-nls \
    --disable-werror || fail "GDB Server configuration failed."

# ============================================================
# Build GDB Server statically
# ============================================================

echo
echo "============================================================"
echo "Building GDB Server ${GDB_VERSION} (static)"
echo "============================================================"

make -j"$JOBS" LDFLAGS="-static"

# ============================================================
# Verify GDB Server build and static linking
# ============================================================

if [ ! -x "$GDBSERVER_EXECUTABLE" ]; then
    fail "GDB Server build failed. Expected executable: ${GDBSERVER_EXECUTABLE}"
fi

if command -v readelf >/dev/null 2>&1; then
    if readelf -l "$GDBSERVER_EXECUTABLE" | grep -q 'INTERP'; then
        fail "GDB Server is dynamically linked: ${GDBSERVER_EXECUTABLE}"
    fi
fi

# ============================================================
# Install GDB and GDB Server
# ============================================================

echo
echo "============================================================"
echo "Installing GDB and GDB Server"
echo "============================================================"

install -m 755 "$GDB_EXECUTABLE" "${BIN_DIR}/gdb"
install -m 755 "$GDBSERVER_EXECUTABLE" "${BIN_DIR}/gdbserver"

# ============================================================
# Verify installation
# ============================================================

[ -x "${BIN_DIR}/gdb" ] ||
    fail "GDB installation failed."

[ -x "${BIN_DIR}/gdbserver" ] ||
    fail "GDB Server installation failed."

echo
echo "Installed GDB:"
"${BIN_DIR}/gdb" --version

echo
echo "Installed GDB Server:"
"${BIN_DIR}/gdbserver" --version

# ============================================================
# Report executable formats
# ============================================================

if command -v file >/dev/null 2>&1; then
    file "${BIN_DIR}/gdb"
    file "${BIN_DIR}/gdbserver"
fi

echo
echo "GDB ${GDB_VERSION} and GDB Server installed successfully."
echo "GDB: ${BIN_DIR}/gdb"
echo "GDB Server: ${BIN_DIR}/gdbserver"
echo "Parallel jobs used: ${JOBS}"
echo "Link mode: static"