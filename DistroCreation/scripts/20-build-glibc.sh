
#!/bin/sh

# ============================================================
# LVGD486 - Build glibc 2.3.2
# Script: 20-build-glibc.sh
# Target: Intel 80486 (i486)
#
# Compiler: GCC 3.3.x
# Binutils: 2.15
# Kernel headers: Linux 2.4
# Threading: LinuxThreads
# Python: 2.3.5-compatible patch helper
#
# All output is written to:
#   /home/user/scripts/log.txt
#
# No installation or staging is performed.
# ============================================================

set -eu

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/glibc"
BUILD_PARENT="${SOURCES_DIR}/build"
BUILD_DIR="${BUILD_PARENT}/glibc-2.3.2"

KERNEL_HEADERS_DIR="${SOURCES_DIR}/linux/include"

GCC_CC="/usr/bin/gcc"
HOST_CC="/usr/bin/gcc"

BINUTILS_BUILD="${BUILD_PARENT}/binutils-2.15"
BINUTILS_SYSTEM_DIR="/usr/bin"

MAKE_EXECUTABLE="/usr/local/bin/make"
PYTHON_EXECUTABLE="/usr/bin/python"

LOG_DIR="/home/user/scripts"
LOG_FILE="${LOG_DIR}/log.txt"

GLIBC_VERSION="2.3.2"
GCC_REQUIRED_VERSION="3.3"
BINUTILS_VERSION="2.15"

TARGET_ARCH="i486"
TARGET_CFLAGS="-O2 -march=${TARGET_ARCH}"

CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"
SSCANF_SOURCE="${SOURCE_DIR}/stdio-common/sscanf.c"
SSCANF_BACKUP="${SSCANF_SOURCE}.orig"

BINUTILS_TOOL_DIR="${BUILD_DIR}/binutils-tools"

# ============================================================
# Initialize logging
# ============================================================

mkdir -p "$LOG_DIR"
exec > "$LOG_FILE" 2>&1

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo
    echo "ERROR: $*"
    exit 1
}

trap '
    STATUS=$?
    if [ "$STATUS" -ne 0 ]; then
        echo
        echo "============================================================"
        echo "BUILD FAILED"
        echo "Exit status: ${STATUS}"
        echo "Log file: ${LOG_FILE}"
        echo "============================================================"
    fi
' 0

# ============================================================
# Build start
# ============================================================

echo "============================================================"
echo "LVGD486 - glibc ${GLIBC_VERSION} build"
echo "Started: $(date)"
echo "============================================================"

# ============================================================
# Verify root privileges
# ============================================================

if [ "$(id -u)" -ne 0 ]; then
    fail "Run this script as root."
fi

# ============================================================
# Verify required commands
# ============================================================

for COMMAND in \
    id env sed getconf grep rm mkdir ln cat date \
    test ls cp python awk
do
    if ! command -v "$COMMAND" >/dev/null 2>&1; then
        fail "Required command not found: ${COMMAND}"
    fi
done

if [ ! -x "$PYTHON_EXECUTABLE" ]; then
    fail "Python interpreter not found: ${PYTHON_EXECUTABLE}"
fi

PYTHON_VERSION_FOUND=$("$PYTHON_EXECUTABLE" -V 2>&1)
echo "Python: ${PYTHON_VERSION_FOUND}"

# ============================================================
# Verify source tree and kernel headers
# ============================================================

if [ ! -x "$CONFIGURE_SCRIPT" ]; then
    fail "glibc configure script not found or not executable: ${CONFIGURE_SCRIPT}"
fi

if [ ! -d "${SOURCE_DIR}/linuxthreads" ]; then
    fail "LinuxThreads directory not found."
fi

if [ ! -f "$SSCANF_SOURCE" ]; then
    fail "sscanf source file not found: ${SSCANF_SOURCE}"
fi

if [ ! -f "${KERNEL_HEADERS_DIR}/linux/version.h" ]; then
    fail "Linux kernel version header not found."
fi

if [ ! -d "${KERNEL_HEADERS_DIR}/asm" ]; then
    fail "Architecture-specific kernel headers not found."
