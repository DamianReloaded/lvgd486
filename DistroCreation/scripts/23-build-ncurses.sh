
#!/bin/sh

# ============================================================
# LVGD486 - Build ncurses 5.4
# Script: 22-build-ncurses.sh
#
# Build static and shared ncurses libraries using:
#   GCC 4.0.2
#   GNU Make 3.81
#
# Statically linked executables:
#   tic, toe, infocmp, clear, tput, tset, tack
#
# Libraries produced:
#   Static:  libncurses.a, libform.a, libmenu.a, libpanel.a
#   Shared:  libncurses.so and versioned shared-library output
#
# Patches:
#   - progs/Makefile.in: link six utilities against the static
#     ncurses archive instead of the shared ncurses library.
#   - tack/Makefile.in: link tack against the static ncurses
#     archive instead of the shared ncurses library.
#
# Behavior:
#   - Restores original templates from backups when available.
#   - Cleans previous build artifacts before configuring.
#   - Creates fresh backups before applying patches.
#   - Traces selected terminfo symbols during executable linking.
#   - Builds both static and shared ncurses libraries.
#   - Verifies that all seven executables are statically linked.
#   - Does not install or stage files.
#
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

echo "============================================================"
echo "LVGD486 - ncurses 5.4 static/shared build"
echo "============================================================"
echo "Started:"
date
echo "Log: ${LOG_FILE}"
echo

# ============================================================
# Constants
# ============================================================

SOURCE_DIR="/home/user/sources/ncurses"

PROGS_MAKEFILE_IN="${SOURCE_DIR}/progs/Makefile.in"
PROGS_MAKEFILE_BACKUP="${PROGS_MAKEFILE_IN}.lvgd486-backup"

TACK_MAKEFILE_IN="${SOURCE_DIR}/tack/Makefile.in"
TACK_MAKEFILE_BACKUP="${TACK_MAKEFILE_IN}.lvgd486-backup"

GCC4_CC="/usr/local/gcc-4.0.2/bin/gcc"
MAKE_EXECUTABLE="/usr/local/bin/make"

STATIC_LDFLAGS="-static"

LINK_TRACE_FLAGS="\
-Wl,--trace-symbol=_nc_tic_expand \
-Wl,--trace-symbol=_nc_infotocap \
-Wl,--trace-symbol=strnames \
-Wl,--trace-symbol=_nc_user_definable \
-Wl,--trace-symbol=_nc_first_name"

STATIC_NCURSES_ARCHIVE="../lib/libncurses.a"

JOBS=1

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

[ -f "${SOURCE_DIR}/Makefile.in" ] ||
    fail "Top-level Makefile.in not found."

[ -d "${SOURCE_DIR}/ncurses" ] ||
    fail "ncurses library source directory is missing."

[ -d "${SOURCE_DIR}/progs" ] ||
    fail "ncurses programs source directory is missing."

[ -d "${SOURCE_DIR}/include" ] ||
    fail "ncurses include directory is missing."

[ -d "${SOURCE_DIR}/tack" ] ||
    fail "tack source directory is missing."

[ -f "$PROGS_MAKEFILE_IN" ] ||
    fail "Programs Makefile template not found: ${PROGS_MAKEFILE_IN}"

[ -f "$TACK_MAKEFILE_IN" ] ||
    fail "tack Makefile template not found: ${TACK_MAKEFILE_IN}"

echo "User:"
id
echo
echo "Compiler: ${GCC4_CC}"
echo "Compiler version: ${GCC_VERSION_FOUND}"
echo "Static executable linker flags: ${STATIC_LDFLAGS}"
echo "Static ncurses archive: ${STATIC_NCURSES_ARCHIVE}"
echo "Link trace flags: ${LINK_TRACE_FLAGS}"
echo "Make: ${MAKE_EXECUTABLE}"
echo "Make version: ${MAKE_VERSION_FOUND}"
echo "Source: ${SOURCE_DIR}"
echo "Parallel build jobs: ${JOBS}"
echo "Library policy: static and shared"
echo "Executable policy: fully static"
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
# Prepare source tree
# ============================================================

