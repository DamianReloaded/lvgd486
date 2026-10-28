#!/bin/sh

# ============================================================
# LVGD486 - Install Kernel Sources
#
# Extract the Linux kernel sources into the development distro
# and install the saved kernel configuration.
#
# Kernel archive:
#   /home/user/download/linux-2.4.37.11.tar.bz2
#
# Installation directory:
#   /home/user/distrodev/home/linux
#
# Kernel configuration:
#   /home/user/doc/kernel-2.4-config/distrodev/.config
#
# The extracted source tree is ready for:
#   make menuconfig
#
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

DOWNLOAD_DIR="/home/user/download"
DISTRODEV_ROOT="/home/user/distrodev"

KERNEL_VERSION="2.4.37.11"
KERNEL_ARCHIVE="linux-${KERNEL_VERSION}.tar.bz2"

KERNEL_SOURCE_NAME="linux-${KERNEL_VERSION}"
KERNEL_SOURCE_DIR="${DISTRODEV_ROOT}/home/linux"

KERNEL_CONFIG_SOURCE="/home/user/doc/kernel-2.4-config/distrodev/.config"
KERNEL_CONFIG_TARGET="${KERNEL_SOURCE_DIR}/.config"

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

# ============================================================
# Prerequisites
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -d "$DOWNLOAD_DIR" ] ||
    fail "Download directory not found: ${DOWNLOAD_DIR}"

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Development distro root not found: ${DISTRODEV_ROOT}"

[ -s "${DOWNLOAD_DIR}/${KERNEL_ARCHIVE}" ] ||
    fail "Kernel archive missing or empty: ${DOWNLOAD_DIR}/${KERNEL_ARCHIVE}"

[ -s "$KERNEL_CONFIG_SOURCE" ] ||
    fail "Kernel configuration missing or empty: ${KERNEL_CONFIG_SOURCE}"

[ ! -e "$KERNEL_SOURCE_DIR" ] ||
    fail "Kernel source destination already exists: ${KERNEL_SOURCE_DIR}"

[ ! -L "$KERNEL_SOURCE_DIR" ] ||
    fail "Kernel source destination is a symlink: ${KERNEL_SOURCE_DIR}"

# ============================================================
# Extract kernel sources
# ============================================================

echo "============================================================"
echo "LVGD486 - Install Linux Kernel Sources"
echo "============================================================"
echo
echo "Kernel version: ${KERNEL_VERSION}"
echo "Archive:        ${DOWNLOAD_DIR}/${KERNEL_ARCHIVE}"
echo "Destination:    ${KERNEL_SOURCE_DIR}"
echo "Configuration:  ${KERNEL_CONFIG_SOURCE}"
echo

mkdir -p "${DISTRODEV_ROOT}/home"

echo "Extracting Linux kernel sources..."

tar -xjf "${DOWNLOAD_DIR}/${KERNEL_ARCHIVE}" \
    -C "${DISTRODEV_ROOT}/home"

# ============================================================
# Rename source directory to stable path
# ============================================================

EXTRACTED_DIR="${DISTRODEV_ROOT}/home/${KERNEL_SOURCE_NAME}"

[ -d "$EXTRACTED_DIR" ] ||
    fail "Expected extracted kernel directory not found: ${EXTRACTED_DIR}"

[ -f "${EXTRACTED_DIR}/Makefile" ] ||
    fail "Kernel Makefile not found in ${EXTRACTED_DIR}"

mv "$EXTRACTED_DIR" "$KERNEL_SOURCE_DIR"

# ============================================================
# Install saved kernel configuration
# ============================================================

echo
echo "Installing saved kernel configuration..."

cp "$KERNEL_CONFIG_SOURCE" "$KERNEL_CONFIG_TARGET"
chmod 644 "$KERNEL_CONFIG_TARGET"

[ -s "$KERNEL_CONFIG_TARGET" ] ||
    fail "Kernel configuration was not installed correctly."

# ============================================================
# Verify installation
# ============================================================

echo
echo "Verifying kernel source installation..."

[ -f "${KERNEL_SOURCE_DIR}/Makefile" ] ||
    fail "Kernel Makefile missing after installation."

[ -s "$KERNEL_CONFIG_TARGET" ] ||
    fail "Kernel .config missing or empty after installation."

echo "Verified kernel Makefile: ${KERNEL_SOURCE_DIR}/Makefile"
echo "Verified kernel config:   ${KERNEL_CONFIG_TARGET}"

# ============================================================
# Final report
# ============================================================

echo
echo "============================================================"
echo "Kernel sources installed successfully"
echo "============================================================"
echo
echo "Kernel source directory:"
echo "  ${KERNEL_SOURCE_DIR}"
echo
echo "Kernel configuration:"
echo "  ${KERNEL_CONFIG_TARGET}"
echo
echo "To configure the kernel, enter the development distro and run:"
echo "  cd /home/linux"
echo "  make menuconfig"
echo

exit 0