fi

if [ ! -d "$BUILD_PARENT" ]; then
    fail "Build parent directory does not exist: ${BUILD_PARENT}"
fi

# ============================================================
# Verify GNU Make 3.81
# ============================================================

if [ ! -x "$MAKE_EXECUTABLE" ]; then
    fail "GNU Make not found: ${MAKE_EXECUTABLE}"
fi

MAKE_VERSION_FOUND=$(
    "$MAKE_EXECUTABLE" --version |
        sed -n '1s/.* //p'
)

if [ "$MAKE_VERSION_FOUND" != "3.81" ]; then
    fail "Expected GNU Make 3.81, found ${MAKE_VERSION_FOUND}"
fi

# ============================================================
# Verify GCC 3.3
# ============================================================

if [ ! -x "$GCC_CC" ]; then
    fail "GCC not found: ${GCC_CC}"
fi

GCC_VERSION_FOUND=$("$GCC_CC" -dumpversion)

case "$GCC_VERSION_FOUND" in
    3.3|3.3.*)
        ;;
    *)
        fail "Expected GCC ${GCC_REQUIRED_VERSION}, found ${GCC_VERSION_FOUND}"
        ;;
esac

HOST_CC_VERSION=$("$HOST_CC" -dumpversion)

echo "GCC: ${GCC_CC}"
echo "GCC version: ${GCC_VERSION_FOUND}"
echo "Host compiler version: ${HOST_CC_VERSION}"
echo "GNU Make version: ${MAKE_VERSION_FOUND}"

# ============================================================
# Patch old glibc sscanf definition
# Compatible with Python 2.3.5
# ============================================================

echo
echo "============================================================"
echo "Checking sscanf compatibility patch"
echo "============================================================"

"$PYTHON_EXECUTABLE" - "$SSCANF_SOURCE" "$SSCANF_BACKUP" <<'PY'
import os
import shutil
import sys

source = sys.argv[1]
backup = sys.argv[2]

f = open(source, "r")
data = f.read()
f.close()

old = (
    "int\n"
    "sscanf (s, format)\n"
    "     const char *s;\n"
    "     const char *format;\n"
    "{"
)

new = (
    "int\n"
    "sscanf (const char *s, const char *format, ...)\n"
    "{"
)

if new in data:
    print("sscanf compatibility patch is already present.")
    sys.exit(0)

count = data.count(old)

if count != 1:
    sys.stderr.write(
        "ERROR: Expected exactly one original sscanf definition; "
        "found %d. Source was not modified.\n" % count
    )
    sys.exit(1)

if os.path.exists(backup):
    sys.stderr.write(
        "ERROR: Backup already exists but patch is absent. "
        "Refusing to overwrite or assume the backup matches: %s\n"
        % backup
    )
    sys.exit(1)

shutil.copy2(source, backup)
print("Original source backed up to: %s" % backup)

updated = data.replace(old, new, 1)

f = open(source, "w")
f.write(updated)
f.close()

f = open(source, "r")
verified = f.read()
f.close()

if verified != updated or new not in verified:
    shutil.copy2(backup, source)
    sys.stderr.write(
        "ERROR: Patch verification failed; original source restored.\n"
    )
    sys.exit(1)

print("Applied and verified ANSI C variadic sscanf definition.")
PY

# ============================================================
# Verify Binutils
# ============================================================

if [ -x "${BINUTILS_BUILD}/gas/as-new" ] &&
   [ -x "${BINUTILS_BUILD}/ld/ld-new" ] &&
   [ -x "${BINUTILS_BUILD}/binutils/ar" ] &&
   [ -x "${BINUTILS_BUILD}/binutils/ranlib" ]
then
    AS_SOURCE="${BINUTILS_BUILD}/gas/as-new"
    LD_SOURCE="${BINUTILS_BUILD}/ld/ld-new"
    AR_SOURCE="${BINUTILS_BUILD}/binutils/ar"
    RANLIB_SOURCE="${BINUTILS_BUILD}/binutils/ranlib"

    echo "Using private Binutils build."
