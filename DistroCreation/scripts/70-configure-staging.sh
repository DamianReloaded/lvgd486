#!/bin/sh

# ============================================================
# LVGD486 - Configure Development Staging System
#
# Configure the development staging tree:
#   - Base /etc configuration files (without /etc/shadow)
#   - Development scripts
#   - rcS startup script
#   - Users and groups
#   - Default MD5-crypt password hashes in /etc/passwd
#   - Home directories and permissions
#   - Account database permissions
#   - BusyBox SUID configuration
#   - System directory ownership
# ============================================================

# ------------------------------------------------------------
# Source directories
# ------------------------------------------------------------

DOC_DIR="/home/user/doc"
ETC_SOURCE_DIR="${DOC_DIR}/etc"
SCRIPT_SOURCE_DIR="${DOC_DIR}/sbin"
RCS_SOURCE="${ETC_SOURCE_DIR}/init.d/rcS"

BUSYBOX_SRC_DIR="/home/user/sources/busybox"

# ------------------------------------------------------------
# Staging directories
# ------------------------------------------------------------

DISTRODEV_ROOT="/home/user/distrodev"
ETC_DIR="${DISTRODEV_ROOT}/etc"
INIT_DIR="${ETC_DIR}/init.d"
SCRIPT_DEST_DIR="${DISTRODEV_ROOT}/sbin"
BIN_DIR="${DISTRODEV_ROOT}/bin"

HOME_DIR="${DISTRODEV_ROOT}/home"
ROOT_HOME="${DISTRODEV_ROOT}/root"
RCS_DEST="${INIT_DIR}/rcS"

# ------------------------------------------------------------
# Account configuration
# ------------------------------------------------------------

ROOT_NAME="root"
ROOT_UID=0
ROOT_GID=0
ROOT_GECOS="Root"
ROOT_PASSWORD="root"
ROOT_HOME="${DISTRODEV_ROOT}/root"
ROOT_SHELL="/bin/bash"

USER_NAME="game"
USER_UID=1000
USER_GID=1000
USER_GECOS="Game"
USER_PASSWORD="game"
USER_HOME="${HOME_DIR}/${USER_NAME}"
USER_SHELL="/bin/bash"

HARDWARE_GROUP="hardware"
HARDWARE_GID=1001

WHEEL_GROUP="wheel"
WHEEL_GID=1002

PASSWD_FILE="${ETC_DIR}/passwd"
GROUP_FILE="${ETC_DIR}/group"
BUSYBOX_CONF="${ETC_DIR}/busybox.conf"

BASE_ETC_FILES="group inittab passwd profile securetty shells"

FILE_COUNT=0

# Temporary files used by this script.
PASSWD_TMP="${PASSWD_FILE}.tmp"
GROUP_TMP="${GROUP_FILE}.tmp"

# ------------------------------------------------------------
# Logging and command execution
# ------------------------------------------------------------

log()
{
    echo "70-configure-staging: $*"
}

fail()
{
    log "ERROR: $*" >&2
    exit 1
}

run()
{
    log "RUN: $*"

    "$@"
    STATUS=$?

    [ "$STATUS" -eq 0 ] ||
        fail "Command failed (exit status $STATUS): $*"
}

cleanup()
{
    rm -f "$PASSWD_TMP" "$GROUP_TMP"
}

trap cleanup 0
trap 'exit 1' 1 2 3 15

# ------------------------------------------------------------
# Check prerequisites
# ------------------------------------------------------------

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Development staging root not found: $DISTRODEV_ROOT"

[ -d "$ETC_SOURCE_DIR" ] ||
    fail "Configuration source directory not found: $ETC_SOURCE_DIR"

[ -d "$SCRIPT_SOURCE_DIR" ] ||
    fail "Script source directory not found: $SCRIPT_SOURCE_DIR"

[ -d "$ETC_DIR" ] ||
    fail "Staging /etc directory not found: $ETC_DIR"

[ -d "$BIN_DIR" ] ||
    fail "Staging /bin directory not found: $BIN_DIR"

[ -f "$RCS_SOURCE" ] ||
    fail "rcS source file not found: $RCS_SOURCE"

command -v awk >/dev/null 2>&1 ||
    fail "awk is required."

command -v python >/dev/null 2>&1 ||
    fail "Python is required to generate MD5-crypt password hashes."

if [ -f "${BUSYBOX_SRC_DIR}/.config" ]; then
    :
else
    log "WARNING: BusyBox .config not found; SUID support cannot be confirmed."
fi

