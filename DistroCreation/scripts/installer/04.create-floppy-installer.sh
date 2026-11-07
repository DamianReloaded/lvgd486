#!/bin/bash

set -eu

# ============================================================
# LVGD486 - Build BusyBox, Linux Kernel and FAT12 Floppy
#
# Floppy filesystem: FAT12
# Floppy contents:   SYSLINUX, kernel, compressed initrd
# Root filesystem:   Minimal ext2 initrd in RAM
# Init:              Custom /sbin/init
# Shell:             BusyBox ash
# Installer:         Mounted from CD-ROM at /install-media
# Utilities:         Loaded from the mounted CD-ROM
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

IMAGE_DIR="/home/user/images"

BUSYBOX_SOURCE="/home/user/sources/busybox"
BUSYBOX_CONFIG_SOURCE="/home/user/doc/busybox-1.0-config/floppy/.config"
BUSYBOX="${BUSYBOX_SOURCE}/busybox"

KERNEL_SOURCE="/home/user/sources/linux"
KERNEL_CONFIG_SOURCE="/home/user/doc/kernel-2.4-config/floppy/.config"
KERNEL_CONFIG="${KERNEL_SOURCE}/.config"
KERNEL="${KERNEL_SOURCE}/arch/i386/boot/bzImage"

SYSLINUX="/home/user/sources/build/syslinux/syslinux"

FLOPPY_IMAGE="${IMAGE_DIR}/lvgd486-installer-floppy.img"

MOUNT_POINT="/mnt/lvgd486-floppy"
LOOP_DEVICE="/dev/loop1"

WORK_DIR="${IMAGE_DIR}/.lvgd486-floppy-build"
INITRD_IMAGE="${WORK_DIR}/initrd.ext2"
INITRD_COMPRESSED="${WORK_DIR}/initrd.gz"
FLOPPY_CONFIG="${WORK_DIR}/syslinux.cfg"

FLOPPY_SIZE_BYTES=1474560
FLOPPY_SECTORS=2880
VOLUME_NAME="LVGD486"

# Raw ext2 image size.
INITRD_SIZE_MB=2

# RAM disk capacity provided to the Linux kernel.
INITRD_RAM_SIZE_KB=3072

# Leave room for FAT allocation rounding and filesystem overhead.
FLOPPY_RESERVE_BYTES=8192

HOST_MKE2FS=""
HOST_E2FSCK=""
MKFS=""

MOUNTED=0
LOOP_ATTACHED=0
IMAGE_CREATED=0
SUCCESS=0

BUSYBOX_APPLETS="
[
cat
cp
echo
grep
id
ls
mkdir
mount
printf
rm
sleep
sync
tar
umount
"

# ============================================================
# Cleanup
# ============================================================

Cleanup()
{
    set +e

    if [ "$MOUNTED" -eq 1 ]; then
        umount "$MOUNT_POINT"
        MOUNTED=0
    fi

    if [ "$LOOP_ATTACHED" -eq 1 ]; then
        losetup -d "$LOOP_DEVICE"
        LOOP_ATTACHED=0
    fi

    if [ "$IMAGE_CREATED" -eq 1 ] && [ "$SUCCESS" -ne 1 ]; then
        echo "Build failed. Removing incomplete floppy image."
        rm -f "$FLOPPY_IMAGE"
    fi

    rm -rf "$WORK_DIR"
}

trap Cleanup EXIT
trap 'exit 1' HUP INT TERM

# ============================================================
# Helpers
# ============================================================

RequireCommand()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ERROR: Required command not found: $1"
        exit 1
    fi
}

RequireFile()
{
    if [ ! -f "$1" ]; then
        echo "ERROR: Required file not found: $1"
        exit 1
    fi
}

RequireExecutable()
{
    if [ ! -x "$1" ]; then
        echo "ERROR: Required executable not found: $1"
        exit 1
    fi
}

RequireConfigEnabled()
{
    if ! grep -q "^$1=y$" "$2"; then
        echo "ERROR: Required configuration option is not enabled: $1"
        exit 1
    fi
}

RequireKernelOption()
{
    RequireConfigEnabled "$1" "$KERNEL_CONFIG"
}

