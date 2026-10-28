#!/bin/sh

# ============================================================
# LVGD486 - Set Staging Ownership
# Script: 72-set-staging-ownership.sh
#
# Set the staging tree to root:root, then assign the games
# and home trees to the game account's staged UID and GID.
# ============================================================

DISTRODEV_ROOT="/home/user/distrodev"
PASSWD_FILE="${DISTRODEV_ROOT}/etc/passwd"
GROUP_FILE="${DISTRODEV_ROOT}/etc/group"

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

# ------------------------------------------------------------
# Validate staging tree
# ------------------------------------------------------------

if [ ! -d "$DISTRODEV_ROOT" ]; then
    fail "Staging directory not found: $DISTRODEV_ROOT"
fi

if [ ! -f "$PASSWD_FILE" ]; then
    fail "Passwd file not found: $PASSWD_FILE"
fi

if [ ! -f "$GROUP_FILE" ]; then
    fail "Group file not found: $GROUP_FILE"
fi

if [ ! -d "${DISTRODEV_ROOT}/games" ]; then
    fail "Games directory not found."
fi

if [ ! -d "${DISTRODEV_ROOT}/home" ]; then
    fail "Home directory not found."
fi

# ------------------------------------------------------------
# Read game UID and GID from the staged account databases
# ------------------------------------------------------------

GAME_UID=`awk -F: '$1 == "game" { print $3; exit }' "$PASSWD_FILE"`
GAME_GID=`awk -F: '$1 == "game" { print $4; exit }' "$PASSWD_FILE"`

if [ -z "$GAME_UID" ] || [ -z "$GAME_GID" ]; then
    fail "Could not determine the game UID and GID from passwd."
fi

if ! awk -F: -v gid="$GAME_GID" \
    '$3 == gid { found = 1 } END { exit !found }' "$GROUP_FILE"
then
    fail "GID $GAME_GID is not present in the staged group file."
fi

echo
echo "============================================================"
echo "LVGD486 - Set Staging Ownership"
echo "============================================================"
echo "Staging root: $DISTRODEV_ROOT"
echo "Game UID:     $GAME_UID"
echo "Game GID:     $GAME_GID"
echo

# ------------------------------------------------------------
# Set the entire staging tree to root:root
# ------------------------------------------------------------

echo "Setting staging tree ownership to root:root..."

chown -R 0:0 "$DISTRODEV_ROOT" ||
    fail "Could not set staging tree ownership to root:root."

# ------------------------------------------------------------
# Assign game ownership to games and home
# ------------------------------------------------------------

echo "Setting games and home ownership to game:game..."

chown -R "${GAME_UID}:${GAME_GID}" \
    "${DISTRODEV_ROOT}/games" \
    "${DISTRODEV_ROOT}/home" ||
    fail "Could not set games and home ownership."

echo
echo "Staging ownership configured successfully."
echo "============================================================"

exit 0