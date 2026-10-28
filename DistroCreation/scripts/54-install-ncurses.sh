#!/bin/sh

# ============================================================
# LVGD486 - Install ncurses 5.4
#
# Install ncurses headers, static libraries, shared libraries,
# terminal utilities, tack, and the compiled terminfo database.
#
# Build source:
#   /home/user/sources/ncurses
#
# Staging root:
#   /home/user/distrodev
#
# Headers, libraries, utilities:
#   /home/user/distrodev/usr/include
#   /home/user/distrodev/usr/lib
#   /home/user/distrodev/usr/bin
#
# Terminfo database:
#   /home/user/distrodev/usr/share/terminfo
#
# Compatibility symlink:
#   /home/user/distrodev/usr/local/share/terminfo
#       -> ../../share/terminfo
#
# No installation into the host system is performed.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCE_DIR="/home/user/sources/ncurses"
DISTRODEV_ROOT="/home/user/distrodev"

NCURSES_VERSION="5.4"

INCLUDE_DIR="${DISTRODEV_ROOT}/usr/include"
LIB_DIR="${DISTRODEV_ROOT}/usr/lib"
BIN_DIR="${DISTRODEV_ROOT}/usr/bin"

# Canonical terminfo database location.
SHARE_DIR="${DISTRODEV_ROOT}/usr/share"
TERMINFO_DIR="${SHARE_DIR}/terminfo"

# Compatibility link for software expecting /usr/local/share.
LOCAL_SHARE_DIR="${DISTRODEV_ROOT}/usr/local/share"
TERMINFO_LINK="${LOCAL_SHARE_DIR}/terminfo"
TERMINFO_LINK_TARGET="../../share/terminfo"

TERMINFO_SOURCE="${SOURCE_DIR}/misc/terminfo.src"

UTILITIES="tic toe infocmp clear tput tset tack"

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo >&2
    echo "ERROR: $*" >&2
    echo "Installation stopped." >&2
    exit 1
}

run()
{
    echo "RUN: $*"
    "$@" || fail "Command failed: $*"
}

# ============================================================
# Prerequisites
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Staging root does not exist: ${DISTRODEV_ROOT}"

[ -d "$SOURCE_DIR" ] ||
    fail "ncurses source directory not found: ${SOURCE_DIR}"

[ -f "$TERMINFO_SOURCE" ] ||
    fail "Terminfo source not found: ${TERMINFO_SOURCE}"

[ -d "${SOURCE_DIR}/include" ] ||
    fail "ncurses source include directory not found."

[ -f "${SOURCE_DIR}/include/curses.h" ] ||
    fail "ncurses public header not found."

for LIBRARY in ncurses form menu panel
do
    [ -s "${SOURCE_DIR}/lib/lib${LIBRARY}.a" ] ||
        fail "Static library missing: ${SOURCE_DIR}/lib/lib${LIBRARY}.a. Run 22-build-ncurses.sh first."
done

[ -s "${SOURCE_DIR}/lib/libncurses.so.5.4" ] ||
    fail "Versioned ncurses shared library missing: ${SOURCE_DIR}/lib/libncurses.so.5.4."

[ -L "${SOURCE_DIR}/lib/libncurses.so.5" ] ||
    fail "Expected shared-library symlink missing: ${SOURCE_DIR}/lib/libncurses.so.5"

[ -L "${SOURCE_DIR}/lib/libncurses.so" ] ||
    fail "Expected linker symlink missing: ${SOURCE_DIR}/lib/libncurses.so"

for UTILITY in tic toe infocmp clear tput tset
do
    [ -x "${SOURCE_DIR}/progs/${UTILITY}" ] ||
        fail "Built utility missing: ${SOURCE_DIR}/progs/${UTILITY}. Run 22-build-ncurses.sh first."
done

[ -x "${SOURCE_DIR}/tack/tack" ] ||
    fail "Built tack executable missing. Run 22-build-ncurses.sh first."

echo "============================================================"
echo "LVGD486 - Install ncurses ${NCURSES_VERSION}"
echo "============================================================"
echo "Started:"
date
echo
echo "Build source:       ${SOURCE_DIR}"
echo "Staging root:       ${DISTRODEV_ROOT}"
echo "Terminfo source:    ${TERMINFO_SOURCE}"
echo "Terminfo target:    ${TERMINFO_DIR}"
echo "Compatibility link: ${TERMINFO_LINK}"
echo "Link target:        ${TERMINFO_LINK_TARGET}"
echo "Static libraries:   enabled"
echo "Shared libraries:   enabled"
echo "Host installation:  disabled"
echo

