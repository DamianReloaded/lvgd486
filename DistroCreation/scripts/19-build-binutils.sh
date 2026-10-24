#!/bin/sh

# ============================================================
# LVGD486 - Build Binutils 2.15
# Script: 19-build-binutils.sh
# Apply the GAS relax_type fix and test the build.
# ============================================================

set -e

# ============================================================
# Logging: display output and save it to log.txt
# ============================================================

LOG_DIR="/home/user/scripts"
LOG_FILE="${LOG_DIR}/log.txt"

mkdir -p "$LOG_DIR" || exit 1
touch "$LOG_FILE" || exit 1

exec > "$LOG_FILE" 2>&1

echo "LVGD486 - Binutils build started"
echo "Log: ${LOG_FILE}"
echo

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/binutils"
BUILD_DIR="${SOURCES_DIR}/build/binutils-2.15"

GCC4_CC="/usr/local/gcc-4.0.2/bin/gcc"
MAKE_EXECUTABLE="/usr/local/bin/make"

BACKUP_DIR="${SOURCES_DIR}/binutils-backup-before-relax-fix-$(date +%Y%m%d-%H%M%S)"

AS_HEADER="${SOURCE_DIR}/gas/as.h"
TC_HEADER="${SOURCE_DIR}/gas/tc.h"

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

[ "$("$GCC4_CC" -dumpversion)" = "4.0.2" ] ||
    fail "Unexpected GCC version."

[ -x "$MAKE_EXECUTABLE" ] ||
    fail "GNU Make not found: ${MAKE_EXECUTABLE}"

MAKE_VERSION_FOUND=$("$MAKE_EXECUTABLE" --version |
    sed -n '1s/.* //p')

[ "$MAKE_VERSION_FOUND" = "3.81" ] ||
    fail "Expected Make 3.81; found ${MAKE_VERSION_FOUND}"

[ -x "${SOURCE_DIR}/configure" ] ||
    fail "Binutils configure script not found."

[ -f "$AS_HEADER" ] ||
    fail "Missing ${AS_HEADER}"

[ -f "$TC_HEADER" ] ||
    fail "Missing ${TC_HEADER}"

[ -f "${SOURCE_DIR}/gas/config/tc-i386.h" ] ||
    fail "Missing GAS i386 header."

echo "Compiler: ${GCC4_CC}"
echo "Make: ${MAKE_EXECUTABLE}"
echo "Source: ${SOURCE_DIR}"
echo "Build: ${BUILD_DIR}"
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
# Identify source state before modifying anything
# ============================================================

if grep -q 'offsetT rlx_forward;' "$AS_HEADER" &&
   grep -q 'offsetT rlx_backward;' "$AS_HEADER" &&
   grep -q 'typedef struct relax_type relax_typeS;' "$AS_HEADER" &&
   ! grep -q 'long rlx_forward;' "$TC_HEADER" &&
   ! grep -q 'long rlx_backward;' "$TC_HEADER"; then

    echo "The relax_type fix appears to be present."

elif grep -q 'long rlx_forward;' "$TC_HEADER" &&
     grep -q 'long rlx_backward;' "$TC_HEADER" &&
     ! grep -q 'offsetT rlx_forward;' "$AS_HEADER" &&
     ! grep -q 'offsetT rlx_backward;' "$AS_HEADER"; then

    echo "Unpatched relax_type definition detected."
    echo "Creating backup: ${BACKUP_DIR}"

    mkdir "$BACKUP_DIR" ||
        fail "Cannot create backup directory."

    cp -p "$AS_HEADER" "${BACKUP_DIR}/as.h"
    cp -p "$TC_HEADER" "${BACKUP_DIR}/tc.h"

    echo "Header backups created."

    # --------------------------------------------------------
    # Apply the patch using Python only after verifying exact
    # source blocks. This avoids line-number-dependent patches.
    # --------------------------------------------------------

    python - "$AS_HEADER" "$TC_HEADER" <<'PYTHON'
import sys

as_path = sys.argv[1]
tc_path = sys.argv[2]

as_file = open(as_path, "r")
as_text = as_file.read()
as_file.close()

tc_file = open(tc_path, "r")
tc_text = tc_file.read()
tc_file.close()

as_anchor = "typedef addressT relax_addressT;\n"
if as_text.count(as_anchor) != 1:
    sys.stderr.write("ERROR: Expected unique relax_addressT anchor in as.h\n")
    sys.exit(2)