[ "$SOURCE_DIR" = "/home/user/sources/ncurses" ] ||
    fail "Unexpected source directory path."

cd "$SOURCE_DIR" ||
    fail "Cannot enter source directory."

# ============================================================
# Restore original templates when backups exist
# ============================================================

echo
echo "============================================================"
echo "Restoring original Makefile.in templates"
echo "============================================================"

if [ -f "$PROGS_MAKEFILE_BACKUP" ]; then
    echo "Restoring ${PROGS_MAKEFILE_IN} from existing backup."
    cp -p "$PROGS_MAKEFILE_BACKUP" "$PROGS_MAKEFILE_IN" ||
        fail "Could not restore programs Makefile.in."
else
    echo "No programs backup found; using existing source template."
fi

if [ -f "$TACK_MAKEFILE_BACKUP" ]; then
    echo "Restoring ${TACK_MAKEFILE_IN} from existing backup."
    cp -p "$TACK_MAKEFILE_BACKUP" "$TACK_MAKEFILE_IN" ||
        fail "Could not restore tack Makefile.in."
else
    echo "No tack backup found; using existing source template."
fi

# ============================================================
# Clean previous build and configuration
# ============================================================

echo
echo "============================================================"
echo "Cleaning previous build and configuration"
echo "============================================================"

if [ -f Makefile ]; then
    if ! "$MAKE_EXECUTABLE" distclean; then
        echo "WARNING: distclean failed; continuing with manual cleanup."
    fi
else
    echo "No previous top-level Makefile found."
fi

rm -f \
    config.cache \
    config.log \
    config.status \
    Makefile \
    progs/Makefile \
    tack/Makefile

rm -f \
    include/config.h \
    include/ncurses_cfg.h \
    include/ncurses_def.h

for BUILD_DIR in objects lib
do
    if [ -e "$BUILD_DIR" ]; then
        echo "Removing generated directory: ${BUILD_DIR}"
        rm -rf "$BUILD_DIR"
    fi
done

for OUTPUT in \
    progs/tic \
    progs/infocmp \
    progs/clear \
    progs/tput \
    progs/toe \
    progs/tset \
    progs/tabs \
    progs/transform.h \
    tack/tack
do
    rm -f "$OUTPUT"
done

find ncurses progs tack -type f \
    \( -name '*.o' -o -name '*.lo' -o -name '*.d' \) \
    -exec rm -f {} \; 2>/dev/null || true

find ncurses -type f \
    \( -name '*.a' -o -name '*.so' -o -name '*.so.*' \) \
    -exec rm -f {} \; 2>/dev/null || true

rm -f \
    "${PROGS_MAKEFILE_IN}.lvgd486-tmp" \
    "${PROGS_MAKEFILE_IN}.lvgd486-tmp.new" \
    "${TACK_MAKEFILE_IN}.lvgd486-tmp" \
    "${TACK_MAKEFILE_IN}.lvgd486-tmp.new"

echo "Previous build cleanup completed."

# ============================================================
# Create fresh original-template backups
# ============================================================

echo
echo "============================================================"
echo "Creating original Makefile.in backups"
echo "============================================================"

cp -p "$PROGS_MAKEFILE_IN" "$PROGS_MAKEFILE_BACKUP" ||
    fail "Could not back up ${PROGS_MAKEFILE_IN}"

cp -p "$TACK_MAKEFILE_IN" "$TACK_MAKEFILE_BACKUP" ||
    fail "Could not back up ${TACK_MAKEFILE_IN}"

echo "Created backup: ${PROGS_MAKEFILE_BACKUP}"
echo "Created backup: ${TACK_MAKEFILE_BACKUP}"

# ============================================================
# Patch progs/Makefile.in
# ============================================================

echo
echo "============================================================"
echo "Patching progs/Makefile.in for static linking"
echo "============================================================"

