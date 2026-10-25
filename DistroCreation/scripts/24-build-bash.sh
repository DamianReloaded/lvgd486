
#!/bin/sh

# ============================================================
# LVGD486 - Build Bash 2.05b
# Script: 24-build-bash.sh
#
# Build Bash using GCC 3.3 with size optimization and static
# linking. Strip debug and symbol information after verification.
#
# No installation or staging performed.
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

echo "LVGD486 - Bash build started"
echo "Log: ${LOG_FILE}"
echo

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/bash"

GCC3_CC="/usr/bin/gcc-3.3"
MAKE_EXECUTABLE="/usr/local/bin/make"
STRIP_EXECUTABLE="/usr/bin/strip"

EXPECTED_MAKE_VERSION="3.81"
EXPECTED_BASH_VERSION="2.05b"

CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"
MAKEFILE="${SOURCE_DIR}/Makefile"
BASH_EXECUTABLE="${SOURCE_DIR}/bash"
UNSTRIPPED_BACKUP="${SOURCE_DIR}/bash.unstripped"

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

[ -x "$GCC3_CC" ] ||
    fail "GCC 3.3 not found: ${GCC3_CC}"

GCC_VERSION_FOUND=$("$GCC3_CC" -dumpversion)

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

[ "$MAKE_VERSION_FOUND" = "$EXPECTED_MAKE_VERSION" ] ||
    fail "Expected Make ${EXPECTED_MAKE_VERSION}; found ${MAKE_VERSION_FOUND}"

[ -x "$STRIP_EXECUTABLE" ] ||
    fail "strip utility not found: ${STRIP_EXECUTABLE}"

[ -d "$SOURCE_DIR" ] ||
    fail "Bash source directory not found: ${SOURCE_DIR}"

[ -x "$CONFIGURE_SCRIPT" ] ||
    fail "Bash configure script not found: ${CONFIGURE_SCRIPT}"

echo "Compiler: ${GCC3_CC}"
echo "GCC version: ${GCC_VERSION_FOUND}"
echo "Make: ${MAKE_EXECUTABLE}"
echo "Make version: ${MAKE_VERSION_FOUND}"
echo "Strip: ${STRIP_EXECUTABLE}"
echo "Source: ${SOURCE_DIR}"
echo "Optimization: -Os"
echo "Link mode: static"
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
unset CONFIG_SITE

# ============================================================
# Clean previous build
# ============================================================

cd "$SOURCE_DIR" ||
    fail "Cannot enter Bash source directory."

echo
echo "Cleaning previous Bash build artifacts..."

if [ -f "$MAKEFILE" ]; then
    if grep -q '^distclean:' "$MAKEFILE"; then
        "$MAKE_EXECUTABLE" distclean ||
            fail "make distclean failed."
    elif grep -q '^clean:' "$MAKEFILE"; then
        "$MAKE_EXECUTABLE" clean ||
            fail "make clean failed."
    else
        echo "No clean target found in the existing Makefile."
    fi
else
    echo "No existing Makefile; nothing to clean."
fi

rm -f \
    "$BASH_EXECUTABLE" \
    "$UNSTRIPPED_BACKUP" \
    "${SOURCE_DIR}/bashbug" \
    "${SOURCE_DIR}/config.cache" \
    "${SOURCE_DIR}/config.log" \
    "${SOURCE_DIR}/config.status"

# ============================================================
# Configure
# ============================================================

cd "$SOURCE_DIR" ||
    fail "Cannot enter Bash source directory."

echo
echo "Configuring Bash ${EXPECTED_BASH_VERSION}..."

# Bash 2.05b may not provide --enable-static-link.
# Request static linking through LDFLAGS.
# -Os asks GCC to optimize for executable size.

if "$CONFIGURE_SCRIPT" --help 2>&1 |
    grep -q -- '--enable-static-link'; then

    env \
        CC="$GCC3_CC" \
        CFLAGS="-Os" \
        LDFLAGS="-static" \
        "$CONFIGURE_SCRIPT" \
            --prefix=/usr/local \
            --disable-nls \
            --enable-static-link
else
    env \
        CC="$GCC3_CC" \
        CFLAGS="-Os" \
        LDFLAGS="-static" \
        "$CONFIGURE_SCRIPT" \
            --prefix=/usr/local \
            --disable-nls
fi

# ============================================================
# Verify configure output
# ============================================================

[ -f "$MAKEFILE" ] ||
    fail "Configure did not generate Makefile."

echo
echo "Configure completed."
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
# Build
# ============================================================

echo
echo "Building Bash ${EXPECTED_BASH_VERSION}..."
echo "Compiler flags: -Os"
echo "Linker flags: -static"

"$MAKE_EXECUTABLE" \
    CFLAGS="-Os" \
    LDFLAGS="-static" \
    -j"$JOBS"

# ============================================================
# Verify build output
# ============================================================

[ -s "$BASH_EXECUTABLE" ] ||
    fail "Built Bash executable missing or empty."

[ -x "$BASH_EXECUTABLE" ] ||
    fail "Built Bash executable is not executable."

# ============================================================
# Verify Bash version before stripping
# ============================================================

echo
echo "Checking Bash version before stripping..."

BASH_VERSION_OUTPUT=$("$BASH_EXECUTABLE" --version 2>&1)

echo "$BASH_VERSION_OUTPUT"

echo "$BASH_VERSION_OUTPUT" |
    grep -q "version ${EXPECTED_BASH_VERSION}" ||
    fail "Bash version check failed."

# ============================================================
# Basic execution test before stripping
# ============================================================

echo
echo "Running basic Bash execution test..."

