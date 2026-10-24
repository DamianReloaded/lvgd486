#!/bin/sh

# ============================================================
# LVGD486 - Build Portmap 5
# Script: 22-build-portmap.sh
#
# Build a statically linked RPC portmapper.
# TCP Wrappers disabled; CHECK_PORT retained.
# No installation or staging performed.
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

echo "LVGD486 - Portmap build started"
echo "Log: ${LOG_FILE}"
echo

# ============================================================
# Constants
# ============================================================

SOURCE_DIR="/home/user/sources/portmap"
MAKEFILE="${SOURCE_DIR}/Makefile"
CHECK_SOURCE="${SOURCE_DIR}/pmap_check.c"
BACKUP="${CHECK_SOURCE}.lvgbak"

GCC4_CC="/usr/local/gcc-4.0.2/bin/gcc"
MAKE_EXECUTABLE="/usr/local/bin/make"

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
# Verify environment
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -x "$GCC4_CC" ] ||
    fail "GCC 4.0.2 not found: ${GCC4_CC}"

GCC_VERSION=$("$GCC4_CC" -dumpversion 2>/dev/null) ||
    fail "Unable to determine GCC version."

[ "$GCC_VERSION" = "4.0.2" ] ||
    fail "Expected GCC 4.0.2; found ${GCC_VERSION}."

[ -x "$MAKE_EXECUTABLE" ] ||
    fail "GNU Make not found: ${MAKE_EXECUTABLE}"

MAKE_VERSION_FOUND=$("$MAKE_EXECUTABLE" --version |
    sed -n '1s/.* //p')

[ "$MAKE_VERSION_FOUND" = "3.81" ] ||
    fail "Expected GNU Make 3.81; found ${MAKE_VERSION_FOUND}."

[ -f "$MAKEFILE" ] ||
    fail "Makefile not found: ${MAKEFILE}"

[ -f "$CHECK_SOURCE" ] ||
    fail "Source file not found: ${CHECK_SOURCE}"

echo "Compiler: ${GCC4_CC}"
echo "GCC version: ${GCC_VERSION}"
echo "Make: ${MAKE_EXECUTABLE}"
echo "Make version: ${MAKE_VERSION_FOUND}"
echo "Source: ${SOURCE_DIR}"
echo "Link mode: static"
echo "TCP Wrappers: disabled"
echo "Port validation: enabled"
echo

# ============================================================
# Clean environment
# ============================================================

unset LIBRARY_PATH
unset CPATH
unset C_INCLUDE_PATH
unset CPLUS_INCLUDE_PATH
unset PKG_CONFIG_PATH
unset CFLAGS
unset CXXFLAGS
unset CPPFLAGS
unset LDFLAGS

# ============================================================
# Back up original source
# ============================================================

if [ ! -f "$BACKUP" ]; then
    cp -p "$CHECK_SOURCE" "$BACKUP" ||
        fail "Could not back up original pmap_check.c."
fi

# ============================================================
# Configure pmap_check.c
# ============================================================

echo "Configuring pmap_check.c..."

# Always start from the saved original source so repeated runs
# do not accumulate source modifications.
cp "$BACKUP" "$CHECK_SOURCE" ||
    fail "Could not restore pmap_check.c from backup."

# Confirm that the expected include exists in the backup.
if ! grep -q 'tcpd\.h' "$CHECK_SOURCE"; then
    fail "tcpd.h include not found in the original source."
fi

TMP_SOURCE="${SOURCE_DIR}/pmap_check.c.tmp"
rm -f "$TMP_SOURCE"

# Wrap the tcpd.h include in HOSTS_ACCESS.
# The source is changed only if the original include is found.
sed \
    's|^[[:space:]]*#include[[:space:]]*<tcpd\.h>[[:space:]]*$|#ifdef HOSTS_ACCESS\
#include <tcpd.h>\
#endif|' \
    "$CHECK_SOURCE" > "$TMP_SOURCE" ||
    fail "Could not generate modified pmap_check.c."

# Verify the expected block exists in the temporary file.
if ! grep -q '^#ifdef HOSTS_ACCESS$' "$TMP_SOURCE"; then
    rm -f "$TMP_SOURCE"
    fail "Could not add HOSTS_ACCESS conditional."
fi

if ! grep -q '^#include <tcpd\.h>$' "$TMP_SOURCE"; then
    rm -f "$TMP_SOURCE"
    fail "tcpd.h include missing from generated source."
fi

if ! grep -q '^#endif$' "$TMP_SOURCE"; then
    rm -f "$TMP_SOURCE"
    fail "Conditional include terminator missing."
fi

# Preserve original file permissions where supported.
chmod --reference="$CHECK_SOURCE" "$TMP_SOURCE" 2>/dev/null || true

