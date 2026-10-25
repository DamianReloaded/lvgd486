
#!/bin/sh

# ============================================================
# LVGD486 - Copy Development Scripts and other stuff
# Script: 41-copy-additional-doc-stuff.sh
# ============================================================

SOURCE_DIR="/home/user/doc/sbin"

DISTRODEV_ROOT="/home/user/distrodev"
DEST_DIR="${DISTRODEV_ROOT}/sbin"
SHARE_DIR="${DISTRODEV_ROOT}/usr/share"

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

run()
{
    echo "RUN: $*"
    "$@"
    STATUS=$?

    [ "$STATUS" -eq 0 ] ||
        fail "Command failed (exit status $STATUS): $*"
}

# ------------------------------------------------------------
# Check prerequisites
# ------------------------------------------------------------

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -d "$SOURCE_DIR" ] ||
    fail "Source directory not found: $SOURCE_DIR"

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Development staging root not found: $DISTRODEV_ROOT"

# ------------------------------------------------------------
# Copy development scripts
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Copying Development System Scripts"
echo "============================================================"

run mkdir -p "$DEST_DIR"

FILE_COUNT=0

for FILE in "$SOURCE_DIR"/*
do
    [ -f "$FILE" ] || continue

    FILENAME="${FILE##*/}"

    run cp "$FILE" "$DEST_DIR/$FILENAME"
    run chmod 0755 "$DEST_DIR/$FILENAME"

    FILE_COUNT=$((FILE_COUNT + 1))
done

[ "$FILE_COUNT" -gt 0 ] ||
    fail "No regular files found in $SOURCE_DIR"

echo
echo "SUCCESS: Copied $FILE_COUNT scripts to $DEST_DIR."
echo "SUCCESS: All copied scripts are executable."

# ------------------------------------------------------------
# Report success
# ------------------------------------------------------------

echo
echo "============================================================"
echo "Development Documentation Files Installed"
echo "============================================================"
echo "Scripts:       $DEST_DIR"
echo "Scripts copied: $FILE_COUNT"

exit 0