GetFileSize()
{
    wc -c < "$1" | tr -d ' '
}

GetAvailableBytes()
{
    df -Pk "$MOUNT_POINT" | awk 'END { print $4 * 1024 }'
}

FormatSize()
{
    awk -v bytes="$1" 'BEGIN {
        printf "%.2f MiB (%.2f KiB, %d bytes)", bytes / 1048576, bytes / 1024, bytes
    }'
}

PrintFloppySizeReport()
{
    local kernelSize
    local initrdSize
    local configSize
    local bootloaderSize
    local requiredSize
    local availableSize
    local deficit

    kernelSize="$(GetFileSize "$KERNEL")"
    initrdSize="$(GetFileSize "$INITRD_COMPRESSED")"
    configSize="$(GetFileSize "$FLOPPY_CONFIG")"

    bootloaderSize=0

    if [ -f "$MOUNT_POINT/ldlinux.sys" ]; then
        bootloaderSize="$(GetFileSize "$MOUNT_POINT/ldlinux.sys")"
    fi

    requiredSize=$((kernelSize + initrdSize + configSize + bootloaderSize))
    availableSize="$(GetAvailableBytes)"

    echo
    echo "============================================================"
    echo "FLOPPY CAPACITY REPORT"
    echo "============================================================"
    echo
    printf "%-24s %s\n" "Floppy capacity:" "$(FormatSize "$FLOPPY_SIZE_BYTES")"
    printf "%-24s %s\n" "Available space:" "$(FormatSize "$availableSize")"
    echo
    echo "Components:"
    printf "  %-22s %s\n" "SYSLINUX (ldlinux.sys):" "$(FormatSize "$bootloaderSize")"
    printf "  %-22s %s\n" "Kernel (bzImage):" "$(FormatSize "$kernelSize")"
    printf "  %-22s %s\n" "Compressed initrd:" "$(FormatSize "$initrdSize")"
    printf "  %-22s %s\n" "Configuration:" "$(FormatSize "$configSize")"
    echo
    printf "%-24s %s\n" "Total component size:" "$(FormatSize "$requiredSize")"
    printf "%-24s %s\n" "Safety reserve:" "$(FormatSize "$FLOPPY_RESERVE_BYTES")"
    printf "%-24s %s\n" "Total required:" "$(FormatSize "$((requiredSize + FLOPPY_RESERVE_BYTES))")"
    echo

    if [ "$((requiredSize + FLOPPY_RESERVE_BYTES))" -gt "$availableSize" ]; then
        deficit=$((requiredSize + FLOPPY_RESERVE_BYTES - availableSize))
        echo "ERROR: Components do not fit on the floppy."
        printf "Additional space required: %s\n" "$(FormatSize "$deficit")"
    else
        echo "The components fit within the available floppy space."
    fi

    echo "============================================================"
    echo
}

CheckFloppyCapacity()
{
    local kernelSize
    local initrdSize
    local configSize
    local bootloaderSize
    local requiredSize
    local availableSize

    kernelSize="$(GetFileSize "$KERNEL")"
    initrdSize="$(GetFileSize "$INITRD_COMPRESSED")"
    configSize="$(GetFileSize "$FLOPPY_CONFIG")"
    bootloaderSize="$(GetFileSize "$MOUNT_POINT/ldlinux.sys")"

    requiredSize=$((kernelSize + initrdSize + configSize + bootloaderSize))
    availableSize="$(GetAvailableBytes)"

    PrintFloppySizeReport

    if [ "$((requiredSize + FLOPPY_RESERVE_BYTES))" -gt "$availableSize" ]; then
        echo "ERROR: Reduce the kernel or initrd size before creating the floppy."
        exit 1
    fi
}

CopyFloppyFile()
{
    local source="$1"
    local destination="$2"

    if ! cp "$source" "$destination"; then
        echo
        echo "ERROR: Failed to copy $source to the floppy."
        echo "The actual FAT12 allocation requirements may exceed the estimate."
        PrintFloppySizeReport
        exit 1
    fi
}