# ============================================================
# Create staging directories
# ============================================================

echo
echo "============================================================"
echo "Preparing staging directories"
echo "============================================================"

run mkdir -p "$INCLUDE_DIR"
run mkdir -p "$LIB_DIR"
run mkdir -p "$BIN_DIR"
run mkdir -p "$SHARE_DIR"
run mkdir -p "$LOCAL_SHARE_DIR"
run mkdir -p "$TERMINFO_DIR"

# ============================================================
# Install ncurses headers
# ============================================================

echo
echo "============================================================"
echo "Installing ncurses headers"
echo "============================================================"

run cp -a "${SOURCE_DIR}/include/." "$INCLUDE_DIR/"

[ -s "${INCLUDE_DIR}/curses.h" ] ||
    fail "curses.h was not installed."

echo "Verified public header: ${INCLUDE_DIR}/curses.h"

# ============================================================
# Install static libraries
# ============================================================

echo
echo "============================================================"
echo "Installing ncurses static libraries"
echo "============================================================"

for LIBRARY in ncurses form menu panel
do
    SOURCE_LIBRARY="${SOURCE_DIR}/lib/lib${LIBRARY}.a"
    TARGET_LIBRARY="${LIB_DIR}/lib${LIBRARY}.a"

    [ -s "$SOURCE_LIBRARY" ] ||
        fail "Static library missing: ${SOURCE_LIBRARY}"

    run install -m 644 "$SOURCE_LIBRARY" "$TARGET_LIBRARY"

    [ -s "$TARGET_LIBRARY" ] ||
        fail "Static library installation failed: ${TARGET_LIBRARY}"

    echo "Installed: ${TARGET_LIBRARY}"
done

# ============================================================
# Install shared libraries
# ============================================================

echo
echo "============================================================"
echo "Installing ncurses shared libraries"
echo "============================================================"

for LIBRARY in ncurses form menu panel
do
    FOUND_SHARED=0

    for SOURCE_SHARED in "${SOURCE_DIR}/lib/lib${LIBRARY}.so"*
    do
        if [ ! -f "$SOURCE_SHARED" ] && [ ! -L "$SOURCE_SHARED" ]; then
            continue
        fi

        TARGET_SHARED="${LIB_DIR}/$(basename "$SOURCE_SHARED")"

        echo "Installing shared-library file: ${SOURCE_SHARED}"
        run cp -a "$SOURCE_SHARED" "$TARGET_SHARED"

        FOUND_SHARED=1
    done

    if [ "$LIBRARY" = "ncurses" ] && [ "$FOUND_SHARED" -eq 0 ]; then
        fail "No ncurses shared-library files were installed."
    fi
done

# Verify the required ncurses shared-library chain.

for SHARED_NAME in libncurses.so libncurses.so.5 libncurses.so.5.4
do
    SHARED_PATH="${LIB_DIR}/${SHARED_NAME}"

    if [ ! -f "$SHARED_PATH" ] && [ ! -L "$SHARED_PATH" ]; then
        fail "Required shared-library file missing: ${SHARED_PATH}"
    fi

    echo "Verified shared-library path: ${SHARED_PATH}"
    ls -l "$SHARED_PATH"
done

[ -f "${LIB_DIR}/libncurses.so.5.4" ] ||
    fail "The ncurses shared-library target is missing or broken."

[ -f "${LIB_DIR}/libncurses.so.5" ] ||
    fail "libncurses.so.5 does not resolve to an existing file."

[ -f "${LIB_DIR}/libncurses.so" ] ||
    fail "libncurses.so does not resolve to an existing file."

echo
echo "Installed ncurses shared-library files:"
ls -l "${LIB_DIR}"/libncurses.so*

# ============================================================
# Install terminal database utilities
# ============================================================

echo
echo "============================================================"
echo "Installing terminal database utilities"
echo "============================================================"

for UTILITY in tic toe infocmp clear tput tset
do
    run install -m 755 \
        "${SOURCE_DIR}/progs/${UTILITY}" \
        "${BIN_DIR}/${UTILITY}"
