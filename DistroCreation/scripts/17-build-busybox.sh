
#!/bin/sh

# ============================================================
# LVGD486 - Build BusyBox 1.00
# Script: 17-build-busybox.sh
#
# Build a size-optimized, statically linked BusyBox.
#
# Build behavior:
#   - Clean previous build artifacts before compiling.
#   - Preserve the original configuration file.
#   - Preserve and verify existing compatibility patches.
#   - Enable -Os through CONFIG_EXTRA_CFLAGS when available.
#   - Preserve an unstripped copy of the successful build.
#   - Strip the final executable.
#   - Verify version, execution, and static linkage.
#
# Does not install BusyBox into the distro.
# ============================================================

set -e

# ------------------------------------------------------------
# Constants
# ------------------------------------------------------------

SOURCE_DIR="/home/user/sources/busybox"
CONFIG_FILE="/home/user/doc/busybox-1.0-config/distrodev/.config"

MAKE_EXECUTABLE="/usr/local/bin/make"
STRIP_EXECUTABLE="/usr/bin/strip"

BUSYBOX_VERSION="1.00"
UNSTRIPPED_BACKUP="/home/user/sources/busybox-1.00.unstripped"

JOBS=1

# ------------------------------------------------------------
# Logging and error handling
# ------------------------------------------------------------

log()
{
    echo "17-build-busybox: $*"
}

fail()
{
    log "ERROR: $*"
    exit 1
}

# ------------------------------------------------------------
# Verify prerequisites
# ------------------------------------------------------------

if [ "$(id -u)" -ne 0 ]; then
    fail "Run this script as root."
fi

if [ ! -f "${SOURCE_DIR}/Makefile" ]; then
    fail "BusyBox source not found at ${SOURCE_DIR}"
fi

if [ ! -f "$CONFIG_FILE" ]; then
    fail "BusyBox configuration not found at ${CONFIG_FILE}"
fi

if [ ! -x "$MAKE_EXECUTABLE" ]; then
    fail "GNU Make not found at ${MAKE_EXECUTABLE}"
fi

if [ ! -x "$STRIP_EXECUTABLE" ]; then
    fail "strip not found at ${STRIP_EXECUTABLE}"
fi

if ! command -v file >/dev/null 2>&1; then
    fail "The file command is required to verify the executable."
fi

# ------------------------------------------------------------
# Verify source version
# ------------------------------------------------------------

cd "$SOURCE_DIR"

if ! grep -Eq '^[[:space:]]*VERSION[[:space:]]*=[[:space:]]*1\.00' Makefile; then
    log "WARNING: Could not confirm BusyBox ${BUSYBOX_VERSION} from Makefile."
    log "Continuing with the supplied source tree."
fi

# ------------------------------------------------------------
# Clean previous build
#
# Use make clean rather than distclean so the source tree's
# configuration and compatibility patches are retained.
# The known-good configuration is copied back after cleaning.
# ------------------------------------------------------------

log "Cleaning previous BusyBox build artifacts..."

"$MAKE_EXECUTABLE" clean

if [ -x "${SOURCE_DIR}/busybox" ]; then
    rm -f "${SOURCE_DIR}/busybox"
fi

log "Previous build cleanup complete."

# ------------------------------------------------------------
# Install configuration
# ------------------------------------------------------------

log "Installing BusyBox configuration..."

cp "$CONFIG_FILE" "${SOURCE_DIR}/.config"

# ------------------------------------------------------------
# Enable size optimization when supported by this configuration
#
# Use the BusyBox configuration option rather than overriding
# CFLAGS on the make command line, which could interfere with
# compiler flags supplied by this older build system.
# ------------------------------------------------------------

if grep -q '^CONFIG_EXTRA_CFLAGS=' "${SOURCE_DIR}/.config"; then
    sed -i \
        's/^CONFIG_EXTRA_CFLAGS=.*/CONFIG_EXTRA_CFLAGS="-Os"/' \
        "${SOURCE_DIR}/.config"

    if ! grep -Fq 'CONFIG_EXTRA_CFLAGS="-Os"' "${SOURCE_DIR}/.config"; then
        fail "Could not enable -Os in CONFIG_EXTRA_CFLAGS."
    fi

    log "Enabled -Os through CONFIG_EXTRA_CFLAGS."
else
    log "WARNING: CONFIG_EXTRA_CFLAGS is not present."
    log "Using the optimization settings in the supplied configuration."
fi

# ------------------------------------------------------------
# Verify static-link configuration
# ------------------------------------------------------------

if ! grep -q '^CONFIG_STATIC=y$' "${SOURCE_DIR}/.config"; then
    fail "CONFIG_STATIC=y is required for a statically linked build."
fi

log "Static linking is enabled in .config."

# ------------------------------------------------------------
# Patch gzip.c build_bl_tree declaration for compiler
# compatibility
# ------------------------------------------------------------

GZIP_SOURCE="${SOURCE_DIR}/archival/gzip.c"

if [ ! -f "$GZIP_SOURCE" ]; then
    fail "gzip.c not found at ${GZIP_SOURCE}"
fi

log "Applying gzip.c compatibility patch..."

if grep -q '^static const int build_bl_tree()' "$GZIP_SOURCE"; then
    sed -i \
        's/^static const int build_bl_tree()/static int build_bl_tree(void)/' \
        "$GZIP_SOURCE"
fi

if grep -q '^static const int build_bl_tree()' "$GZIP_SOURCE"; then
    fail "gzip.c compatibility patch failed."
fi

if ! grep -q '^static int build_bl_tree(void)' "$GZIP_SOURCE"; then
    fail "Expected build_bl_tree declaration not found; inspect ${GZIP_SOURCE}."
fi