CreateDevice()
{
    local type="$1"
    local path="$2"
    local major="$3"
    local minor="$4"
    local mode="$5"

    if [ -e "$path" ] || [ -L "$path" ]; then
        rm -f "$path"
    fi

    mknod "$path" "$type" "$major" "$minor"
    chmod "$mode" "$path"
}

CreateIdeDevices()
{
    local root="$1"
    local name="$2"
    local major="$3"
    local base="$4"
    local partition

    CreateDevice b "${root}/dev/${name}" "$major" "$base" 660

    for partition in 1 2 3 4 5 6 7 8; do
        CreateDevice b "${root}/dev/${name}${partition}" \
            "$major" "$((base + partition))" 660
    done
}

CreateScsiDevices()
{
    local root="$1"
    local name="$2"
    local base="$3"
    local partition

    CreateDevice b "${root}/dev/${name}" 8 "$base" 660

    for partition in 1 2 3 4 5 6 7 8; do
        CreateDevice b "${root}/dev/${name}${partition}" \
            8 "$((base + partition))" 660
    done
}

ValidateStaticI386Executable()
{
    local path="$1"
    local description="$2"
    local info

    RequireExecutable "$path"

    info="$(file "$path")"
    echo "$info"

    case "$info" in
        *"ELF 32-bit"*) ;;
        *)
            echo "ERROR: $description must be a 32-bit executable."
            exit 1
            ;;
    esac

    case "$info" in
        *80386*|*i386*) ;;
        *)
            echo "ERROR: $description must target Intel 80386."
            exit 1
            ;;
    esac

    case "$info" in
        *"statically linked"*) ;;
        *)
            echo "ERROR: $description must be statically linked."
            exit 1
            ;;
    esac
}

ValidateBusyBoxConfig()
{
    local config="$BUSYBOX_SOURCE/.config"
    local option
    local enabled=0

    echo "Checking BusyBox configuration..."

    for option in \
        CONFIG_STATIC \
        CONFIG_ASH \
        CONFIG_FEATURE_SH_IS_ASH \
        CONFIG_TEST \
        CONFIG_ECHO \
		CONFIG_PRINTF \
        CONFIG_CAT \
        CONFIG_CP \
        CONFIG_GREP \
        CONFIG_ID \
        CONFIG_LS \
        CONFIG_MKDIR \
        CONFIG_MOUNT \
        CONFIG_RM \
        CONFIG_SLEEP \
        CONFIG_SYNC \
        CONFIG_TAR \
        CONFIG_UMOUNT
    do
        RequireConfigEnabled "$option" "$config"
    done

    for option in \
        CONFIG_FEATURE_BUFFERS_USE_MALLOC \
        CONFIG_FEATURE_BUFFERS_GO_ON_STACK \
        CONFIG_FEATURE_BUFFERS_GO_IN_BSS
    do
        if grep -q "^${option}=y$" "$config"; then
            enabled=$((enabled + 1))
        fi
    done

    if [ "$enabled" -ne 1 ]; then
        echo "ERROR: Exactly one BusyBox buffer allocation option must be enabled."
        exit 1
    fi

    echo "BusyBox configuration is valid."
}

ValidateBusyBoxExecutable()
{
    echo "Checking BusyBox executable..."
    ValidateStaticI386Executable "$BUSYBOX" "BusyBox"
}

ValidateKernelConfig()
{
    echo "Checking kernel configuration..."

    RequireKernelOption CONFIG_BLK_DEV_INITRD
    RequireKernelOption CONFIG_BLK_DEV_RAM
    RequireKernelOption CONFIG_EXT2_FS
    RequireKernelOption CONFIG_PROC_FS
    RequireKernelOption CONFIG_ISO9660_FS

    RequireKernelOption CONFIG_BLK_DEV_IDE
    RequireKernelOption CONFIG_BLK_DEV_IDECD

    echo "Required kernel options are enabled."
}

# ============================================================
# Preflight
# ============================================================

echo "Checking build environment..."

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: Run this script as root."
    exit 1
fi

for COMMAND in \
    make gcc-3.3 yes dd cp mkdir rm mount umount \
    losetup grep file awk wc sync df tr cat id \
    mknod chmod ln gzip od
do
    RequireCommand "$COMMAND"
done

HOST_MKE2FS="$(command -v mke2fs || true)"
HOST_E2FSCK="$(command -v e2fsck || true)"