done

# ============================================================
# Install tack
# ============================================================

echo
echo "============================================================"
echo "Installing tack"
echo "============================================================"

run install -m 755 \
    "${SOURCE_DIR}/tack/tack" \
    "${BIN_DIR}/tack"

# ============================================================
# Compile terminfo database in the canonical location
# ============================================================

echo
echo "============================================================"
echo "Compiling terminfo database"
echo "============================================================"

# Compile directly into /usr/share/terminfo in the staging tree.
#
# LD_LIBRARY_PATH and TERMINFO apply only to the tic process.
# The host environment and host installation remain untouched.

if ! LD_LIBRARY_PATH="${LIB_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    TERMINFO="$TERMINFO_DIR" \
    "${BIN_DIR}/tic" -o "$TERMINFO_DIR" "$TERMINFO_SOURCE"
then
    fail "Could not compile the terminfo database."
fi

# ============================================================
# Verify required terminfo entries
# ============================================================

echo
echo "============================================================"
echo "Verifying compiled terminfo entries"
echo "============================================================"

for ENTRY in linux vt100
do
    ENTRY_FIRST_CHAR="$(printf '%s' "$ENTRY" | cut -c 1)"
    ENTRY_FILE="${TERMINFO_DIR}/${ENTRY_FIRST_CHAR}/${ENTRY}"

    [ -s "$ENTRY_FILE" ] ||
        fail "Compiled terminfo entry missing: ${ENTRY_FILE}"

    echo "Verified entry file: ${ENTRY_FILE}"
    ls -l "$ENTRY_FILE"
done

# ============================================================
# Test entries using staged infocmp
# ============================================================

echo
echo "============================================================"
echo "Testing terminfo database with staged infocmp"
echo "============================================================"

for ENTRY in vt100 linux
do
    echo
    echo "Testing terminal type: ${ENTRY}"

    if LD_LIBRARY_PATH="${LIB_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
       TERMINFO="$TERMINFO_DIR" \
       TERM="$ENTRY" \
       "${BIN_DIR}/infocmp" "$ENTRY" >/dev/null
    then
        echo "PASS: staged infocmp can read ${ENTRY}"
    else
        fail "Staged infocmp cannot read terminfo entry: ${ENTRY}"
    fi
done

# ============================================================
# Create compatibility symlink after database installation
# ============================================================

echo
echo "============================================================"
echo "Creating terminfo compatibility symlink"
echo "============================================================"

# Refuse to overwrite an actual directory or regular file.
# An existing symlink may safely be replaced.

if [ -e "$TERMINFO_LINK" ] && [ ! -L "$TERMINFO_LINK" ]; then
    fail "Refusing to replace existing non-symlink: ${TERMINFO_LINK}"
fi

if [ -L "$TERMINFO_LINK" ]; then
    run rm "$TERMINFO_LINK"
fi

run ln -s "$TERMINFO_LINK_TARGET" "$TERMINFO_LINK"

[ -L "$TERMINFO_LINK" ] ||
    fail "Terminfo compatibility symlink was not created."

[ -d "$TERMINFO_LINK" ] ||
    fail "Terminfo compatibility symlink does not resolve to a directory."

[ -s "${TERMINFO_LINK}/l/linux" ] ||
    fail "Linux terminfo entry is not accessible through the compatibility symlink."

[ -s "${TERMINFO_LINK}/v/vt100" ] ||
    fail "VT100 terminfo entry is not accessible through the compatibility symlink."

echo "Verified compatibility symlink:"
ls -ld "$TERMINFO_LINK"

# ============================================================
# Verify installed headers
# ============================================================

echo
echo "============================================================"
echo "Verifying installed ncurses headers"
echo "============================================================"

for HEADER in curses.h ncurses.h term.h unctrl.h
do
    if [ -f "${INCLUDE_DIR}/${HEADER}" ]; then
        echo "Verified: ${INCLUDE_DIR}/${HEADER}"
    else
        echo "NOTICE: Header not present at expected path: ${INCLUDE_DIR}/${HEADER}"
    fi
done

[ -s "${INCLUDE_DIR}/curses.h" ] ||
    fail "Required ncurses header was not installed."

# ============================================================
# Verify installed static libraries
# ============================================================

echo
echo "============================================================"
echo "Verifying installed static libraries"
echo "============================================================"