PROGS_PATCH_TEMP="${PROGS_MAKEFILE_IN}.lvgd486-tmp"
PROGS_PATCH_OUTPUT="${PROGS_PATCH_TEMP}.new"

rm -f "$PROGS_PATCH_TEMP" "$PROGS_PATCH_OUTPUT"

cp "$PROGS_MAKEFILE_BACKUP" "$PROGS_PATCH_TEMP" ||
    fail "Could not prepare temporary programs Makefile.in."

# Replace any existing DEPS_CURSES link input in each utility
# recipe with the explicit static archive. This prevents the
# generated DEPS_CURSES variable from selecting libncurses.so.
#
# The archive is inserted before LDFLAGS_DEFAULT, where the
# existing Makefile places linker flags and the output option.

if ! awk '
/^[^	 #][^:]*:/ {
    current = ""

    if (index($0, "tic$x:") == 1)
        current = "tic"
    else if (index($0, "toe$x:") == 1)
        current = "toe"
    else if (index($0, "clear$x:") == 1)
        current = "clear"
    else if (index($0, "tput$x:") == 1)
        current = "tput"
    else if (index($0, "infocmp$x:") == 1)
        current = "infocmp"
    else if (index($0, "tset$x:") == 1)
        current = "tset"
}

/^[	 ]*@ECHO_LINK@[	 ]*\$\(LINK\)/ {
    if (current != "") {
        if ($0 !~ /\$\(LDFLAGS_DEFAULT\)/) {
            print "Missing LDFLAGS_DEFAULT in " current \
                " link recipe." > "/dev/stderr"
            failed = 1
        }
        else {
            # Remove any previous static-archive insertion.
            gsub(/\.\.\/lib\/libncurses\.a[	 ]*/, "", $0)

            # Replace DEPS_CURSES if present; otherwise insert
            # the static archive before LDFLAGS_DEFAULT.
            if ($0 ~ /\$\(DEPS_CURSES\)/) {
                sub(/\$\(DEPS_CURSES\)/,
                    "../lib/libncurses.a")
            }
            else {
                sub(/\$\(LDFLAGS_DEFAULT\)/,
                    "../lib/libncurses.a $(LDFLAGS_DEFAULT)")
            }

            # Do not permit a shared ncurses library as input.
            if ($0 ~ /libncurses\.so/) {
                print "Shared ncurses input remains in " current \
                    " link recipe." > "/dev/stderr"
                failed = 1
            }

            if ($0 ~ /libncurses\.a/)
                found[current] = 1
        }
    }
}

{
    print
}

END {
    names[1] = "tic"
    names[2] = "toe"
    names[3] = "clear"
    names[4] = "tput"
    names[5] = "infocmp"
    names[6] = "tset"

    for (i = 1; i <= 6; i++) {
        if (!found[names[i]]) {
            print "Missing or unpatched static link recipe: " \
                names[i] > "/dev/stderr"
            failed = 1
        }
    }

    if (failed)
        exit 1
}
' "$PROGS_PATCH_TEMP" > "$PROGS_PATCH_OUTPUT"
then
    rm -f "$PROGS_PATCH_TEMP" "$PROGS_PATCH_OUTPUT"
    fail "Could not patch all six utility link recipes."
fi

mv "$PROGS_PATCH_OUTPUT" "$PROGS_PATCH_TEMP" ||
    fail "Could not finalize programs Makefile.in patch."

mv "$PROGS_PATCH_TEMP" "$PROGS_MAKEFILE_IN" ||
    fail "Could not install patched programs Makefile.in."

echo "Patched utility link recipes:"
grep -n -E '^[	 ]*@ECHO_LINK@[	 ]*\$\(LINK\)' \
    "$PROGS_MAKEFILE_IN" || true

# ============================================================
# Patch tack/Makefile.in
# ============================================================

echo
echo "============================================================"
echo "Patching tack/Makefile.in for static linking"
echo "============================================================"

TACK_PATCH_TEMP="${TACK_MAKEFILE_IN}.lvgd486-tmp"
TACK_PATCH_OUTPUT="${TACK_PATCH_TEMP}.new"