else
    AS_SOURCE="${BINUTILS_SYSTEM_DIR}/as"
    LD_SOURCE="${BINUTILS_SYSTEM_DIR}/ld"
    AR_SOURCE="${BINUTILS_SYSTEM_DIR}/ar"
    RANLIB_SOURCE="${BINUTILS_SYSTEM_DIR}/ranlib"

    echo "Private Binutils outputs incomplete; checking system tools."
fi

for TOOL_PATH in \
    "$AS_SOURCE" "$LD_SOURCE" "$AR_SOURCE" "$RANLIB_SOURCE"
do
    if [ ! -x "$TOOL_PATH" ]; then
        fail "Binutils tool not found or not executable: ${TOOL_PATH}"
    fi
done

AS_VERSION_FOUND=$("$AS_SOURCE" --version | sed -n '1s/.* //p')
LD_VERSION_FOUND=$("$LD_SOURCE" --version | sed -n '1s/.* //p')
AR_VERSION_FOUND=$("$AR_SOURCE" --version | sed -n '1s/.* //p')
RANLIB_VERSION_FOUND=$("$RANLIB_SOURCE" --version | sed -n '1s/.* //p')

if [ "$AS_VERSION_FOUND" != "$BINUTILS_VERSION" ]; then
    fail "Expected assembler ${BINUTILS_VERSION}, found ${AS_VERSION_FOUND}"
fi

if [ "$LD_VERSION_FOUND" != "$BINUTILS_VERSION" ]; then
    fail "Expected linker ${BINUTILS_VERSION}, found ${LD_VERSION_FOUND}"
fi

if [ "$AR_VERSION_FOUND" != "$BINUTILS_VERSION" ]; then
    fail "Expected ar ${BINUTILS_VERSION}, found ${AR_VERSION_FOUND}"
fi

if [ "$RANLIB_VERSION_FOUND" != "$BINUTILS_VERSION" ]; then
    fail "Expected ranlib ${BINUTILS_VERSION}, found ${RANLIB_VERSION_FOUND}"
fi

echo "Verified Binutils version ${BINUTILS_VERSION}."
echo "Assembler: ${AS_SOURCE}"
echo "Linker: ${LD_SOURCE}"
echo "Archiver: ${AR_SOURCE}"
echo "Ranlib: ${RANLIB_SOURCE}"

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

# ============================================================
# Display configuration
# ============================================================

echo
echo "glibc version: ${GLIBC_VERSION}"
echo "Target architecture: ${TARGET_ARCH}"
echo "Target CFLAGS: ${TARGET_CFLAGS}"
echo "Kernel headers: ${KERNEL_HEADERS_DIR}"
echo "LinuxThreads: enabled"
echo "Build directory: ${BUILD_DIR}"
echo "Parallel jobs: ${JOBS}"
echo "Log file: ${LOG_FILE}"

# ============================================================
# Validate build directory before deleting it
# ============================================================

if [ "$BUILD_DIR" != "/home/user/sources/build/glibc-2.3.2" ]; then
    fail "Unexpected build directory path: ${BUILD_DIR}"
fi

case "$BUILD_DIR" in
    "${BUILD_PARENT}/glibc-2.3.2")
        ;;
    *)
        fail "Build directory is outside the expected location."
        ;;
esac

# ============================================================
# Prepare clean build directory
# ============================================================

echo
echo "============================================================"
echo "Preparing clean build directory"
echo "============================================================"

if [ -d "$BUILD_DIR" ]; then
    echo "Removing previous build directory: ${BUILD_DIR}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
mkdir -p "$BINUTILS_TOOL_DIR"

# ============================================================
# Create private Binutils symlinks
# ============================================================

echo
echo "Creating private Binutils tool links."

ln -s "$AS_SOURCE" "${BINUTILS_TOOL_DIR}/as"
ln -s "$LD_SOURCE" "${BINUTILS_TOOL_DIR}/ld"
ln -s "$AR_SOURCE" "${BINUTILS_TOOL_DIR}/ar"
ln -s "$RANLIB_SOURCE" "${BINUTILS_TOOL_DIR}/ranlib"

