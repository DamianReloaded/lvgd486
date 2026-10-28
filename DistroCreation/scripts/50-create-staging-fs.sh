#!/bin/sh

# ============================================================
# LVGD486 - Create Staging Filesystems
# ============================================================

BUSYBOX_SOURCE="/home/user/sources/busybox/busybox"
DHCP_SCRIPT_SOURCE="/home/user/doc/dhcp/default.script"
DISTRODEV_ROOT="/home/user/distrodev"
DISTROGAME_ROOT="/home/user/distrogame"

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
# Prerequisites
# ------------------------------------------------------------

[ "$(id -u)" -eq 0 ] || fail "Run this script as root."
[ -f "$BUSYBOX_SOURCE" ] || fail "BusyBox binary not found: $BUSYBOX_SOURCE"
[ -x "$BUSYBOX_SOURCE" ] || fail "BusyBox binary is not executable."
[ -f "$DHCP_SCRIPT_SOURCE" ] || fail "DHCP script not found: $DHCP_SCRIPT_SOURCE"

# Discover the compiled applets before modifying staging roots.
APPLET_OUTPUT=$("$BUSYBOX_SOURCE" 2>&1)
STATUS=$?

[ "$STATUS" -eq 0 ] ||
    fail "Could not execute BusyBox to obtain its applet list."

APPLET_LIST=$(
    printf '%s\n' "$APPLET_OUTPUT" |
    awk '
        /Currently defined functions:/ {
            found = 1
            next
        }
        found {
            gsub(/,/, " ")
            for (i = 1; i <= NF; i++) {
                name = $i
                gsub(/[^A-Za-z0-9_.+-]/, "", name)
                if (name != "")
                    print name
            }
        }
    ' |
    sort -u
)

[ -n "$APPLET_LIST" ] ||
    fail "Could not parse the BusyBox applet list."

# ------------------------------------------------------------
# Create staging root
# ------------------------------------------------------------

create_staging_root()
{
    ROOT="$1"

    echo
    echo "============================================================"
    echo "Preparing $ROOT"
    echo "============================================================"

    run mkdir -p \
        "$ROOT/bin" "$ROOT/sbin" "$ROOT/etc" \
        "$ROOT/dev" "$ROOT/proc" "$ROOT/sys" \
        "$ROOT/tmp" "$ROOT/var/log" \
        "$ROOT/usr/bin" "$ROOT/usr/sbin" \
        "$ROOT/usr/lib" "$ROOT/usr/share" \
        "$ROOT/lib" "$ROOT/root" "$ROOT/home" \
        "$ROOT/mnt" "$ROOT/opt" "$ROOT/games/lvgd/demos"

    # Clear existing BusyBox files and links in /bin.
    echo "Cleaning $ROOT/bin"
    run find "$ROOT/bin" -mindepth 1 -maxdepth 1 -exec rm -f {} \;

    # Copy and verify BusyBox before creating any links.
    run cp "$BUSYBOX_SOURCE" "$ROOT/bin/busybox"
    run chmod 0755 "$ROOT/bin/busybox"

    [ -s "$ROOT/bin/busybox" ] ||
        fail "BusyBox binary missing or empty: $ROOT/bin/busybox"

    echo "Verified: $ROOT/bin/busybox"

    # Create links only after the binary is in place.
    LINK_COUNT=0

    for APPLET in $APPLET_LIST
    do
        [ "$APPLET" = "busybox" ] && continue

        run ln -s busybox "$ROOT/bin/$APPLET"
        LINK_COUNT=$((LINK_COUNT + 1))
    done

    # Ensure shell test applet links exist.
    for APPLET in '[' test
    do
        if [ ! -e "$ROOT/bin/$APPLET" ]; then
            run ln -s busybox "$ROOT/bin/$APPLET"
            LINK_COUNT=$((LINK_COUNT + 1))
        fi
    done

    [ "$LINK_COUNT" -gt 0 ] ||
        fail "No applet symlinks created in $ROOT/bin"

    run chmod 1777 "$ROOT/tmp"

    # Install the DHCP script in DISTRODEV_ROOT only.
    if [ "$ROOT" = "$DISTRODEV_ROOT" ]; then
        echo "Installing DHCP default.script in development root"

        run mkdir -p "$ROOT/usr/share/udhcpc"
        run cp "$DHCP_SCRIPT_SOURCE" "$ROOT/usr/share/udhcpc/default.script"
        run chmod 0755 "$ROOT/usr/share/udhcpc/default.script"

        [ -s "$ROOT/usr/share/udhcpc/default.script" ] ||
            fail "DHCP script missing or empty in development root."

        echo "Verified: $ROOT/usr/share/udhcpc/default.script"
    fi

    echo "Created $LINK_COUNT applet symlinks in $ROOT/bin"
}

# ------------------------------------------------------------
# Build both roots
# ------------------------------------------------------------

create_staging_root "$DISTRODEV_ROOT"
create_staging_root "$DISTROGAME_ROOT"

echo
echo "SUCCESS: Both staging roots have BusyBox and applet links."
echo "SUCCESS: Shell test applet links verified."
echo "SUCCESS: DHCP default.script installed in DISTRODEV_ROOT only."
exit 0
