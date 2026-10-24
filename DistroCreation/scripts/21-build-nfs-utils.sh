#!/bin/sh

# ============================================================
# LVGD486 - Build NFS Utilities 1.0.6
# Script: 21-build-nfs-utils.sh
# Build NFS utilities with static linking.
# No installation or staging is performed.
# ============================================================

set -e

# ============================================================
# Logging: save output to log.txt
# ============================================================

LOG_DIR="/home/user/scripts"
LOG_FILE="${LOG_DIR}/log.txt"

mkdir -p "$LOG_DIR" || exit 1
touch "$LOG_FILE" || exit 1

exec > "$LOG_FILE" 2>&1

echo "LVGD486 - NFS utilities static build started"
echo "Log: ${LOG_FILE}"
echo

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/nfs-utils"

GCC4_CC="/usr/local/gcc-4.0.2/bin/gcc"
MAKE_EXECUTABLE="/usr/local/bin/make"

SUPPORT_LIB_DIR="${SOURCE_DIR}/support/lib"

STATIC_CC="${GCC4_CC} -static"
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
# Verify environment
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -x "$GCC4_CC" ] ||
    fail "GCC 4.0.2 not found: ${GCC4_CC}"

GCC_VERSION_FOUND=$("$GCC4_CC" -dumpversion)

[ "$GCC_VERSION_FOUND" = "4.0.2" ] ||
    fail "Expected GCC 4.0.2; found ${GCC_VERSION_FOUND}"

[ -x "$MAKE_EXECUTABLE" ] ||
    fail "GNU Make not found: ${MAKE_EXECUTABLE}"

MAKE_VERSION_FOUND=$("$MAKE_EXECUTABLE" --version |
    sed -n '1s/.* //p')

[ "$MAKE_VERSION_FOUND" = "3.81" ] ||
    fail "Expected Make 3.81; found ${MAKE_VERSION_FOUND}"

[ -x "${SOURCE_DIR}/configure" ] ||
    fail "Configure script not found: ${SOURCE_DIR}/configure"

[ -f "${SOURCE_DIR}/rules.mk" ] ||
    fail "Build rules not found: ${SOURCE_DIR}/rules.mk"

[ -f "${SOURCE_DIR}/config.mk.in" ] ||
    fail "Configuration template not found: ${SOURCE_DIR}/config.mk.in"

[ -d "${SOURCE_DIR}/support" ] ||
    fail "Support source directory is missing."

[ -d "${SOURCE_DIR}/tools/rpcgen" ] ||
    fail "rpcgen source directory is missing."

[ -d "${SOURCE_DIR}/utils/nfsd" ] ||
    fail "NFS server source directory is missing."

[ -d "${SOURCE_DIR}/utils/mountd" ] ||
    fail "NFS mount daemon source directory is missing."

[ -d "${SOURCE_DIR}/utils/exportfs" ] ||
    fail "NFS export utility source directory is missing."

echo "Compiler: ${GCC4_CC}"
echo "Static compiler flags: -static"
echo "Static linker flags: ${STATIC_LDFLAGS}"
echo "Support library directory: ${SUPPORT_LIB_DIR}"
echo "Make: ${MAKE_EXECUTABLE}"
echo "Source: ${SOURCE_DIR}"
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
# Prepare source tree
# ============================================================

[ "$SOURCE_DIR" = "/home/user/sources/nfs-utils" ] ||
    fail "Unexpected source directory path."

mkdir -p "${SOURCE_DIR}/support/include" ||
    fail "Cannot create support/include directory."

cd "$SOURCE_DIR" ||
    fail "Cannot enter source directory."

# ============================================================
# Clean previous configuration
# ============================================================

echo
echo "Cleaning previous configuration files..."

rm -f \
    config.cache \
    config.log \
    config.status \
    config.mk \
    nfs-utils.spec \
    utils/Makefile \
    support/include/config.h

# ============================================================
# Configure
# ============================================================

echo
echo "Configuring NFS utilities 1.0.6..."

env CC="$GCC4_CC" \
    CFLAGS="-O2" \
    LDFLAGS="-static" \
    ./configure --prefix=/usr/local

[ -f config.mk ] ||
    fail "Configure did not generate config.mk."

[ -f support/include/config.h ] ||
    fail "Configure did not generate support/include/config.h."

[ -f utils/Makefile ] ||
    fail "Configure did not generate utils/Makefile."

# ============================================================
# Static build arguments
# ============================================================

echo
echo "Static linking enabled for all build stages."
echo "Compiler command: ${STATIC_CC}"
echo "Linker flags: ${STATIC_LDFLAGS}"

# ============================================================
# Build tools first
# rpcgen is required by the support/export build.
# ============================================================

echo
echo "============================================================"
echo "Building tools with static linking"
echo "============================================================"

"$MAKE_EXECUTABLE" \
    -B \
    -C tools \
    TOP=../ \
    "CC=${STATIC_CC}" \
    "LDFLAGS=${STATIC_LDFLAGS}" \
    -j"$JOBS" \
    all

# ============================================================
# Verify rpcgen at the path expected by support/export
# ============================================================

RPCGEN="${SOURCE_DIR}/bin/rpcgen"