cat "$TMP_SOURCE" > "$CHECK_SOURCE" ||
    fail "Could not update pmap_check.c."

rm -f "$TMP_SOURCE"

echo "Updated tcpd.h section:"
grep -n -B 1 -A 1 'tcpd\.h' "$CHECK_SOURCE" || true
echo

# ============================================================
# Configure Makefile
# ============================================================

echo "Disabling TCP Wrappers in Makefile..."

# Comment out the default HOSTS_ACCESS assignment.
sed -i \
    's/^HOSTS_ACCESS=[[:space:]]*-DHOSTS_ACCESS/# HOSTS_ACCESS disabled: tcpd.h unavailable/' \
    "$MAKEFILE"

# Disable libwrap linking.
sed -i \
    's/^WRAP_LIB[[:space:]]*=[[:space:]]*-lwrap[[:space:]]*$/WRAP_LIB =/' \
    "$MAKEFILE"

if grep -q '^HOSTS_ACCESS=[[:space:]]*-DHOSTS_ACCESS' "$MAKEFILE"; then
    fail "HOSTS_ACCESS remains enabled."
fi

if grep -q '^WRAP_LIB[[:space:]]*=[[:space:]]*-lwrap[[:space:]]*$' "$MAKEFILE"; then
    fail "WRAP_LIB still requests libwrap."
fi

grep -q '^CHECK_PORT[[:space:]]*=[[:space:]]*-DCHECK_PORT' "$MAKEFILE" ||
    fail "CHECK_PORT is not enabled."

echo "Makefile settings:"
grep -n -E '^(HOSTS_ACCESS|WRAP_LIB|CHECK_PORT|FACILITY)[[:space:]]*=' \
    "$MAKEFILE" || true

# ============================================================
# Verify preprocessing
# ============================================================

echo
echo "Testing preprocessing..."

"$GCC4_CC" \
    -O2 \
    -DFACILITY=LOG_DAEMON \
    -E "$CHECK_SOURCE" \
    -o /dev/null ||
    fail "Preprocessing pmap_check.c failed."

echo "Preprocessing passed."
echo

# ============================================================
# Determine parallel job count
# ============================================================

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)

case "$JOBS" in
    ''|*[!0-9]*|0)
        JOBS=1
        ;;
esac

echo "Parallel build jobs: ${JOBS}"

# ============================================================
# Clean previous build outputs
# ============================================================

cd "$SOURCE_DIR" ||
    fail "Cannot enter source directory."

echo
echo "Cleaning previous build outputs..."

"$MAKE_EXECUTABLE" clean \
    CC="$GCC4_CC" \
    CFLAGS="-O2 -static -DFACILITY=LOG_DAEMON" ||
    fail "Portmap clean target failed."

# ============================================================
# Build
# ============================================================

echo
echo "Building Portmap with static linking..."

"$MAKE_EXECUTABLE" \
    -j"$JOBS" \
    CC="$GCC4_CC" \
    CFLAGS="-O2 -static -DFACILITY=LOG_DAEMON" \
    LDFLAGS="-static" \
    WRAP_LIB= \
    portmap ||
    fail "Portmap compilation or linking failed."

# ============================================================
# Verify executable
# ============================================================

PORTMAP_BINARY="${SOURCE_DIR}/portmap"

[ -f "$PORTMAP_BINARY" ] ||
    fail "Portmap executable was not produced."

[ -x "$PORTMAP_BINARY" ] ||
    fail "Portmap output is not executable."

FILE_COMMAND=$(command -v file 2>/dev/null || true)

[ -n "$FILE_COMMAND" ] ||
    fail "The file command is unavailable."

FILE_DESCRIPTION=$("$FILE_COMMAND" "$PORTMAP_BINARY") ||
    fail "Could not inspect Portmap executable."

echo
echo "$FILE_DESCRIPTION"

case "$FILE_DESCRIPTION" in
    *ELF*) ;;
    *) fail "Output is not an ELF executable." ;;
esac

case "$FILE_DESCRIPTION" in
    *"statically linked"*)
        echo "Static-link verification passed."
        ;;
    *)
        fail "Portmap is not statically linked."
        ;;
esac

case "$FILE_DESCRIPTION" in
    *"80386"*|*"Intel 80386"*)
        echo "32-bit x86 architecture verification passed."
        ;;
    *)
        echo "WARNING: Could not confirm 32-bit x86 architecture."
        ;;
esac

# ============================================================
# Success
# ============================================================

echo
echo "SUCCESS: Portmap build completed."
echo "Executable: ${PORTMAP_BINARY}"
echo "Static linking: verified"
echo "TCP Wrappers: disabled"
echo "Port validation: enabled"
echo "No installation or staging performed."
echo "Log: ${LOG_FILE}"

exit 0