if [ -z "$HOST_MKE2FS" ]; then
    echo "ERROR: Host mke2fs is required to create the initrd."
    exit 1
fi

if [ -z "$HOST_E2FSCK" ]; then
    echo "ERROR: Host e2fsck is required to check the initrd."
    exit 1
fi

if command -v mkfs.msdos >/dev/null 2>&1; then
    MKFS="$(command -v mkfs.msdos)"
elif command -v mkdosfs >/dev/null 2>&1; then
    MKFS="$(command -v mkdosfs)"
else
    echo "ERROR: Neither mkfs.msdos nor mkdosfs is available."
    exit 1
fi

for INPUT in "$BUSYBOX_CONFIG_SOURCE" "$KERNEL_CONFIG_SOURCE"; do
    RequireFile "$INPUT"
done

for DIRECTORY in "$BUSYBOX_SOURCE" "$KERNEL_SOURCE"; do
    if [ ! -d "$DIRECTORY" ]; then
        echo "ERROR: Required directory not found: $DIRECTORY"
        exit 1
    fi
done

RequireExecutable "$SYSLINUX"

if [ -e "$FLOPPY_IMAGE" ]; then
    echo "ERROR: Output image already exists: $FLOPPY_IMAGE"
    exit 1
fi

if mount | grep -q " $MOUNT_POINT "; then
    echo "ERROR: Mount point is already in use: $MOUNT_POINT"
    exit 1
fi

if losetup "$LOOP_DEVICE" >/dev/null 2>&1; then
    echo "ERROR: Loop device is already attached: $LOOP_DEVICE"
    exit 1
fi

mkdir -p "$IMAGE_DIR"
mkdir -p "$MOUNT_POINT"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

# ============================================================
# Configure and build BusyBox
# ============================================================

echo
echo "============================================================"
echo "Building BusyBox"
echo "============================================================"

cp "$BUSYBOX_CONFIG_SOURCE" "$BUSYBOX_SOURCE/.config"

ValidateBusyBoxConfig

cd "$BUSYBOX_SOURCE"

make clean

if [ ! -f .config ]; then
    echo "ERROR: BusyBox .config was removed by make clean."
    exit 1
fi

ValidateBusyBoxConfig

echo "Compiling BusyBox..."

yes "n" | make ARCH=i386 CC=gcc-3.3 LDFLAGS="-static -Wl,--gc-sections -Wl,--strip-all"

RequireExecutable "$BUSYBOX"
ValidateBusyBoxExecutable

echo "BusyBox build successful."

# ============================================================
# Configure and build Linux kernel
# ============================================================

echo
echo "============================================================"
echo "Building Linux kernel"
echo "============================================================"

cp "$KERNEL_CONFIG_SOURCE" "$KERNEL_CONFIG"

cd "$KERNEL_SOURCE"

echo "Running kernel oldconfig..."

yes "n" | make ARCH=i386 CC=gcc-3.3 oldconfig

echo "Building kernel dependencies..."

make ARCH=i386 CC=gcc-3.3 dep

echo "Compiling kernel..."

make ARCH=i386 CC=gcc-3.3 bzImage

RequireFile "$KERNEL"

echo "Kernel build successful."

ValidateKernelConfig

# ============================================================
# Create minimal ext2 initrd image
# ============================================================

echo
echo "============================================================"
echo "Creating minimal ext2 initrd"
echo "============================================================"

echo "Creating ${INITRD_SIZE_MB} MiB initrd image..."

dd if=/dev/zero of="$INITRD_IMAGE" \
    bs=1024 count=$((INITRD_SIZE_MB * 1024)) 2>/dev/null

"$HOST_MKE2FS" -F -r 0 -b 1024 -m 0 "$INITRD_IMAGE"

echo "Attaching initrd loop device..."

losetup "$LOOP_DEVICE" "$INITRD_IMAGE"
LOOP_ATTACHED=1

echo "Mounting initrd..."

mount -t ext2 "$LOOP_DEVICE" "$MOUNT_POINT"
MOUNTED=1

# ============================================================
# Create minimal initrd filesystem layout
# ============================================================

echo "Creating initrd directories..."