if [ ! -x "$RPCGEN" ]; then
    echo
    echo "Expected rpcgen was not found at:"
    echo "  ${RPCGEN}"
    echo
    echo "Searching the source tree for rpcgen..."

    find "$SOURCE_DIR" -type f -name rpcgen -print

    fail "The tools build did not produce executable ${RPCGEN}."
fi

echo "rpcgen: ${RPCGEN}"

# ============================================================
# Build support libraries and headers
# ============================================================

echo
echo "============================================================"
echo "Building support components with static linking"
echo "============================================================"

"$MAKE_EXECUTABLE" \
    -B \
    -C support \
    TOP=../ \
    "CC=${STATIC_CC}" \
    "LDFLAGS=${STATIC_LDFLAGS}" \
    -j"$JOBS" \
    all

# ============================================================
# Verify support library archives
# ============================================================

echo
echo "============================================================"
echo "Verifying support library archives"
echo "============================================================"

for LIBRARY in \
    "${SOURCE_DIR}/support/export/libexport.a" \
    "${SOURCE_DIR}/support/nfs/libnfs.a" \
    "${SOURCE_DIR}/support/misc/libmisc.a"
do
    [ -f "$LIBRARY" ] ||
        fail "Required support archive not found: ${LIBRARY}"

    echo "Verified: ${LIBRARY}"
done

# ============================================================
# Build NFS utilities
# Include support/lib in the linker search path.
# ============================================================

echo
echo "============================================================"
echo "Building NFS utilities with static linking"
echo "============================================================"

"$MAKE_EXECUTABLE" \
    -B \
    -C utils \
    TOP=../ \
    "CC=${STATIC_CC}" \
    "LDFLAGS=${STATIC_LDFLAGS} -L${SUPPORT_LIB_DIR}" \
    -j"$JOBS" \
    all

# ============================================================
# Verify essential NFS server utilities
# ============================================================

echo
echo "============================================================"
echo "Verifying essential NFS server utilities"
echo "============================================================"

for OUTPUT in \
    "${SOURCE_DIR}/utils/nfsd/nfsd" \
    "${SOURCE_DIR}/utils/mountd/mountd" \
    "${SOURCE_DIR}/utils/exportfs/exportfs"
do
    [ -x "$OUTPUT" ] ||
        fail "Expected server utility missing: ${OUTPUT}"

    echo "Built: ${OUTPUT}"
done

# ============================================================
# Verify additional utilities when built
# ============================================================

for OUTPUT in \
    "${SOURCE_DIR}/utils/statd/statd" \
    "${SOURCE_DIR}/utils/nfsstat/nfsstat" \
    "${SOURCE_DIR}/utils/showmount/showmount" \
    "${SOURCE_DIR}/utils/rquotad/rquotad" \
    "${SOURCE_DIR}/utils/nhfsstone/nhfsstone"
do
    if [ -x "$OUTPUT" ]; then
        echo "Built: ${OUTPUT}"
    else
        echo "Not produced: ${OUTPUT}"
    fi
done

# ============================================================
# Verify static linking of ELF executables
# under bin, tools, and utils.
# ============================================================

echo
echo "============================================================"
echo "Verifying static linking"
echo "============================================================"

CHECKED_ELF_COUNT=0

for SEARCH_DIR in \
    "${SOURCE_DIR}/bin" \
    "${SOURCE_DIR}/tools" \
    "${SOURCE_DIR}/utils"
do
    [ -d "$SEARCH_DIR" ] || continue

    find "$SEARCH_DIR" -type f -perm -111 -print |
    while read OUTPUT
    do
        FILE_DESCRIPTION=$(file "$OUTPUT") ||
            exit 1

        case "$FILE_DESCRIPTION" in
            *ELF*)
                echo "$FILE_DESCRIPTION"

                case "$FILE_DESCRIPTION" in
                    *"statically linked"*)
                        ;;
                    *)
                        echo "ERROR: Executable is not statically linked: ${OUTPUT}"
                        exit 1
                        ;;
                esac
                ;;
        esac
    done || fail "Static-link verification failed in ${SEARCH_DIR}"

    # Count ELF executables in this directory.
    for OUTPUT in $(find "$SEARCH_DIR" -type f -perm -111 -print)
    do
        FILE_DESCRIPTION=$(file "$OUTPUT") ||
            fail "Cannot inspect executable: ${OUTPUT}"

        case "$FILE_DESCRIPTION" in
            *ELF*)
                case "$FILE_DESCRIPTION" in
                    *"statically linked"*)
                        CHECKED_ELF_COUNT=$((CHECKED_ELF_COUNT + 1))
                        ;;
                esac
                ;;
        esac
    done
done

[ "$CHECKED_ELF_COUNT" -gt 0 ] ||
    fail "No statically linked ELF executables were found."

echo
echo "Verified statically linked ELF executables: ${CHECKED_ELF_COUNT}"

# ============================================================
# Success
# ============================================================

echo
echo "============================================================"
echo "Static build summary"
echo "============================================================"

echo "NFS server: ${SOURCE_DIR}/utils/nfsd/nfsd"
echo "Mount daemon: ${SOURCE_DIR}/utils/mountd/mountd"
echo "Export utility: ${SOURCE_DIR}/utils/exportfs/exportfs"
echo "Verified static ELF executables: ${CHECKED_ELF_COUNT}"
echo "No installation or staging performed."
echo "Log: ${LOG_FILE}"

echo
echo "SUCCESS: NFS utilities 1.0.6 static build completed."

exit 0