for FILE in $BASE_ETC_FILES
do
    [ -f "${ETC_SOURCE_DIR}/${FILE}" ] ||
        fail "Source configuration file not found: ${ETC_SOURCE_DIR}/${FILE}"
done

# ------------------------------------------------------------
# Copy base system configuration files
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Copying Base System Configuration Files"
echo "============================================================"

for FILE in $BASE_ETC_FILES
do
    run cp "${ETC_SOURCE_DIR}/${FILE}" "${ETC_DIR}/${FILE}"
done

log "Copied base configuration files to $ETC_DIR."

# This staging configuration stores password hashes in passwd.
# Do not copy or retain a staged shadow file.
if [ -e "${ETC_DIR}/shadow" ]; then
    run rm -f "${ETC_DIR}/shadow"
    log "Removed staged /etc/shadow."
fi

# ------------------------------------------------------------
# Copy development scripts
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Copying Development System Scripts"
echo "============================================================"

run mkdir -p "$SCRIPT_DEST_DIR"

FILE_COUNT=0

for FILE in "${SCRIPT_SOURCE_DIR}"/*
do
    [ -f "$FILE" ] || continue

    FILENAME="${FILE##*/}"

    run cp "$FILE" "${SCRIPT_DEST_DIR}/${FILENAME}"
    run chmod 0755 "${SCRIPT_DEST_DIR}/${FILENAME}"

    FILE_COUNT=$((FILE_COUNT + 1))
done

[ "$FILE_COUNT" -gt 0 ] ||
    fail "No regular files found in $SCRIPT_SOURCE_DIR"

log "Copied $FILE_COUNT scripts to $SCRIPT_DEST_DIR."

# ------------------------------------------------------------
# Install rcS startup script
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Installing rcS Startup Script"
echo "============================================================"

run mkdir -p "$INIT_DIR"
run cp "$RCS_SOURCE" "$RCS_DEST"
run chmod 0755 "$RCS_DEST"

log "Installed rcS to $RCS_DEST."

# ------------------------------------------------------------
# Check account database files
# ------------------------------------------------------------

[ -f "$PASSWD_FILE" ] ||
    fail "Passwd file not found: $PASSWD_FILE"

[ -f "$GROUP_FILE" ] ||
    fail "Group file not found: $GROUP_FILE"

# ------------------------------------------------------------
# Configure groups
# ------------------------------------------------------------