for TOOL_NAME in as ld ar ranlib
do
    if [ ! -x "${BINUTILS_TOOL_DIR}/${TOOL_NAME}" ]; then
        fail "Private tool link is not executable: ${TOOL_NAME}"
    fi
done

ls -l "$BINUTILS_TOOL_DIR"

# ============================================================
# Test GCC compilation and linking
# ============================================================

echo
echo "============================================================"
echo "Testing GCC for ${TARGET_ARCH}"
echo "============================================================"

GCC_TEST_SOURCE="${BUILD_DIR}/gcc-test.c"
GCC_TEST_OUTPUT="${BUILD_DIR}/gcc-test"

cat > "$GCC_TEST_SOURCE" <<'EOF'
int main(void)
{
    return sizeof(long double) == 0;
}
EOF

env -i \
    HOME="/root" \
    PATH="${BINUTILS_TOOL_DIR}:/usr/bin:/bin" \
    "$GCC_CC" \
    -O2 \
    "-march=${TARGET_ARCH}" \
    -o "$GCC_TEST_OUTPUT" \
    "$GCC_TEST_SOURCE"

if [ ! -x "$GCC_TEST_OUTPUT" ]; then
    fail "GCC test executable was not produced."
fi

echo "GCC compilation and linking test succeeded."

# ============================================================
# Configure glibc 2.3.2
# ============================================================

cd "$BUILD_DIR"

echo
echo "============================================================"
echo "Configuring glibc ${GLIBC_VERSION}"
echo "============================================================"

env -i \
    HOME="/root" \
    PATH="${BINUTILS_TOOL_DIR}:/usr/bin:/bin" \
    CC="$GCC_CC" \
    BUILD_CC="$HOST_CC" \
    CFLAGS="$TARGET_CFLAGS" \
    AS="${BINUTILS_TOOL_DIR}/as" \
    LD="${BINUTILS_TOOL_DIR}/ld" \
    AR="${BINUTILS_TOOL_DIR}/ar" \
    RANLIB="${BINUTILS_TOOL_DIR}/ranlib" \
    "$CONFIGURE_SCRIPT" \
    --prefix=/usr \
    --with-headers="$KERNEL_HEADERS_DIR" \
    --with-binutils="$BINUTILS_TOOL_DIR" \
    --enable-add-ons=linuxthreads \
    --disable-profile \
    --without-gd \
    --enable-kernel=2.4.0

echo
echo "glibc configure completed successfully."

# ============================================================
# Build glibc
# ============================================================

echo
echo "============================================================"
echo "Building glibc ${GLIBC_VERSION}"
echo "============================================================"

"$MAKE_EXECUTABLE" -j"$JOBS"

# ============================================================
# Verify build outputs
# ============================================================

echo
echo "============================================================"
echo "Verifying glibc build outputs"
echo "============================================================"

if [ ! -f "${BUILD_DIR}/libc.so" ] &&
   [ ! -f "${BUILD_DIR}/libc.a" ]
then
    fail "Expected libc.so or libc.a was not produced."
fi

if [ -f "${BUILD_DIR}/libc.so" ]; then
    echo "Found: ${BUILD_DIR}/libc.so"
fi

if [ -f "${BUILD_DIR}/libc.a" ]; then
    echo "Found: ${BUILD_DIR}/libc.a"
fi

# ============================================================
# Build success
# ============================================================

echo
echo "============================================================"
echo "SUCCESS: glibc ${GLIBC_VERSION} build completed."
echo "============================================================"

echo "Target architecture: ${TARGET_ARCH}"
echo "Target CFLAGS: ${TARGET_CFLAGS}"
echo "GCC: ${GCC_CC} (${GCC_VERSION_FOUND})"
echo "Binutils version: ${BINUTILS_VERSION}"
echo "LinuxThreads: enabled"
echo "Build directory: ${BUILD_DIR}"
echo "No installation or staging performed."
echo "Parallel jobs: ${JOBS}"
echo "Finished: $(date)"
echo "Log file: ${LOG_FILE}"

exit 0
