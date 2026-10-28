
#!/bin/sh

# ============================================================
# LVGD486 - Build Linux Kernel 2.4.37.11
# ============================================================

# Constants
SOURCE_DIR="/home/user/sources/linux"
CONFIG_FILE="/home/user/doc/kernel-2.4-config/distrodev/.config"
MAKE_EXECUTABLE="/usr/local/bin/make"
KERNEL_VERSION="2.4.37.11"

# Verify root privileges
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run this script as root."
    exit 1
fi

# Verify kernel source
if [ ! -f "${SOURCE_DIR}/Makefile" ]; then
    echo "Error: Kernel source not found at ${SOURCE_DIR}"
    exit 1
fi

# Verify kernel configuration
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Kernel configuration not found at ${CONFIG_FILE}"
    exit 1
fi

# Verify GNU Make
if [ ! -x "$MAKE_EXECUTABLE" ]; then
    echo "Error: GNU Make not found at ${MAKE_EXECUTABLE}"
    exit 1
fi

# Copy kernel configuration
echo "Installing kernel configuration..."
cp "$CONFIG_FILE" "${SOURCE_DIR}/.config" || exit 1

# Enter kernel source directory
cd "$SOURCE_DIR" || exit 1

# Generate configuration headers
echo "Generating kernel configuration headers..."
"$MAKE_EXECUTABLE" ARCH=i386 CC=gcc-3.3 oldconfig || exit 1

# Build kernel
echo "Building Linux ${KERNEL_VERSION}..."
"$MAKE_EXECUTABLE" ARCH=i386 CC=gcc-3.3 bzImage || exit 1

# Verify kernel image
if [ ! -f "${SOURCE_DIR}/arch/i386/boot/bzImage" ]; then
    echo "Error: Kernel build finished without producing bzImage."
    exit 1
fi

echo "Linux ${KERNEL_VERSION} built successfully."
echo "Kernel image: ${SOURCE_DIR}/arch/i386/boot/bzImage"