ensure_group()
{
    GROUP_NAME="$1"
    GROUP_ID="$2"
    GROUP_MEMBERS="$3"

    GROUP_NAME_COUNT=$(awk -F: -v name="$GROUP_NAME" '
        $1 == name { count++ }
        END { print count + 0 }
    ' "$GROUP_FILE") ||
        fail "Could not inspect group file."

    [ "$GROUP_NAME_COUNT" -le 1 ] ||
        fail "Group ${GROUP_NAME} appears more than once."

    if awk -F: -v gid="$GROUP_ID" -v name="$GROUP_NAME" '
        $3 == gid && $1 != name { found=1 }
        END { exit !found }
    ' "$GROUP_FILE"
    then
        fail "GID ${GROUP_ID} is already assigned to another group."
    fi

    if [ "$GROUP_NAME_COUNT" -eq 1 ]; then
        umask 077

        awk -F: -v OFS=: \
            -v name="$GROUP_NAME" \
            -v gid="$GROUP_ID" \
            -v members="$GROUP_MEMBERS" '
            $1 == name {
                $2 = "x"
                $3 = gid
                $4 = members
            }
            { print }
        ' "$GROUP_FILE" > "$GROUP_TMP" ||
            fail "Could not update group ${GROUP_NAME}."

        run chown 0:0 "$GROUP_TMP"
        run chmod 0644 "$GROUP_TMP"
        run mv "$GROUP_TMP" "$GROUP_FILE"

        log "Updated group ${GROUP_NAME}."
        return 0
    fi

    printf '%s:x:%s:%s\n' \
        "$GROUP_NAME" "$GROUP_ID" "$GROUP_MEMBERS" >> "$GROUP_FILE" ||
        fail "Could not create group ${GROUP_NAME}."

    log "Created group ${GROUP_NAME}."
}

ensure_group "$ROOT_NAME" "$ROOT_GID" ""
ensure_group "$USER_NAME" "$USER_GID" ""
ensure_group "$HARDWARE_GROUP" "$HARDWARE_GID" "${ROOT_NAME},${USER_NAME}"
ensure_group "$WHEEL_GROUP" "$WHEEL_GID" "$USER_NAME"

# ------------------------------------------------------------
# Configure users
# ------------------------------------------------------------

ensure_user()
{
    ACCOUNT_NAME="$1"
    ACCOUNT_UID="$2"
    ACCOUNT_GID="$3"
    ACCOUNT_GECOS="$4"
    ACCOUNT_HOME="$5"
    ACCOUNT_SHELL="$6"

    ACCOUNT_NAME_COUNT=$(awk -F: -v name="$ACCOUNT_NAME" '
        $1 == name { count++ }
        END { print count + 0 }
    ' "$PASSWD_FILE") ||
        fail "Could not inspect passwd file."

    [ "$ACCOUNT_NAME_COUNT" -le 1 ] ||
        fail "User ${ACCOUNT_NAME} appears more than once."

    if awk -F: -v uid="$ACCOUNT_UID" -v name="$ACCOUNT_NAME" '
        $3 == uid && $1 != name { found=1 }
        END { exit !found }
    ' "$PASSWD_FILE"
    then
        fail "UID ${ACCOUNT_UID} is already assigned to another user."
    fi

    if [ "$ACCOUNT_NAME_COUNT" -eq 1 ]; then
        log "Updating existing user ${ACCOUNT_NAME}."

        umask 077

        awk -F: -v OFS=: \
            -v name="$ACCOUNT_NAME" \
            -v uid="$ACCOUNT_UID" \
            -v gid="$ACCOUNT_GID" \
            -v gecos="$ACCOUNT_GECOS" \
            -v home="$ACCOUNT_HOME" \
            -v shell="$ACCOUNT_SHELL" '
            $1 == name {
                $3 = uid
                $4 = gid
                $5 = gecos
                $6 = home
                $7 = shell
            }
            { print }
        ' "$PASSWD_FILE" > "$PASSWD_TMP" ||
            fail "Could not update passwd entry for ${ACCOUNT_NAME}."

        run chown 0:0 "$PASSWD_TMP"
        run chmod 0644 "$PASSWD_TMP"
        run mv "$PASSWD_TMP" "$PASSWD_FILE"

        return 0
    fi

    printf '%s:x:%s:%s:%s:%s:%s\n' \
        "$ACCOUNT_NAME" \
        "$ACCOUNT_UID" \
        "$ACCOUNT_GID" \
        "$ACCOUNT_GECOS" \
        "$ACCOUNT_HOME" \
        "$ACCOUNT_SHELL" >> "$PASSWD_FILE" ||
        fail "Could not create user ${ACCOUNT_NAME}."

    log "Created user ${ACCOUNT_NAME}."
}

ensure_user "$ROOT_NAME" "$ROOT_UID" "$ROOT_GID" \
    "$ROOT_GECOS" "$ROOT_HOME" "$ROOT_SHELL"

ensure_user "$USER_NAME" "$USER_UID" "$USER_GID" \
    "$USER_GECOS" "$USER_HOME" "$USER_SHELL"

# ------------------------------------------------------------
# Configure default development passwords
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Configuring Default Passwords"
echo "============================================================"

# Default passwords are configured by the constants above.
#
# Hash format:
#   MD5-crypt ($1$...)
#
# Hashes are stored in /etc/passwd. This arrangement must be
# compatible with the actual BusyBox 1.00 authentication applets.

ROOT_HASH=$(python -c '
import crypt
import sys
print crypt.crypt(sys.argv[1], "$1$ab$")
' "$ROOT_PASSWORD" 2>/dev/null) ||
    fail "Could not generate root MD5-crypt password hash."

USER_HASH=$(python -c '
import crypt
import sys
print crypt.crypt(sys.argv[1], "$1$cd$")
' "$USER_PASSWORD" 2>/dev/null) ||
    fail "Could not generate MD5-crypt password hash for ${USER_NAME}."

case "$ROOT_HASH" in
    '$1$'*) ;;
    *) fail "Invalid MD5-crypt password hash generated for ${ROOT_NAME}." ;;
esac

case "$USER_HASH" in
    '$1$'*) ;;
    *) fail "Invalid MD5-crypt password hash generated for ${USER_NAME}." ;;
esac