log "gzip.c compatibility patch verified."

# ------------------------------------------------------------
# Verify BusyBox 1.00 NFS mount-data argument handling
#
# mount_one() must pass:
#   Argument 5: string_flags - binary NFS mount data
#   Argument 8: extra_opts   - textual mtab options
#
# Restore the correct argument order if a previous patch
# changed it.
# ------------------------------------------------------------

MOUNT_SOURCE="${SOURCE_DIR}/util-linux/mount.c"

if [ ! -f "$MOUNT_SOURCE" ]; then
    fail "mount.c not found at ${MOUNT_SOURCE}"
fi

log "Verifying NFS mount-data argument handling..."

sed -i \
    's/(device, directory, filesystemType, flags, extra_opts,/(device, directory, filesystemType, flags, string_flags,/' \
    "$MOUNT_SOURCE"

sed -i \
    's/useMtab, fakeIt, string_flags, TRUE, all)) {/useMtab, fakeIt, extra_opts, TRUE, all)) {/' \
    "$MOUNT_SOURCE"

if ! grep -Fq \
    '(device, directory, filesystemType, flags, string_flags,' \
    "$MOUNT_SOURCE"; then
    fail "NFS mount-data verification failed: expected string_flags as argument 5."
fi

if ! grep -Fq \
    'useMtab, fakeIt, extra_opts, TRUE, all)) {' \
    "$MOUNT_SOURCE"; then
    fail "NFS mtab verification failed: expected extra_opts as argument 8."
fi

log "NFS mount-data arguments verified."

# ------------------------------------------------------------
# Build BusyBox
# ------------------------------------------------------------

log "Building BusyBox ${BUSYBOX_VERSION}..."

"$MAKE_EXECUTABLE" -j"$JOBS"

# ------------------------------------------------------------
# Verify initial build output
# ------------------------------------------------------------

if [ ! -f "${SOURCE_DIR}/busybox" ] || \
   [ ! -x "${SOURCE_DIR}/busybox" ]; then
    fail "Build finished without producing an executable BusyBox."
fi

log "Checking the unstripped executable..."

if ! "${SOURCE_DIR}/busybox" --help >/dev/null 2>&1; then
    fail "Unstripped BusyBox failed its execution test."
fi

if ! file "${SOURCE_DIR}/busybox" | grep -qi 'statically linked'; then
    log "Executable information:"
    file "${SOURCE_DIR}/busybox"
    fail "The built executable does not appear to be statically linked."
fi

# ------------------------------------------------------------
# Preserve unstripped executable
#
# Keep the backup outside the source directory so it cannot
# be confused with the BusyBox build target.
# ------------------------------------------------------------

log "Saving unstripped executable..."

cp "${SOURCE_DIR}/busybox" "${UNSTRIPPED_BACKUP}.new"
chmod 755 "${UNSTRIPPED_BACKUP}.new"
mv -f "${UNSTRIPPED_BACKUP}.new" "$UNSTRIPPED_BACKUP"

log "Unstripped backup: ${UNSTRIPPED_BACKUP}"

# ------------------------------------------------------------
# Record size before stripping
# ------------------------------------------------------------

log "Executable size before stripping:"

if command -v size >/dev/null 2>&1; then
    size "${SOURCE_DIR}/busybox"
fi

ls -l "${SOURCE_DIR}/busybox"

# ------------------------------------------------------------
# Strip executable
# ------------------------------------------------------------

log "Stripping BusyBox executable..."

"$STRIP_EXECUTABLE" --strip-all "${SOURCE_DIR}/busybox"

if [ ! -x "${SOURCE_DIR}/busybox" ]; then
    fail "BusyBox is missing or not executable after stripping."
fi

# ------------------------------------------------------------
# Verify final executable
# ------------------------------------------------------------

log "Verifying stripped BusyBox..."

if ! "${SOURCE_DIR}/busybox" --help >/dev/null 2>&1; then
    fail "Stripped BusyBox failed its execution test."
fi

if ! file "${SOURCE_DIR}/busybox" | grep -qi 'statically linked'; then
    log "Executable information:"
    file "${SOURCE_DIR}/busybox"
    fail "Final executable does not appear to be statically linked."
fi

# Verify that the executable reports its expected version.
VERSION_OUTPUT=$("${SOURCE_DIR}/busybox" 2>&1 || true)

if ! echo "$VERSION_OUTPUT" | grep -Fq "$BUSYBOX_VERSION"; then
    log "WARNING: Could not confirm version ${BUSYBOX_VERSION} from normal output."
    log "Version output:"
    echo "$VERSION_OUTPUT"
fi

# ------------------------------------------------------------
# Final report
# ------------------------------------------------------------

log "Executable size after stripping:"

if command -v size >/dev/null 2>&1; then
    size "${SOURCE_DIR}/busybox"
fi

ls -l "${SOURCE_DIR}/busybox"

log "Final executable information:"
file "${SOURCE_DIR}/busybox"

if command -v ldd >/dev/null 2>&1; then
    LDD_OUTPUT=$(ldd "${SOURCE_DIR}/busybox" 2>&1 || true)

    if echo "$LDD_OUTPUT" | grep -qi 'not a dynamic executable'; then
        log "Static linkage verified by ldd."
    elif echo "$LDD_OUTPUT" | grep -qi 'statically linked'; then
        log "Static linkage verified by ldd."
    else
        log "WARNING: ldd returned unexpected output:"
        echo "$LDD_OUTPUT"
    fi
fi

log "BusyBox ${BUSYBOX_VERSION} built successfully."
log "Executable: ${SOURCE_DIR}/busybox"
log "Unstripped backup: ${UNSTRIPPED_BACKUP}"
log "No installation was performed."