for LIBRARY in ncurses form menu panel
do
    LIBRARY_PATH="${LIB_DIR}/lib${LIBRARY}.a"

    [ -s "$LIBRARY_PATH" ] ||
        fail "Static library not installed: ${LIBRARY_PATH}"

    echo "Verified: ${LIBRARY_PATH}"
    ls -l "$LIBRARY_PATH"
done

# ============================================================
# Verify installed shared libraries
# ============================================================

echo
echo "============================================================"
echo "Verifying installed shared libraries"
echo "============================================================"

[ -s "${LIB_DIR}/libncurses.so.5.4" ] ||
    fail "Versioned ncurses shared library is missing or empty."

[ -f "${LIB_DIR}/libncurses.so.5" ] ||
    fail "Versioned runtime link libncurses.so.5 is broken."

[ -f "${LIB_DIR}/libncurses.so" ] ||
    fail "Linker symlink libncurses.so is broken."

echo "Verified runtime library: ${LIB_DIR}/libncurses.so.5"
echo "Verified linker library:  ${LIB_DIR}/libncurses.so"
echo "Verified library target:  ${LIB_DIR}/libncurses.so.5.4"

# ============================================================
# Verify installed utilities
# ============================================================

echo
echo "============================================================"
echo "Verifying installed utilities"
echo "============================================================"

for UTILITY in $UTILITIES
do
    TOOL_PATH="${BIN_DIR}/${UTILITY}"

    [ -x "$TOOL_PATH" ] ||
        fail "Utility not installed or not executable: ${TOOL_PATH}"

    echo "Verified executable: ${TOOL_PATH}"
done

# ============================================================
# Inspect executable formats when file is available
# ============================================================

echo
echo "============================================================"
echo "Inspecting installed executable formats"
echo "============================================================"

if [ -x /usr/bin/file ]; then
    FILE_EXECUTABLE="/usr/bin/file"
elif [ -x /bin/file ]; then
    FILE_EXECUTABLE="/bin/file"
else
    FILE_EXECUTABLE=""
fi

if [ -n "$FILE_EXECUTABLE" ]; then
    for UTILITY in $UTILITIES
    do
        echo
        echo "Executable: ${BIN_DIR}/${UTILITY}"

        "$FILE_EXECUTABLE" "${BIN_DIR}/${UTILITY}" ||
            fail "Could not inspect executable: ${BIN_DIR}/${UTILITY}"
    done
else
    echo "NOTICE: 'file' is unavailable; skipping executable format inspection."
fi

# ============================================================
# Installation summary
# ============================================================

echo
echo "============================================================"
echo "Installed ncurses ${NCURSES_VERSION}"
echo "============================================================"

echo
echo "Headers:"
ls -ld "$INCLUDE_DIR"
ls -l "${INCLUDE_DIR}/curses.h"

echo
echo "Static libraries:"
ls -l \
    "${LIB_DIR}/libncurses.a" \
    "${LIB_DIR}/libform.a" \
    "${LIB_DIR}/libmenu.a" \
    "${LIB_DIR}/libpanel.a"

echo
echo "Shared ncurses libraries:"
ls -l "${LIB_DIR}"/libncurses.so*

echo
echo "Utilities:"
ls -l \
    "${BIN_DIR}/tic" \
    "${BIN_DIR}/toe" \
    "${BIN_DIR}/infocmp" \
    "${BIN_DIR}/clear" \
    "${BIN_DIR}/tput" \
    "${BIN_DIR}/tset" \
    "${BIN_DIR}/tack"

echo
echo "Canonical terminfo entries:"
ls -l \
    "${TERMINFO_DIR}/l/linux" \
    "${TERMINFO_DIR}/v/vt100"

echo
echo "Terminfo installation path:"
echo "${TERMINFO_DIR}"

echo
echo "Terminfo compatibility link:"
ls -ld "$TERMINFO_LINK"

echo
echo "Staging root: ${DISTRODEV_ROOT}"
echo "Static libraries: installed"
echo "Shared ncurses runtime library: installed"
echo "Terminfo database: installed"
echo "Terminfo compatibility symlink: installed"
echo "Host system libraries were not modified."

echo
echo "SUCCESS: ncurses ${NCURSES_VERSION} staged in ${DISTRODEV_ROOT}"

exit 0