mkdir -p "$MOUNT_POINT/bin"
mkdir -p "$MOUNT_POINT/dev"
mkdir -p "$MOUNT_POINT/dev/cdroms"
mkdir -p "$MOUNT_POINT/etc"
mkdir -p "$MOUNT_POINT/install-media"
mkdir -p "$MOUNT_POINT/proc"
mkdir -p "$MOUNT_POINT/sbin"
mkdir -p "$MOUNT_POINT/tmp"

chmod 1777 "$MOUNT_POINT/tmp"

# ============================================================
# Install BusyBox
# ============================================================

echo "Installing BusyBox..."

cp "$BUSYBOX" "$MOUNT_POINT/bin/busybox"
chmod 755 "$MOUNT_POINT/bin/busybox"

echo "Creating BusyBox applet symlinks..."

for APPLET in $BUSYBOX_APPLETS; do
    ln -s busybox "$MOUNT_POINT/bin/$APPLET"
done

ln -s busybox "$MOUNT_POINT/bin/sh"

# ============================================================
# Create device nodes
# ============================================================

echo "Creating device nodes..."

CreateDevice c "$MOUNT_POINT/dev/console" 5 1 600
CreateDevice c "$MOUNT_POINT/dev/null" 1 3 666
CreateDevice c "$MOUNT_POINT/dev/zero" 1 5 666
CreateDevice c "$MOUNT_POINT/dev/tty" 5 0 666
CreateDevice c "$MOUNT_POINT/dev/tty0" 4 0 600
CreateDevice c "$MOUNT_POINT/dev/tty1" 4 1 600
CreateDevice c "$MOUNT_POINT/dev/ptmx" 5 2 666

CreateDevice b "$MOUNT_POINT/dev/ram0" 1 0 600

# IDE disks and partitions.
CreateIdeDevices "$MOUNT_POINT" hda 3 0
CreateIdeDevices "$MOUNT_POINT" hdb 3 64
CreateIdeDevices "$MOUNT_POINT" hdc 22 0
CreateIdeDevices "$MOUNT_POINT" hdd 22 64
CreateIdeDevices "$MOUNT_POINT" hde 33 0
CreateIdeDevices "$MOUNT_POINT" hdf 33 64
CreateIdeDevices "$MOUNT_POINT" hdg 34 0
CreateIdeDevices "$MOUNT_POINT" hdh 34 64

# SCSI disks and CD-ROMs.
CreateScsiDevices "$MOUNT_POINT" sda 0
CreateScsiDevices "$MOUNT_POINT" sdb 16
CreateScsiDevices "$MOUNT_POINT" sdc 32
CreateScsiDevices "$MOUNT_POINT" sdd 48

CreateDevice b "$MOUNT_POINT/dev/scd0" 11 0 660
CreateDevice b "$MOUNT_POINT/dev/scd1" 11 1 660
CreateDevice b "$MOUNT_POINT/dev/scd2" 11 2 660
CreateDevice b "$MOUNT_POINT/dev/scd3" 11 3 660

# The IDE CD-ROM on hdc uses major 22, minor 0.
# Create a real block device for the traditional devfs-style path.
CreateDevice b "$MOUNT_POINT/dev/cdroms/cdrom0" 22 0 660

# Keep /dev/hdc available and make /dev/cdrom resolve to the
# explicit CD-ROM device path.
ln -s cdroms/cdrom0 "$MOUNT_POINT/dev/cdrom"

# ============================================================
# Verify CD-ROM device nodes
# ============================================================

echo "Verifying CD-ROM device nodes..."

if [ ! -b "$MOUNT_POINT/dev/hdc" ]; then
    echo "ERROR: /dev/hdc is not a block device."
    exit 1
fi

if [ ! -b "$MOUNT_POINT/dev/cdroms/cdrom0" ]; then
    echo "ERROR: /dev/cdroms/cdrom0 is not a block device."
    exit 1
fi

if [ ! -L "$MOUNT_POINT/dev/cdrom" ]; then
    echo "ERROR: /dev/cdrom is not a symlink."
    exit 1
fi