ROOT_CHECK=$(python -c '
import crypt
import sys
h = sys.argv[1]
password = sys.argv[2]
print crypt.crypt(password, h)
' "$ROOT_HASH" "$ROOT_PASSWORD" 2>/dev/null) ||
    fail "Could not verify root password hash."

USER_CHECK=$(python -c '
import crypt
import sys
h = sys.argv[1]
password = sys.argv[2]
print crypt.crypt(password, h)
' "$USER_HASH" "$USER_PASSWORD" 2>/dev/null) ||
    fail "Could not verify password hash for ${USER_NAME}."

[ "$ROOT_CHECK" = "$ROOT_HASH" ] ||
    fail "Root password hash verification failed."

[ "$USER_CHECK" = "$USER_HASH" ] ||
    fail "Password hash verification failed for ${USER_NAME}."

umask 077

awk -F: -v OFS=: \
    -v root_name="$ROOT_NAME" \
    -v root_hash="$ROOT_HASH" \
    -v user_name="$USER_NAME" \
    -v user_hash="$USER_HASH" '
    $1 == root_name {
        $2 = root_hash
        root_seen++
    }
    $1 == user_name {
        $2 = user_hash
        user_seen++
    }
    { print }
    END {
        if (root_seen != 1 || user_seen != 1)
            exit 1
    }
' "$PASSWD_FILE" > "$PASSWD_TMP" ||
    fail "Could not update password fields or duplicate/missing accounts detected."

run chown 0:0 "$PASSWD_TMP"
run chmod 0644 "$PASSWD_TMP"
run mv "$PASSWD_TMP" "$PASSWD_FILE"

log "Configured password for ${ROOT_NAME} (MD5-crypt)."
log "Configured password for ${USER_NAME} (MD5-crypt)."

# ------------------------------------------------------------
# Normalize staged paths in /etc/passwd
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Normalizing Passwd Home Directories"
echo "============================================================"

STAGING_PREFIX="/home/user/distrodev"

if grep -Fq "$STAGING_PREFIX" "$PASSWD_FILE"; then
    umask 077

	sed 's|/home/user/distrodev/|/|g' "$PASSWD_FILE" > "$PASSWD_TMP" ||
		fail "Could not normalize staging paths in passwd."

    run chown 0:0 "$PASSWD_TMP"
    run chmod 0644 "$PASSWD_TMP"
    run mv "$PASSWD_TMP" "$PASSWD_FILE"

    log "Replaced ${STAGING_PREFIX} with / in passwd."
else
    log "No staging paths found in passwd."
fi

# ------------------------------------------------------------
# Configure home directories
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Configuring Home Directories"
echo "============================================================"

run mkdir -p "$ROOT_HOME" "$USER_HOME"

run chown "$ROOT_UID:$ROOT_GID" "$ROOT_HOME"
run chmod 0700 "$ROOT_HOME"

run chown "$USER_UID:$USER_GID" "$USER_HOME"
run chmod 0700 "$USER_HOME"

# ------------------------------------------------------------
# Configure account database permissions
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Configuring Account Database Permissions"
echo "============================================================"

run chown 0:0 "$PASSWD_FILE" "$GROUP_FILE"
run chmod 0644 "$PASSWD_FILE" "$GROUP_FILE"

# ------------------------------------------------------------
# Configure BusyBox SUID
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Configuring BusyBox SUID"
echo "============================================================"

BUSYBOX_BIN="${BIN_DIR}/busybox"

[ -f "$BUSYBOX_BIN" ] ||
    fail "BusyBox binary not found: $BUSYBOX_BIN"

cat > "$BUSYBOX_CONF" <<'EOF'
[SUID]
su = ssx root.0
EOF

[ -s "$BUSYBOX_CONF" ] ||
    fail "Could not write BusyBox configuration."

run chown 0:0 "$BUSYBOX_CONF"
run chmod 0600 "$BUSYBOX_CONF"

if [ -f "${BUSYBOX_SRC_DIR}/.config" ] &&
    grep -q '^CONFIG_FEATURE_SUID=y' "${BUSYBOX_SRC_DIR}/.config"
then
    run chown 0:0 "$BUSYBOX_BIN"
    run chmod 4755 "$BUSYBOX_BIN"
    log "Enabled SUID on BusyBox."
else
    run chown 0:0 "$BUSYBOX_BIN"
    run chmod 0755 "$BUSYBOX_BIN"

    log "WARNING: BusyBox SUID support was not confirmed."
    log "BusyBox was left without the setuid bit."
fi

# ------------------------------------------------------------
# Configure system directory ownership
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Configuring System Directory Ownership"
echo "============================================================"

for DIR in bin sbin etc lib usr
do
    if [ -d "${DISTRODEV_ROOT}/${DIR}" ]; then
        run chown -R 0:0 "${DISTRODEV_ROOT}/${DIR}"
    fi
done

# Reapply critical permissions after ownership changes.

run chown 0:0 "$PASSWD_FILE" "$GROUP_FILE"
run chmod 0644 "$PASSWD_FILE" "$GROUP_FILE"

run chown 0:0 "$BUSYBOX_CONF"
run chmod 0600 "$BUSYBOX_CONF"

if [ -f "${BUSYBOX_SRC_DIR}/.config" ] &&
    grep -q '^CONFIG_FEATURE_SUID=y' "${BUSYBOX_SRC_DIR}/.config"
then
    run chown 0:0 "$BUSYBOX_BIN"
    run chmod 4755 "$BUSYBOX_BIN"
else
    run chown 0:0 "$BUSYBOX_BIN"
    run chmod 0755 "$BUSYBOX_BIN"
fi

# ------------------------------------------------------------
# Validate final account entries
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Validating Account Database"
echo "============================================================"

awk -F: \
    -v root_name="$ROOT_NAME" \
    -v root_uid="$ROOT_UID" \
    -v root_gid="$ROOT_GID" \
    -v user_name="$USER_NAME" \
    -v user_uid="$USER_UID" \
    -v user_gid="$USER_GID" '
    $1 == root_name {
        if (NF != 7 || $2 !~ /^\$1\$/ ||
            $3 != root_uid || $4 != root_gid)
            bad=1
        root_seen++
    }

    $1 == user_name {
        if (NF != 7 || $2 !~ /^\$1\$/ ||
            $3 != user_uid || $4 != user_gid)
            bad=1
        user_seen++
    }

    END {
        if (root_seen != 1 || user_seen != 1 || bad)
            exit 1
    }
' "$PASSWD_FILE" ||
    fail "Final passwd entries failed validation."

awk -F: \
    -v root_name="$ROOT_NAME" \
    -v root_gid="$ROOT_GID" \
    -v user_name="$USER_NAME" \
    -v user_gid="$USER_GID" \
    -v hardware_group="$HARDWARE_GROUP" \
    -v hardware_gid="$HARDWARE_GID" \
    -v wheel_group="$WHEEL_GROUP" \
    -v wheel_gid="$WHEEL_GID" '
    $1 == root_name {
        if ($3 != root_gid) bad=1
        root_seen++
    }

    $1 == user_name {
        if ($3 != user_gid) bad=1
        user_seen++
    }

    $1 == hardware_group {
        if ($3 != hardware_gid) bad=1
        hardware_seen++
    }

    $1 == wheel_group {
        if ($3 != wheel_gid) bad=1
        wheel_seen++
    }

    END {
        if (root_seen != 1 || user_seen != 1 ||
            hardware_seen != 1 || wheel_seen != 1 || bad)
            exit 1
    }
' "$GROUP_FILE" ||
    fail "Final group entries failed validation."

[ ! -e "${ETC_DIR}/shadow" ] ||
    fail "Unexpected shadow file exists in staging /etc."

[ "$(stat -c '%a' "$PASSWD_FILE" 2>/dev/null)" = "644" ] ||
    fail "Unexpected permissions on passwd file."

[ "$(stat -c '%a' "$GROUP_FILE" 2>/dev/null)" = "644" ] ||
    fail "Unexpected permissions on group file."

log "Account database validation passed."

# ------------------------------------------------------------
# Report success
# ------------------------------------------------------------

echo
echo "============================================================"
echo "LVGD486 Staging Configuration Complete"
echo "============================================================"
echo "Base configuration: $ETC_SOURCE_DIR"
echo "Scripts copied:     $FILE_COUNT"
echo "Scripts:            $SCRIPT_DEST_DIR"
echo "Startup script:     $RCS_DEST"
echo "Root account:       ${ROOT_NAME} (UID ${ROOT_UID})"
echo "User account:       ${USER_NAME} (UID ${USER_UID})"
echo "Groups:             ${ROOT_NAME}, ${USER_NAME}, ${HARDWARE_GROUP}, ${WHEEL_GROUP}"
echo "Root home:          $ROOT_HOME"
echo "User home:          $USER_HOME"
echo "Passwd file:        $PASSWD_FILE"
echo "Shadow file:        not used"
echo
echo "Development passwords configured:"
echo "  ${ROOT_NAME}: ${ROOT_PASSWORD}"
echo "  ${USER_NAME}: ${USER_PASSWORD}"
echo
echo "WARNING: Default passwords are intended for local use."
echo "NOTE: Additional hardware device permissions must be configured separately."

exit 0