old_struct = """struct relax_type
{
  /* Forward reach. Signed number. > 0.  */
  long rlx_forward;
  /* Backward reach. Signed number. < 0.  */
  long rlx_backward;

  /* Bytes length of this address.  */
  unsigned char rlx_length;

  /* Next longer relax-state.  0 means there is no 'next' relax-state.  */
  relax_substateT rlx_more;
};

typedef struct relax_type relax_typeS;
"""

if tc_text.count(old_struct) != 1:
    sys.stderr.write("ERROR: Expected unique original struct in tc.h\n")
    sys.exit(3)

new_struct = """struct relax_type
{
  /* Forward reach. Signed number. > 0.  */
  offsetT rlx_forward;
  /* Backward reach. Signed number. < 0.  */
  offsetT rlx_backward;

  /* Bytes length of this address.  */
  unsigned char rlx_length;

  /* Next longer relax-state.  0 means there is no 'next' relax-state.  */
  relax_substateT rlx_more;
};

typedef struct relax_type relax_typeS;
"""

as_text = as_text.replace(
    as_anchor,
    as_anchor + "\n" + new_struct + "\n",
    1
)

tc_text = tc_text.replace(old_struct, "", 1)

as_tmp = as_path + ".new"
tc_tmp = tc_path + ".new"

for path, content in ((as_tmp, as_text), (tc_tmp, tc_text)):
    output = open(path, "w")
    output.write(content)
    output.close()

import os
os.rename(as_tmp, as_path)
os.rename(tc_tmp, tc_path)

print("Moved relax_type to gas/as.h and changed reach fields to offsetT.")
PYTHON

    echo "Source patch applied."

else
    fail "Unrecognized header state. No source patch applied."
fi

# ============================================================
# Verify the patched source
# ============================================================

grep -q 'offsetT rlx_forward;' "$AS_HEADER" ||
    fail "Missing offsetT rlx_forward in as.h."

grep -q 'offsetT rlx_backward;' "$AS_HEADER" ||
    fail "Missing offsetT rlx_backward in as.h."

grep -q 'typedef struct relax_type relax_typeS;' "$AS_HEADER" ||
    fail "Missing relax_typeS typedef in as.h."

if grep -q 'long rlx_forward;' "$TC_HEADER" ||
   grep -q 'long rlx_backward;' "$TC_HEADER"; then
    fail "Old relax_type fields remain in tc.h."
fi

echo "Source verification passed."
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
# Prepare a clean build directory
# ============================================================

[ "$BUILD_DIR" = "/home/user/sources/build/binutils-2.15" ] ||
    fail "Unexpected build directory path."

if [ -d "$BUILD_DIR" ]; then
    echo "Removing previous build directory."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# ============================================================
# Configure
# ============================================================

cd "$BUILD_DIR" ||
    fail "Cannot enter build directory."

echo "Configuring Binutils 2.15..."

env CC="$GCC4_CC" \
    "$SOURCE_DIR/configure" \
        --prefix=/usr/local \
        --disable-nls \
        --disable-werror

# ============================================================
# Build
# ============================================================

echo
echo "Building Binutils 2.15..."

"$MAKE_EXECUTABLE" -j"$JOBS"

# ============================================================
# Verify build outputs
# ============================================================

for OUTPUT in \
    "${BUILD_DIR}/gas/as-new" \
    "${BUILD_DIR}/ld/ld-new" \
    "${BUILD_DIR}/binutils/ar" \
    "${BUILD_DIR}/binutils/ranlib"
do
    [ -x "$OUTPUT" ] ||
        fail "Expected build output missing: ${OUTPUT}"
done

# ============================================================
# Verify versions
# ============================================================

"${BUILD_DIR}/gas/as-new" --version
"${BUILD_DIR}/ld/ld-new" --version

"${BUILD_DIR}/gas/as-new" --version |
    grep -q '2\.15' ||
    fail "Assembler version check failed."

"${BUILD_DIR}/ld/ld-new" --version |
    grep -q '2\.15' ||
    fail "Linker version check failed."

# ============================================================
# Success
# ============================================================

echo
echo "SUCCESS: Binutils 2.15 build completed."
echo "Assembler: ${BUILD_DIR}/gas/as-new"
echo "Linker: ${BUILD_DIR}/ld/ld-new"
echo "ar: ${BUILD_DIR}/binutils/ar"
echo "ranlib: ${BUILD_DIR}/binutils/ranlib"
echo "No installation or staging performed."
echo "Log: ${LOG_FILE}"

exit 0