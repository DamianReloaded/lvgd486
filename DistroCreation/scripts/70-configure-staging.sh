#!/bin/sh

# ============================================================
# LVGD486 - Configure Staging Systems
# Script: 21-configure-staging.sh
# ============================================================

DISTRODEV_ROOT="/home/user/distrodev"
DISTROGAME_ROOT="/home/user/distrogame"

DISTRODEV_RCS_SOURCE="/home/user/doc/rcS/distrodev/rcS"
DISTROGAME_RCS_SOURCE="/home/user/doc/rcS/distrogame/rcS"

SYSTEM_PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin"

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

[ "$(id -u)" -eq 0 ] || fail "Run this script as root."

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Development staging root not found: $DISTRODEV_ROOT"

[ -d "$DISTROGAME_ROOT" ] ||
    fail "Gaming staging root not found: $DISTROGAME_ROOT"

[ -f "$DISTRODEV_RCS_SOURCE" ] ||
    fail "Development rcS source not found: $DISTRODEV_RCS_SOURCE"

[ -f "$DISTROGAME_RCS_SOURCE" ] ||
    fail "Gaming rcS source not found: $DISTROGAME_RCS_SOURCE"

# ------------------------------------------------------------
# Configure one staging root
# ------------------------------------------------------------

configure_staging()
{
    ROOT="$1"
    RCS_SOURCE="$2"

    echo
    echo "============================================================"
    echo "Configuring: $ROOT"
    echo "============================================================"

    run mkdir -p "$ROOT/etc/init.d"

    # Configure init and start an interactive shell with PATH.
    echo "Writing $ROOT/etc/inittab"

    cat > "$ROOT/etc/inittab" <<'EOF'
::sysinit:/etc/init.d/rcS
::respawn:/bin/sh -c '. /etc/profile; exec /bin/sh'
::ctrlaltdel:/sbin/reboot
EOF

    STATUS=$?
    [ "$STATUS" -eq 0 ] ||
        fail "Could not write $ROOT/etc/inittab"

    # Install the system-wide shell environment.
    echo "Writing $ROOT/etc/profile"

    cat > "$ROOT/etc/profile" <<EOF
# ============================================================
# LVGD486 - System-wide Shell Environment
# ============================================================

PATH=$SYSTEM_PATH
export PATH
EOF

    STATUS=$?
    [ "$STATUS" -eq 0 ] ||
        fail "Could not write $ROOT/etc/profile"

    run chmod 0644 "$ROOT/etc/profile"

    # Install the distribution-specific startup script.
    echo "Copying $RCS_SOURCE to $ROOT/etc/init.d/rcS"

    run cp "$RCS_SOURCE" "$ROOT/etc/init.d/rcS"
    run chmod 0755 "$ROOT/etc/init.d/rcS"

    # Verify configuration files.
    [ -s "$ROOT/etc/inittab" ] ||
        fail "Missing or empty inittab: $ROOT/etc/inittab"

    [ -s "$ROOT/etc/profile" ] ||
        fail "Missing or empty profile: $ROOT/etc/profile"

    [ -x "$ROOT/etc/init.d/rcS" ] ||
        fail "Startup script is not executable: $ROOT/etc/init.d/rcS"

    echo "Configured PATH: $SYSTEM_PATH"
    echo "Configuration completed for $ROOT"
}

# ------------------------------------------------------------
# Configure both distros
# ------------------------------------------------------------

configure_staging "$DISTRODEV_ROOT" "$DISTRODEV_RCS_SOURCE"
configure_staging "$DISTROGAME_ROOT" "$DISTROGAME_RCS_SOURCE"

echo
echo "SUCCESS: Both staging systems are configured."
echo "SUCCESS: PATH is configured for interactive shells."
exit 0