rm -f "$TACK_PATCH_TEMP" "$TACK_PATCH_OUTPUT"

cp "$TACK_MAKEFILE_BACKUP" "$TACK_PATCH_TEMP" ||
    fail "Could not prepare temporary tack Makefile.in."

# Keep DEPS_TACK, but replace the curses link input with the
# static ncurses archive.

if ! awk '
/^[^	 #][^:]*:/ {
    current = ""

    if (index($0, "tack$x:") == 1)
        current = "tack"
}

/^[	 ]*@ECHO_LINK@[	 ]*\$\(LINK\)/ {
    if (current == "tack") {
        if ($0 !~ /\$\(DEPS_TACK\)/) {
            print "tack link recipe lacks DEPS_TACK." > "/dev/stderr"
            failed = 1
        }

        if ($0 !~ /\$\(LDFLAGS_DEFAULT\)/) {
            print "tack link recipe lacks LDFLAGS_DEFAULT." \
                > "/dev/stderr"
            failed = 1
        }
        else {
            # Remove any earlier archive insertion.
            gsub(/\.\.\/lib\/libncurses\.a[	 ]*/, "", $0)

            if ($0 ~ /\$\(DEPS_CURSES\)/) {
                sub(/\$\(DEPS_CURSES\)/,
                    "../lib/libncurses.a")
            }
            else {
                sub(/\$\(LDFLAGS_DEFAULT\)/,
                    "../lib/libncurses.a $(LDFLAGS_DEFAULT)")
            }
        }

        if ($0 ~ /libncurses\.so/) {
            print "Shared ncurses input remains in tack recipe." \
                > "/dev/stderr"
            failed = 1
        }

        if ($0 ~ /\$\(DEPS_TACK\)/ &&
            $0 ~ /libncurses\.a/)
            found = 1
    }
}

{
    print
}

END {
    if (!found) {
        print "Missing or unpatched static tack link recipe." \
            > "/dev/stderr"
        failed = 1
    }

    if (failed)
        exit 1
}
' "$TACK_PATCH_TEMP" > "$TACK_PATCH_OUTPUT"
then
    rm -f "$TACK_PATCH_TEMP" "$TACK_PATCH_OUTPUT"
    fail "Could not patch tack link recipe."
fi

mv "$TACK_PATCH_OUTPUT" "$TACK_PATCH_TEMP" ||
    fail "Could not finalize tack Makefile.in patch."

mv "$TACK_PATCH_TEMP" "$TACK_MAKEFILE_IN" ||
    fail "Could not install patched tack Makefile.in."

echo "Patched tack link recipe:"
grep -n -E '^[	 ]*@ECHO_LINK@[	 ]*\$\(LINK\)' \
    "$TACK_MAKEFILE_IN" || true

# ============================================================
# Configure ncurses for static and shared libraries
# ============================================================

echo
echo "============================================================"
echo "Configuring ncurses 5.4 for static and shared libraries"
echo "============================================================"

# Do not pass -static globally. Configure tests and shared
# library construction must be allowed to link normally.

if ! env \
    CC="$GCC4_CC" \
    CFLAGS="-O2" \
    LDFLAGS="" \
    ./configure \
        --prefix=/usr/local \
        --with-normal \
        --with-shared \
        --without-debug \
        --without-profile \
        --without-cxx \
        --without-cxx-binding \
        --without-ada \
        --without-manpages
then
    fail "ncurses configure failed."
fi

[ -f Makefile ] ||
    fail "Configure did not generate the top-level Makefile."

[ -f progs/Makefile ] ||
    fail "Configure did not generate progs/Makefile."

[ -f tack/Makefile ] ||
    fail "Configure did not generate tack/Makefile."

echo "Configure completed successfully."

# ============================================================
# Verify generated utility link recipes
# ============================================================

echo
echo "============================================================"
echo "Verifying generated utility link recipes"
echo "============================================================"