TEST_OUTPUT=$(
    "$BASH_EXECUTABLE" -c 'echo LVGD486_BASH_TEST'
) || fail "Bash execution test failed."

[ "$TEST_OUTPUT" = "LVGD486_BASH_TEST" ] ||
    fail "Unexpected output from Bash execution test: ${TEST_OUTPUT}"

echo "$TEST_OUTPUT"
echo "Basic execution test passed."

# ============================================================
# Verify static linking before stripping
# ============================================================

echo
echo "Checking executable linkage before stripping..."

if command -v file >/dev/null 2>&1; then
    FILE_OUTPUT=$(file "$BASH_EXECUTABLE" 2>&1)
    echo "$FILE_OUTPUT"

    echo "$FILE_OUTPUT" |
        grep -q 'statically linked' ||
        fail "Bash executable is not confirmed statically linked."
else
    fail "The file utility is required to verify static linkage."
fi

if command -v ldd >/dev/null 2>&1; then
    LDD_OUTPUT=$(ldd "$BASH_EXECUTABLE" 2>&1 || true)
    echo "$LDD_OUTPUT"

    case "$LDD_OUTPUT" in
        *"not a dynamic executable"*|*"statically linked"*)
            echo "Static linkage confirmed."
            ;;
        *)
            fail "Could not confirm static linking."
            ;;
    esac
fi

# ============================================================
# Record unstripped executable size
# ============================================================

echo
echo "============================================================"
echo "Recording unstripped executable"
echo "============================================================"

UNSTRIPPED_SIZE=$(wc -c < "$BASH_EXECUTABLE" |
    tr -d ' ')

echo "Unstripped file size: ${UNSTRIPPED_SIZE} bytes"

echo
echo "Saving unstripped backup: ${UNSTRIPPED_BACKUP}"

cp -p "$BASH_EXECUTABLE" "$UNSTRIPPED_BACKUP" ||
    fail "Could not save unstripped Bash backup."

[ -s "$UNSTRIPPED_BACKUP" ] ||
    fail "Unstripped backup is missing or empty."

# ============================================================
# Strip executable
# ============================================================

echo
echo "============================================================"
echo "Stripping Bash executable"
echo "============================================================"

"$STRIP_EXECUTABLE" --strip-all "$BASH_EXECUTABLE" ||
    fail "Could not strip Bash executable."

[ -s "$BASH_EXECUTABLE" ] ||
    fail "Stripped Bash executable is missing or empty."

STRIPPED_SIZE=$(wc -c < "$BASH_EXECUTABLE" |
    tr -d ' ')

echo "Unstripped size: ${UNSTRIPPED_SIZE} bytes"
echo "Stripped size:   ${STRIPPED_SIZE} bytes"

if [ "$UNSTRIPPED_SIZE" -gt 0 ]; then
    SAVED_BYTES=$((UNSTRIPPED_SIZE - STRIPPED_SIZE))
    SAVED_PERCENT=$((SAVED_BYTES * 100 / UNSTRIPPED_SIZE))

    echo "Space saved: ${SAVED_BYTES} bytes (${SAVED_PERCENT}%)"
fi

# ============================================================
# Verify stripped executable
# ============================================================

echo
echo "============================================================"
echo "Verifying stripped executable"
echo "============================================================"

chmod 755 "$BASH_EXECUTABLE"

BASH_VERSION_OUTPUT=$("$BASH_EXECUTABLE" --version 2>&1) ||
    fail "Stripped Bash failed to run."

echo "$BASH_VERSION_OUTPUT"

echo "$BASH_VERSION_OUTPUT" |
    grep -q "version ${EXPECTED_BASH_VERSION}" ||
    fail "Stripped Bash version check failed."

TEST_OUTPUT=$(
    "$BASH_EXECUTABLE" -c 'echo LVGD486_BASH_TEST'
) || fail "Stripped Bash execution test failed."

[ "$TEST_OUTPUT" = "LVGD486_BASH_TEST" ] ||
    fail "Stripped Bash produced unexpected test output."

echo "Stripped Bash execution test passed."

# ============================================================
# Verify stripped executable remains statically linked
# ============================================================

echo
echo "Checking static linkage after stripping..."

FILE_OUTPUT=$(file "$BASH_EXECUTABLE" 2>&1)
echo "$FILE_OUTPUT"

echo "$FILE_OUTPUT" |
    grep -q 'statically linked' ||
    fail "Stripped Bash is not confirmed statically linked."

if command -v ldd >/dev/null 2>&1; then
    LDD_OUTPUT=$(ldd "$BASH_EXECUTABLE" 2>&1 || true)
    echo "$LDD_OUTPUT"

    case "$LDD_OUTPUT" in
        *"not a dynamic executable"*|*"statically linked"*)
            echo "Stripped executable remains statically linked."
            ;;
        *)
            fail "Static linkage check failed after stripping."
            ;;
    esac
fi

# ============================================================
# Final executable information
# ============================================================

echo
echo "============================================================"
echo "Final Bash executable"
echo "============================================================"

ls -l "$BASH_EXECUTABLE"
size "$BASH_EXECUTABLE"
file "$BASH_EXECUTABLE"

# ============================================================
# Success
# ============================================================

echo
echo "SUCCESS: Bash ${EXPECTED_BASH_VERSION} build completed."
echo "Compiler: ${GCC3_CC}"
echo "Optimization: -Os"
echo "Static linking: verified"
echo "Stripped executable: ${BASH_EXECUTABLE}"
echo "Unstripped backup: ${UNSTRIPPED_BACKUP}"
echo "No installation or staging performed."
echo "Log: ${LOG_FILE}"

exit 0