echo "IDE CD-ROM device mapping:"
ls -l "$MOUNT_POINT/dev/hdc"
ls -l "$MOUNT_POINT/dev/cdroms/cdrom0"
ls -l "$MOUNT_POINT/dev/cdrom"

# ============================================================
# Create /linuxrc
# ============================================================

echo "Creating /linuxrc..."

cat > "$MOUNT_POINT/linuxrc" <<'LINUXRC'
#!/bin/sh

export PATH=/bin:/sbin
export HOME=/

echo
echo "============================================================"
echo " LVGD486 - Initial RAM Disk"
echo "============================================================"
echo

mount -t proc proc /proc 2>/dev/null

echo "CD-ROM device mapping:"
ls -l /dev/hdc
ls -l /dev/cdroms/cdrom0
ls -l /dev/cdrom

echo
echo "Searching for the installer CD-ROM..."
echo

attempt=0
media_found=0

while [ "$attempt" -lt 30 ]; do
    for device in \
        /dev/cdroms/cdrom0 \
        /dev/hdc \
        /dev/hdd \
        /dev/hdb \
        /dev/hda \
        /dev/hde \
        /dev/hdf \
        /dev/hdg \
        /dev/hdh \
        /dev/scd0 \
        /dev/scd1 \
        /dev/scd2 \
        /dev/scd3
    do
        if mount -t iso9660 -o ro "$device" /install-media 2>/dev/null; then
            if [ -f /install-media/distrodev.tar.gz ] &&
               [ -x /install-media/install.sh ]; then
                echo
                echo "Installer CD found on $device"
                media_found=1
                break
            fi

            umount /install-media 2>/dev/null
        fi
    done

    if [ "$media_found" -eq 1 ]; then
        break
    fi

    attempt=$((attempt + 1))
    sleep 1
done

if [ "$media_found" -ne 1 ]; then
    echo
    echo "WARNING: Installer CD-ROM was not found."
    echo
    echo "Check that the CD-ROM is connected as IDE secondary master (hdc)."
    echo "The expected device is /dev/cdroms/cdrom0 (major 22, minor 0)."
    echo
    echo "You can retry manually with:"
    echo
    echo "  mount -t iso9660 -o ro /dev/cdroms/cdrom0 /install-media"
    echo
else
    echo
    echo "Installer media mounted at /install-media"
    echo
fi

exit 0
LINUXRC

chmod 755 "$MOUNT_POINT/linuxrc"

# ============================================================
# Create /sbin/init
# ============================================================

echo "Creating /sbin/init..."

cat > "$MOUNT_POINT/sbin/init" <<'INIT'
#!/bin/sh

export HOME=/
export PATH=/install-media/bin:/install-media/sbin:/bin:/sbin
export PS1='LVGD486# '

mount -t proc proc /proc 2>/dev/null

echo
echo "============================================================"
echo " LVGD486 - Installer Environment"
echo "============================================================"
echo

if [ -f /install-media/install.sh ]; then
    echo "Installer CD detected."
    echo
    echo "Run the installer with:"
    echo
    echo "  /install-media/install.sh"
    echo
    echo "GRUB Legacy is available at /install-media/bin/grub."
    echo "e2fsprogs utilities are available under /install-media/sbin."
else
    echo "Installer CD not mounted."
    echo
    echo "Insert the installer CD and mount it manually."
    echo
    echo "Expected CD-ROM device: /dev/cdroms/cdrom0"
    echo
    echo "Mount it with:"
    echo
    echo "  mount -t iso9660 -o ro /dev/cdroms/cdrom0 /install-media"
fi

echo

exec /bin/sh
INIT

chmod 755 "$MOUNT_POINT/sbin/init"

# ============================================================
# Create /etc/profile
# ============================================================

echo "Creating /etc/profile..."

cat > "$MOUNT_POINT/etc/profile" <<'PROFILE'
export HOME=/
export PATH=/install-media/bin:/install-media/sbin:/bin:/sbin
export PS1='LVGD486# '
PROFILE

# ============================================================
# Verify minimal initrd contents
# ============================================================

echo "Verifying initrd contents..."