for TARGET in tic toe clear tput infocmp tset
do
    case "$TARGET" in
        tic)     DEP="DEPS_TIC" ;;
        toe)     DEP="DEPS_TOE" ;;
        clear)   DEP="DEPS_CLEAR" ;;
        tput)    DEP="DEPS_TPUT" ;;
        infocmp) DEP="DEPS_INFOCMP" ;;
        tset)    DEP="DEPS_TSET" ;;
    esac

    RECIPE=$(grep -F "$DEP" progs/Makefile |
        grep -F '$(LINK)' |
        head -n 1 || true)

    [ -n "$RECIPE" ] ||
        fail "Could not find generated link recipe for ${TARGET}."

    echo "${TARGET}: ${RECIPE}"

    echo "$RECIPE" | grep -Fq '../lib/libncurses.a' ||
        fail "Generated ${TARGET} link recipe does not use the static ncurses archive."

    echo "$RECIPE" | grep -Fq '$(LDFLAGS_DEFAULT)' ||
        fail "Generated ${TARGET} link recipe lacks LDFLAGS_DEFAULT."

    if echo "$RECIPE" | grep -Fq '../lib/libncurses.so'; then
        fail "Generated ${TARGET} link recipe references the shared ncurses library."
    fi
done

echo "All six utility link recipes use the static ncurses archive."

# ============================================================
# Verify generated tack link recipe
# ============================================================

echo
echo "============================================================"
echo "Verifying generated tack link recipe"
echo "============================================================"

TACK_RECIPE=$(grep -F '$(LINK)' tack/Makefile |
    grep -F '$(DEPS_TACK)' |
    head -n 1 || true)

[ -n "$TACK_RECIPE" ] ||
    fail "Could not find generated tack link recipe."

echo "tack: ${TACK_RECIPE}"

echo "$TACK_RECIPE" | grep -Fq '../lib/libncurses.a' ||
    fail "Generated tack link recipe does not use the static ncurses archive."

echo "$TACK_RECIPE" | grep -Fq '$(LDFLAGS_DEFAULT)' ||
    fail "Generated tack link recipe lacks LDFLAGS_DEFAULT."

if echo "$TACK_RECIPE" | grep -Fq '../lib/libncurses.so'; then
    fail "Generated tack link recipe references the shared ncurses library."
fi

echo "Generated tack link recipe uses the static ncurses archive."

# ============================================================
# Check terminfo compiler sources
# ============================================================

echo
echo "============================================================"
echo "Checking terminfo compiler sources"
echo "============================================================"

for SOURCE_FILE in \
    ncurses/comp_expand.c \
    ncurses/comp_scan.c \
    ncurses/comp_parse.c
do
    if [ -f "$SOURCE_FILE" ]; then
        echo "Found: ${SOURCE_FILE}"
    else
        echo "NOTICE: ${SOURCE_FILE} is not present at this path."
    fi
done

# ============================================================
# Build ncurses static and shared libraries
# ============================================================

echo
echo "============================================================"
echo "Building ncurses 5.4 static and shared libraries"
echo "============================================================"

if ! "$MAKE_EXECUTABLE" \
    "CC=${GCC4_CC}" \
    "CFLAGS=-O2" \
    -j"$JOBS" \
    libs
then
    fail "Building ncurses static and shared libraries failed."
fi

[ -s "${SOURCE_DIR}/lib/libncurses.a" ] ||
    fail "Static ncurses archive was not built."

# ============================================================
# Generate transform.h before compiling utilities
# ============================================================

echo
echo "============================================================"
echo "Generating progs/transform.h"
echo "============================================================"

if ! "$MAKE_EXECUTABLE" \
    -C progs \
    "CC=${GCC4_CC}" \
    "CFLAGS=-O2" \
    transform.h
then
    echo
    echo "transform.h generation failed. Relevant Makefile rules:"
    sed -n '140,165p' progs/Makefile || true
    fail "Could not generate progs/transform.h."
fi

[ -s "${SOURCE_DIR}/progs/transform.h" ] ||
    fail "transform.h target completed, but progs/transform.h is missing or empty."

