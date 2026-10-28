#!/bin/sh

# ============================================================
# LVGD486 - Kernel Font Permission Diagnostics
# Script: 64-diagnose-kfontop.sh
# ============================================================

KERNEL_DIR="/home/user/distrodev/home/linux"
LOG_FILE="log.txt"

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

[ -d "$KERNEL_DIR" ] ||
    fail "Kernel source directory not found: $KERNEL_DIR"

[ -f "$KERNEL_DIR/drivers/char/vt.c" ] ||
    fail "vt.c not found in kernel source tree."

if ! (
    exec > "$LOG_FILE" 2>&1

    echo "============================================================"
    echo "LVGD486 - KDFONTOP Permission Diagnostics"
    echo "============================================================"
    echo "Date: $(date)"
    echo "Kernel source: $KERNEL_DIR"
    echo

    cd "$KERNEL_DIR" || exit 1

    echo "=== 1. Kernel version ==="
    grep -E '^(VERSION|PATCHLEVEL|SUBLEVEL|EXTRAVERSION) =' Makefile
    echo

    echo "=== 2. KD_FONT_OP_GET occurrences ==="
    grep -R -n 'KD_FONT_OP_GET' \
        drivers/char include 2>&1
    echo

    echo "=== 3. KDFONTOP occurrences ==="
    grep -R -n 'KDFONTOP' \
        drivers/char include 2>&1
    echo

    echo "=== 4. Permission checks and EPERM in vt.c ==="
    grep -n -E -C 5 'EPERM|perm|KDFONTOP|KD_FONT_OP_GET' \
        drivers/char/vt.c 2>&1
    echo

    echo "=== 5. Font operation implementation ==="
    grep -R -n -E -C 8 \
        'con_font_op|KD_FONT_OP_GET|return -EPERM' \
        drivers/char 2>&1
    echo

    echo "=== 6. EPERM checks in relevant console sources ==="
    grep -n -E -C 3 'EPERM|capable|KD_FONT_OP_GET|con_font_op' \
        drivers/char/console.c \
        drivers/char/vt.c \
        drivers/char/consolemap.c 2>&1
    echo

    echo "=== 7. Complete KDFONTOP handler in vt.c ==="
    awk '
        /case KDFONTOP:/ { show = 1 }
        show { print }
        show && /break;/ { count++ }
        show && count >= 2 { exit }
    ' drivers/char/vt.c
    echo

    echo "=== Diagnostics finished ==="
) ; then
    fail "Could not generate diagnostic log."
fi

[ -s "$LOG_FILE" ] ||
    fail "Diagnostic log is empty."

echo "Diagnostics saved to: $LOG_FILE"
echo "Please share the contents of this file."