for REQUIRED_FILE in \
    linuxrc \
    bin/busybox \
    bin/sh \
    sbin/init \
    etc/profile \
    dev/console \
    dev/null \
    dev/zero \
    dev/ram0 \
    dev/hdc \
    dev/cdroms/cdrom0 \
    dev/cdrom \
    dev/scd0
do
    if [ ! -e "$MOUNT_POINT/$REQUIRED_FILE" ]; then
        echo "ERROR: Missing initrd file: $REQUIRED_FILE"
        exit 1
    fi
done

for APPLET in $BUSYBOX_APPLETS; do
    if [ ! -L "$MOUNT_POINT/bin/$APPLET" ]; then
        echo "ERROR: Missing BusyBox symlink: $APPLET"
        exit 1
    fi
done

if [ ! -L "$MOUNT_POINT/bin/sh" ]; then
    echo "ERROR: Missing BusyBox shell symlink."
    exit 1
fi

if [ ! -b "$MOUNT_POINT/dev/cdroms/cdrom0" ]; then
    echo "ERROR: CD-ROM device node is invalid."
    exit 1
fi

echo "Initrd filesystem usage:"
df -k "$MOUNT_POINT"

AVAILABLE_KB="$(df -k "$MOUNT_POINT" | awk 'END { print $4 }')"

if [ "$AVAILABLE_KB" -lt 128 ]; then
    echo "ERROR: Initrd is nearly full."
    exit 1
fi

# ============================================================
# Unmount and check initrd
# ============================================================

echo "Unmounting initrd..."

sync
umount "$MOUNT_POINT"
MOUNTED=0

echo "Detaching initrd loop device..."

losetup -d "$LOOP_DEVICE"
LOOP_ATTACHED=0

echo "Checking ext2 filesystem..."

"$HOST_E2FSCK" -fn "$INITRD_IMAGE"

# ============================================================
# Compress initrd
# ============================================================

echo "Compressing initrd..."

gzip -9 -c "$INITRD_IMAGE" > "$INITRD_COMPRESSED"
gzip -t "$INITRD_COMPRESSED"

INITRD_SIZE="$(GetFileSize "$INITRD_COMPRESSED")"
echo "Compressed initrd size: $(FormatSize "$INITRD_SIZE")"

# ============================================================
# Create FAT12 floppy image
# ============================================================

echo
echo "============================================================"
echo "Creating FAT12 floppy"
echo "============================================================"

IMAGE_CREATED=1

dd if=/dev/zero of="$FLOPPY_IMAGE" bs=512 count="$FLOPPY_SECTORS" 2>/dev/null

ACTUAL_SIZE="$(GetFileSize "$FLOPPY_IMAGE")"

if [ "$ACTUAL_SIZE" -ne "$FLOPPY_SIZE_BYTES" ]; then
    echo "ERROR: Incorrect floppy image size."
    exit 1
fi

echo "Attaching floppy loop device..."

losetup "$LOOP_DEVICE" "$FLOPPY_IMAGE"
LOOP_ATTACHED=1

echo "Formatting FAT12..."

"$MKFS" -F 12 -n "$VOLUME_NAME" "$LOOP_DEVICE"

# Install SYSLINUX before checking capacity, so ldlinux.sys is included.
echo "Installing SYSLINUX..."

"$SYSLINUX" "$LOOP_DEVICE"

echo "Mounting floppy..."

mount -t msdos -o umask=000 "$LOOP_DEVICE" "$MOUNT_POINT"
MOUNTED=1

# ============================================================
# Create SYSLINUX 2.11 configuration
# ============================================================

cat > "$FLOPPY_CONFIG" <<EOF
DEFAULT lvgd486
PROMPT 1
TIMEOUT 50

LABEL lvgd486
    KERNEL bzImage
    APPEND initrd=initrd.gz root=/dev/ram0 rootfstype=ext2 rw ramdisk_size=${INITRD_RAM_SIZE_KB}
EOF

# ============================================================
# Check available floppy capacity
# ============================================================

echo
echo "Checking floppy capacity..."

RequireFile "$MOUNT_POINT/ldlinux.sys"

CheckFloppyCapacity

# ============================================================
# Copy boot files
# ============================================================

echo
echo "Copying kernel..."

CopyFloppyFile "$KERNEL" "$MOUNT_POINT/bzImage"