echo "Verified generated header: ${SOURCE_DIR}/progs/transform.h"

# ============================================================
# Build terminal database utilities statically
# ============================================================

echo
echo "============================================================"
echo "Building terminal database utilities statically"
echo "============================================================"

if ! "$MAKE_EXECUTABLE" \
    -C progs \
    "CC=${GCC4_CC}" \
    "CFLAGS=-O2" \
    "LDFLAGS=${STATIC_LDFLAGS} ${LINK_TRACE_FLAGS}" \
    -j"$JOBS" \
    tic toe infocmp clear tput tset
then
    echo
    echo "Utility build failed. Generated link recipes:"
    grep -n -E '(^tic|^toe|^clear|^tput|^infocmp|^tset|DEPS_CURSES|\$\(LINK\))' \
        progs/Makefile || true

    echo
    echo "Generated transform.h:"
    if [ -f progs/transform.h ]; then
        ls -l progs/transform.h
    else
        echo "progs/transform.h is missing."
    fi

    echo
    echo "Available utility targets:"
    grep -n -E '^[^	 #][^:]*:' progs/Makefile || true

    fail "Building terminal database utilities failed."
fi

# ============================================================
# Build tack statically
# ============================================================

echo
echo "============================================================"
echo "Building tack statically"
echo "============================================================"

if ! "$MAKE_EXECUTABLE" \
    -C tack \
    "CC=${GCC4_CC}" \
    "CFLAGS=-O2" \
    "LDFLAGS=${STATIC_LDFLAGS} ${LINK_TRACE_FLAGS}" \
    -j"$JOBS" \
    tack
then
    echo
    echo "tack build failed. Generated link recipe:"
    grep -n -E '(^tack|DEPS_TACK|DEPS_CURSES|\$\(LINK\))' \
        tack/Makefile || true

    echo
    echo "Available tack targets:"
    grep -n -E '^[^	 #][^:]*:' tack/Makefile || true

    fail "Building tack failed."
fi

# ============================================================
# Verify static libraries
# ============================================================

echo
echo "============================================================"
echo "Verifying ncurses static libraries"
echo "============================================================"

for LIBRARY in \
    "${SOURCE_DIR}/lib/libncurses.a" \
    "${SOURCE_DIR}/lib/libform.a" \
    "${SOURCE_DIR}/lib/libmenu.a" \
    "${SOURCE_DIR}/lib/libpanel.a"
do
    [ -s "$LIBRARY" ] ||
        fail "Expected static library not found or empty: ${LIBRARY}"

    echo "Verified static library: ${LIBRARY}"
done

# ============================================================
# Verify shared ncurses library outputs
# ============================================================

echo
echo "============================================================"
echo "Verifying ncurses shared libraries"
echo "============================================================"

SHARED_FOUND=0

for LIBRARY in \
    "${SOURCE_DIR}/lib/libncurses.so" \
    "${SOURCE_DIR}/lib/libncurses.so.5" \
    "${SOURCE_DIR}/lib/libncurses.so.5.4"
do
    if [ -s "$LIBRARY" ]; then
        echo "Verified shared-library output: ${LIBRARY}"
        SHARED_FOUND=1
    elif [ -L "$LIBRARY" ]; then
        echo "Shared-library symlink: ${LIBRARY}"
        ls -l "$LIBRARY"

        if [ -e "$LIBRARY" ]; then
            SHARED_FOUND=1
        else
            echo "WARNING: dangling shared-library symlink: ${LIBRARY}"
        fi
    fi
done

[ "$SHARED_FOUND" -eq 1 ] ||
    fail "No usable ncurses shared-library output was found."

[ -f "${SOURCE_DIR}/lib/libncurses.so.5.4" ] ||
    fail "Expected versioned shared library libncurses.so.5.4 is missing or broken."

echo
echo "Shared-library directory listing:"
ls -l "${SOURCE_DIR}/lib/"*ncurses.so* ||
    fail "Could not list ncurses shared-library outputs."

# ============================================================
# Verify terminal database utilities and tack
# ============================================================

echo
echo "============================================================"
echo "Verifying built executables"
echo "============================================================"

for UTILITY in tic toe infocmp clear tput tset
do
    [ -x "${SOURCE_DIR}/progs/${UTILITY}" ] ||
        fail "Utility was not built: ${SOURCE_DIR}/progs/${UTILITY}"

    echo "Built: ${SOURCE_DIR}/progs/${UTILITY}"
done

[ -x "${SOURCE_DIR}/tack/tack" ] ||
    fail "tack executable was not built: ${SOURCE_DIR}/tack/tack"

echo "Built: ${SOURCE_DIR}/tack/tack"

# ============================================================
# Verify static linking of generated ELF executables
# ============================================================

echo
echo "============================================================"
echo "Verifying static linking of executables"
echo "============================================================"

CHECKED_ELF_COUNT=0

for OUTPUT in \
    "${SOURCE_DIR}/progs/tic" \
    "${SOURCE_DIR}/progs/toe" \
    "${SOURCE_DIR}/progs/infocmp" \
    "${SOURCE_DIR}/progs/clear" \
    "${SOURCE_DIR}/progs/tput" \
    "${SOURCE_DIR}/progs/tset" \
    "${SOURCE_DIR}/tack/tack"
do
    [ -f "$OUTPUT" ] ||
        fail "Expected executable is missing: ${OUTPUT}"

    [ -x "$OUTPUT" ] ||
        fail "Expected executable is not executable: ${OUTPUT}"

    FILE_DESCRIPTION=$(file "$OUTPUT") ||
        fail "Cannot inspect executable: ${OUTPUT}"

    echo "$FILE_DESCRIPTION"

    case "$FILE_DESCRIPTION" in
        *ELF*)
            ;;
        *)
            fail "Output is not identified as an ELF executable: ${OUTPUT}"
            ;;
    esac

    case "$FILE_DESCRIPTION" in
        *"statically linked"*)
            CHECKED_ELF_COUNT=$((CHECKED_ELF_COUNT + 1))
            ;;
        *)
            fail "Executable is not statically linked: ${OUTPUT}"
            ;;
    esac

    # A dynamically linked ELF executable normally contains a
    # PT_INTERP program header. Reject one if readelf is available.
    if command -v readelf >/dev/null 2>&1; then
        if readelf -l "$OUTPUT" 2>/dev/null |
            grep -q 'INTERP'
        then
            fail "Executable contains a dynamic interpreter: ${OUTPUT}"
        fi
    fi
done

[ "$CHECKED_ELF_COUNT" -eq 7 ] ||
    fail "Expected seven statically linked ELF executables; checked ${CHECKED_ELF_COUNT}."

echo
echo "Verified statically linked ELF executables: ${CHECKED_ELF_COUNT}"

# ============================================================
# Success
# ============================================================

echo
echo "============================================================"
echo "ncurses 5.4 build summary"
echo "============================================================"

echo "Compiler: ${GCC4_CC} (${GCC_VERSION_FOUND})"
echo "Static library: ${SOURCE_DIR}/lib/libncurses.a"
echo "Form library: ${SOURCE_DIR}/lib/libform.a"
echo "Menu library: ${SOURCE_DIR}/lib/libmenu.a"
echo "Panel library: ${SOURCE_DIR}/lib/libpanel.a"
echo "Shared library outputs: ${SOURCE_DIR}/lib/"
echo "Generated header: ${SOURCE_DIR}/progs/transform.h"
echo "Terminfo compiler: ${SOURCE_DIR}/progs/tic"
echo "tack: ${SOURCE_DIR}/tack/tack"
echo "Library policy: static and shared"
echo "Executable policy: fully static"
echo "Verified static ELF executables: ${CHECKED_ELF_COUNT}"
echo "No installation or staging performed."
echo "Log: ${LOG_FILE}"

echo
echo "SUCCESS: ncurses 5.4 static/shared build completed."

exit 0