echo "Copying compressed initrd..."

CopyFloppyFile "$INITRD_COMPRESSED" "$MOUNT_POINT/initrd.gz"

echo "Copying SYSLINUX configuration..."

CopyFloppyFile "$FLOPPY_CONFIG" "$MOUNT_POINT/syslinux.cfg"

sync

# ============================================================
# Verify final floppy filesystem
# ============================================================

echo
echo "Verifying final floppy contents..."

for REQUIRED_FILE in \
    ldlinux.sys \
    syslinux.cfg \
    bzImage \
    initrd.gz
do
    RequireFile "$MOUNT_POINT/$REQUIRED_FILE"
done

gzip -t "$MOUNT_POINT/initrd.gz"

AVAILABLE_BYTES="$(GetAvailableBytes)"

echo
echo "Final floppy usage:"
df -k "$MOUNT_POINT"

if [ "$AVAILABLE_BYTES" -lt 8192 ]; then
    echo
    echo "WARNING: Less than 8 KiB remains on the floppy."
    PrintFloppySizeReport
fi

sync
umount "$MOUNT_POINT"
MOUNTED=0

# ============================================================
# Detach floppy loop device
# ============================================================

echo "Detaching floppy loop device..."

losetup -d "$LOOP_DEVICE"
LOOP_ATTACHED=0

# ============================================================
# Verify final image
# ============================================================

echo "Checking final floppy image..."

ACTUAL_SIZE="$(GetFileSize "$FLOPPY_IMAGE")"

if [ "$ACTUAL_SIZE" -ne "$FLOPPY_SIZE_BYTES" ]; then
    echo "ERROR: Final image has an incorrect size."
    exit 1
fi

BOOT_SIGNATURE="$(od -An -tx1 -j510 -N2 "$FLOPPY_IMAGE" | tr -d ' \n')"

if [ "$BOOT_SIGNATURE" != "55aa" ]; then
    echo "ERROR: Invalid boot sector signature: $BOOT_SIGNATURE"
    exit 1
fi

SUCCESS=1

# ============================================================
# Report
# ============================================================

KERNEL_SIZE="$(GetFileSize "$KERNEL")"
INITRD_SIZE="$(GetFileSize "$INITRD_COMPRESSED")"

echo
echo "============================================================"
echo "LVGD486 floppy image created successfully."
echo "============================================================"
echo
echo "Image:            $FLOPPY_IMAGE"
echo "Filesystem:       FAT12"
echo "Bootloader:       SYSLINUX 2.11-compatible configuration"
echo "Kernel:           bzImage"
echo "Kernel size:      $(FormatSize "$KERNEL_SIZE")"
echo "Initrd:           initrd.gz"
echo "Initrd size:      $(FormatSize "$INITRD_SIZE")"
echo "Initrd raw size:  ${INITRD_SIZE_MB} MiB"
echo "Initrd RAM size:  ${INITRD_RAM_SIZE_KB} KiB"
echo "Root:             /dev/ram0"
echo "Root filesystem:  ext2"
echo "Init:             /sbin/init"
echo "Shell:            BusyBox ash"
echo "Installer media:  /install-media"
echo
echo "Floppy contents:"
echo "  /ldlinux.sys"
echo "  /syslinux.cfg"
echo "  /bzImage"
echo "  /initrd.gz"
echo
echo "CD-ROM device mapping:"
echo "  /dev/hdc                 block 22:0"
echo "  /dev/cdroms/cdrom0       block 22:0"
echo "  /dev/cdrom               -> cdroms/cdrom0"
echo
echo "Initrd contents:"
echo "  /bin/busybox"
echo "  /bin/* (BusyBox symlinks)"
echo "  /dev/*"
echo "  /etc/profile"
echo "  /linuxrc"
echo "  /sbin/init"
echo "  /install-media"
echo "  /proc"
echo "  /tmp"
echo
echo "GRUB and e2fsprogs are provided by the mounted CD-ROM:"
echo "  /install-media/bin/grub"
echo "  /install-media/sbin/*"
echo
echo "Installer CD mounted at /install-media."
echo "Run the installer with:"
echo
echo "  /install-media/install.sh"
echo
echo "============================================================"