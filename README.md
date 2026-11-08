<p align="right">Last update: 2026-11-08</p>

# LVGD486

**[ LINUX VINTAGE GAME DEVELOPMENT DISTRO ]**

## For the 486 (and up!)

*Old hardware. New games. No unnecessary layers.*

---

### What if your old 486 could become a dedicated gaming console again?

**LVGD486 is a minimal Linux distribution designed to turn surviving 486 PCs and compatible machines into platforms for vintage-style game development and gaming.**

Inspired by the simplicity of DOS gaming, LVGD486 combines a lean Linux environment with access to hardware through the Linux kernel and the freedom to program VGA graphics directly. It is built for people who want to write games for real vintage computers, learn how the whole system works, and squeeze something fun out of hardware that modern software has long left behind.

No desktop environment to get in the way. No need to turn a vintage PC into something it was never meant to be. Just a focused system for building and running games.

**Make your 486 a gaming machine.**

### Get LVGD486

- [I want to make it myself](https://damianreloaded101.blogspot.com/p/lvgd486-from-sources-how-to.html)
- [Install Floppy](https://github.com/DamianReloaded/vhd/raw/refs/heads/main/lvgd486-installer-floppy.7z?download=)
- [Development Install CD](https://github.com/DamianReloaded/vhd/raw/refs/heads/main/lvgd486-installer-cd.7z?download=)
- [Development Install USB](https://github.com/DamianReloaded/vhd/raw/refs/heads/main/lvgd486-installer-usb.7z?download=)
- [Preinstalled VHD](https://github.com/DamianReloaded/vhd/raw/refs/heads/main/lvgd486dev.7z?download=)
- [Install Instructions](https://damianreloaded101.blogspot.com/p/lgvd486-install.html)

---

# [ INDEX ] :: Contents

1. [The Idea](#the-idea)
2. [Why the 486?](#why-the-486)
3. [Why Linux?](#why-linux)
4. [Small by Design](#small-by-design)
5. [Make Games, Not Operating Systems](#make-games-not-operating-systems)
6. [The VGD Game Library](#the-vgd-game-library)
7. [Build It Yourself](#build-it-yourself)
8. [Who Is LVGD486 For?](#who-is-lvgd486-for)
9. [Get Started](#get-started-with-lvgd486)
10. [Recommended Software](#recommended-software)
11. [Source Code and Build Components](#source-code-and-build-components)
12. [Build LVGD486 from Source](#build-lvgd486-from-source)

---

# The Idea

The 486 era was a fascinating time for PC gaming. Developers worked with limited memory, modest processing power, VGA graphics, and sound hardware that demanded careful attention. Those limitations shaped the games and programming techniques of an entire generation.

Today, plenty of those machines are still around. Some are waiting in attics, some are being restored, and others are already running their favorite DOS games. But vintage computers can be more than museum pieces or a way to replay old classics.

**>> What about creating new games for them?**

LVGD486 exists to make that idea practical. The goal is to provide a dedicated environment for games specifically developed or ported for the platform, while keeping the system small enough to respect the constraints of vintage hardware.

The target is the 486 and up, with the emphasis on understanding the machine, working close to the hardware, and creating games that belong on it.

---

# Why the 486?

The 486 occupies a fascinating place in the history of personal computing. It was a machine powerful enough to move beyond the earliest era of PC gaming, opening the door to smoother animation, more complex game logic, richer sound, and increasingly ambitious graphics.

What makes it especially interesting is how much it could accomplish with so little. A well-equipped 486 could deliver gaming experiences that felt remarkably close to those of dedicated 16-bit consoles, even though the underlying hardware worked in very different ways.

The Super Nintendo and the 486 were not equivalent machines. The SNES had specialized graphics and sound hardware designed specifically for games, while the PC offered a programmable processor, VGA graphics, and a wide variety of sound cards. Yet, with the right software and careful optimization, a 486 could reproduce surprisingly sophisticated 2D gameplay and, in some cases, run ports or emulators of games originally designed for 16-bit consoles.

**>> The 486 was more capable than its limitations might suggest.**

That is what makes the platform so compelling. It is old enough to expose the relationship between software and hardware, but capable enough to leave plenty of room for creativity. Performance is not something to take for granted. It is something to understand, earn, and optimize.

LVGD486 embraces that challenge. The goal is not simply to preserve old software or recreate the limitations of the past. It is to explore what can still be built on this hardware today, with a dedicated Linux-based environment for developing and running games on the 486 and compatible systems.

---

# Why Linux?

Why use Linux for something that is supposed to feel like DOS?

Because we want the simplicity of a dedicated gaming environment while taking advantage of the Linux kernel's device drivers and hardware support.

LVGD486 is designed to run in a single-user, single-task-oriented environment. It is not intended to be a general-purpose desktop distribution. Its job is to provide the foundation needed to run games and give developers access to the machine.

### Linux underneath, vintage gaming on top

The kernel handles supported hardware through its drivers, which are compiled directly into the kernel configuration used by the system. This keeps the setup straightforward and avoids relying on a collection of separately loaded driver modules.

At the same time, the environment allows games to program the VGA card directly where appropriate. Developers can work with video modes and graphics techniques without being forced into the abstractions of a modern desktop environment.

The idea is not to hide the hardware. It is to give you a practical way to work with it.

---

# Small by Design

Every byte of RAM matters when your computer has only a small amount of memory to begin with. The operating system should not consume resources that your game could use.

That is why LVGD486 uses Linux kernel 2.4. The choice is intentional: the project prioritizes a small memory footprint and a compact environment over running the latest kernel or supporting every modern feature.

The system is deliberately minimal. It focuses on the essentials instead of providing a full desktop, background services, and software that a dedicated gaming machine does not need.

This is part of the philosophy behind LVGD486: keep the operating environment lean, leave room for the game, and make the system understandable enough that you can learn from it.

### [!] LEGACY SOFTWARE NOTICE

Kernel 2.4 is obsolete and no longer receives normal upstream security maintenance. LVGD486 is intended for vintage computing, experimentation, and dedicated gaming systems, rather than as a secure, general-purpose computer for everyday internet use.

---

# Make Games, Not Operating Systems

Not everyone wants to start by configuring a kernel or assembling an entire Linux system. Sometimes you just want to write a game.

LVGD486 includes a game-development library intended to take care of some of the common tasks involved in building vintage-style games. Instead of implementing every piece of basic hardware interaction yourself, you can start with the facilities already provided and concentrate on gameplay.

### What can you work with?

- **[VIDEO] Video:** Set video modes and build your own graphics routines.
- **[KEYS] Keyboard:** Handle player controls and keyboard input.
- **[MOUSE] Mouse:** Add mouse-driven interfaces and gameplay.
- **[JOY] Joystick:** Support joystick input for games that benefit from it.
- **[SOUND] Sound:** Play digital audio.
- **[MIDI] MIDI:** Add MIDI music to your games.

These facilities give you a starting point for building the kinds of games that feel at home on vintage PCs: arcade games, platformers, shoot-'em-ups, puzzle games, adventures, strategy games, and experiments that would be difficult to justify on a modern machine but are perfect for a 486.

You can begin with a simple moving sprite and grow from there. Learn how to draw to the screen, respond to input, play sounds, and build your own game loop. Work within the machine's limits and discover how much you can do with the resources available.

The library is there to help you get started, not to take away your freedom. The platform is intentionally open to experimentation and customization.

---

# The VGD Game Library

An operating system is only part of the solution. To make game development practical, LVGD486 includes VGD, our own C++ library for building games directly on the platform.

VGD brings together graphics, input, and audio functionality behind a consistent programming interface. Instead of building these systems from scratch for every project, developers can focus on gameplay, game logic, and the creative work that makes each game unique.

**>> From pixels to sound, build your own games.**

### Graphics :: VGA Mode 13h

VGD provides a 320 × 200 graphics mode with 256 colors, using the classic VGA Mode 13h framebuffer. This is a familiar starting point for developing the kind of pixel-art games associated with the DOS era.

The display interface includes pixel drawing, filled rectangles, text rendering, framebuffer access, and frame presentation. A backbuffer lets games prepare a frame before displaying it, while a configurable frame-rate limiter helps control presentation timing.

Developers can work directly with the framebuffer when they need precise control over their rendering routines.

### Input :: Keyboard

The keyboard interface provides key-down and key-up events, character input, and convenient key-state queries. Games can check whether a key is held, newly pressed, or released, making it suitable for both continuous movement and event-driven controls.

This supports classic keyboard-driven gameplay, from arcade-style controls to menus, text entry, and more complex game interfaces.

### Input :: Mouse

VGD also provides mouse input, including movement deltas, left, right, and middle buttons, and button press and release events.

This opens the door to mouse-driven interfaces, point-and-click adventures, strategy games, and other genres that benefit from direct pointer control.

### Input :: Joystick and Gamepad

VGD supports joystick input through the Linux joystick device interface. It automatically searches for available devices under `/dev/input/js0` through `/dev/input/js15`, as well as the legacy `/dev/js0` through `/dev/js15` device paths.

The joystick interface provides horizontal and vertical axis values, button states, and information about the connected device. Games can use these inputs to implement analog movement, directional controls, and configurable gamepad buttons.

This makes VGD suitable for arcade-style games, platformers, racing games, and other projects where a joystick or compatible controller provides a more natural way to play.

Device availability depends on the controller, its Linux driver, and the input devices exposed by the system.

### Audio :: Sound Effects and Samples

The VGD audio system supports PCM audio output, configurable sample rates and channel counts, and callback-driven sample generation. Its audio mixer supports multiple simultaneous voices, allowing games to combine sound effects and other audio sources.

The mixer can play sampled audio, generate oscillators for simple synthesized tones, and produce noise-based effects. WAV loading support provides a way to bring recorded sounds into a game.

Together, these features provide the foundations for sound effects, simple synthesized audio, and mixed audio playback.

### Audio :: MIDI

VGD includes MIDI output support for sending musical messages to a MIDI device. The interface provides note-on and note-off messages, program changes, control changes, and commands to silence active notes.

This makes it possible to explore music playback through compatible MIDI hardware, including external synthesizers and sound modules, when the appropriate device is available.

### Designed for Game Development

VGD is designed around the needs of small, performance-conscious games. Its interfaces expose the underlying concepts clearly, giving developers room to write their own rendering, animation, input, and game-loop code.

The library does not try to be a heavyweight game engine. It provides building blocks that developers can understand, extend, and use to create games suited to the capabilities of vintage hardware.

**>> Your game. Your code. Your hardware.**

---

# Build It Yourself

One of the most important aspects of LVGD486 is that its sources are available and the system can be customized.

> **>> Our recommendation: build the distribution from source yourself.**

Not because everyone needs to become a Linux kernel developer, but because building a complete system for vintage hardware is a magnificent learning experience.

You get to see how the pieces fit together: the kernel, its drivers, the userspace environment, the development library, and the games that run on top of them.

You can inspect the configuration, choose what belongs in your system, modify components, and adapt the result to your own machine and experiments. You are not limited to treating the distribution as a mysterious black box.

Want to learn about kernel configuration? Interested in how a minimal Linux system boots? Curious about how software interacts with VGA hardware? LVGD486 gives you a practical project in which those questions have tangible answers.

And if your main interest is game development, you can start with the provided library and explore the operating system whenever you feel like going deeper.

---

# Who Is LVGD486 For?

- **Vintage PC enthusiasts** who want to give an old 486 a new purpose.
- **Game developers** who want to create new games for real vintage hardware.
- **Retrocomputing fans** who enjoy experimenting with the machines themselves, not just running old software.
- **Linux enthusiasts** who want to explore a minimal system built around an older kernel.
- **Low-level programmers** interested in VGA programming, input devices, sound, and hardware-oriented development.
- **Curious learners** who want to understand how an operating system is assembled from its source code.

You do not need to be an expert in every one of these fields. You can approach LVGD486 from the gaming side, the operating-system side, or simply from the desire to make an old computer do something new.

---

# Get Started with LVGD486

There are two ways to get started with LVGD486. You can build the system from source and learn how every component fits together, or you can use our prebuilt ISO images to get your hands on the system and start experimenting.

Whether you're a vintage PC enthusiast looking to revive an old 486, a programmer interested in VGA graphics and low-level development, or someone who wants to learn how to build a complete Linux system, there is a place to start.

### Two ISO images. Two purposes.

LVGD486 will provide two ISO images, each designed for a different purpose.

### [ ISO-01 ] :: LVGD486 Gaming System

This is the system intended to run on your vintage gaming machine. It provides the minimal Linux environment needed to run games developed or ported for LVGD486.

The goal is to turn a surviving 486 PC into a dedicated gaming console: boot the machine, launch your games, and enjoy software designed for the hardware.

**Use this image if you want to run LVGD486 games on real hardware or experiment with the system as a gaming platform.**

[DOWNLOAD GAMING ISO](REPLACE-WITH-GAMING-ISO-URL)

### [ ISO-02 ] :: LVGD486 Development Environment

The development environment is intended for programmers who want to build games, modify the distribution, or experiment with the underlying system.

It includes the tools needed to customize the kernel, rebuild the LVGD486 system, work on the game-development library, and compile software for the target platform.

This is the recommended starting point if you want to contribute to the project or learn how the entire system is assembled from source.

**Use this image if you want to develop games, customize LVGD486, or work on the operating system itself.**

[DOWNLOAD DEVELOPMENT ISO](https://github.com/DamianReloaded/lvgd486/raw/refs/heads/main/InstallMedia/lvgd486-installer-cd.7z?download=)

Both images will be accompanied by documentation to help you understand their purpose and get started.

---

# Recommended Software

You don't need a vintage PC to begin exploring LVGD486. Emulators and virtual machines let you experiment with the system, test your builds, and learn the development workflow before moving to real hardware.

For building the system from source, we also recommend a suitable 32-bit Linux development environment. The following tools provide a practical starting point.

### 86Box — Vintage PC emulation

[86BOX OFFICIAL WEBSITE](https://86box.net/)

86Box is particularly interesting for LVGD486 because it focuses on emulating vintage PC hardware, including systems from the 486 era. It is a natural choice when you want to test your distro in a machine that resembles the real hardware you are targeting.

Use it to experiment with virtual vintage PC configurations, boot installation media, and test your games without risking your original hardware.

For a project centered on the 486, hardware-oriented emulation is especially useful because compatibility and resource constraints are part of the development experience.

### VirtualBox — Convenient virtual machines

[VIRTUALBOX OFFICIAL WEBSITE](https://www.virtualbox.org/)

VirtualBox is a convenient option for running a separate Linux development environment on a modern computer. It is especially useful for setting up a Debian Sarge virtual machine in which you can build the kernel, BusyBox, GCC, and other components without changing your main operating system.

Its main role in the workflow is convenience: keep the legacy build environment isolated, take snapshots before making major changes, and return to a known working state when experimenting.

VirtualBox is not a cycle-accurate 486 emulator, so use it primarily for the development environment rather than assuming it reproduces the performance or hardware behavior of a real vintage PC.

### QEMU — Flexible machine emulation

[QEMU OFFICIAL WEBSITE](https://www.qemu.org/)

QEMU is another useful tool for testing operating systems and software in virtual machines. Its flexibility makes it suitable for experimenting with different machine configurations and automating parts of a development workflow.

Depending on the selected machine model and configuration, QEMU can help you test boot media, investigate system behavior, and run development builds before trying them on physical hardware.

As with any emulator, the results depend on the emulated hardware and configuration. Always test hardware-dependent behavior on the intended target when possible.

### Debian Sarge — A legacy build environment

Debian Sarge is recommended as a 32-bit development environment because it provides a historically appropriate userspace for working with the older toolchain and software used by LVGD486.

Building Linux kernel 2.4, BusyBox, and GCC in a dedicated 32-bit environment provides a straightforward starting point for reproducing the project's build process.

Using a separate virtual machine also keeps this legacy toolchain isolated from your modern development system.

[GET DEBIAN SARGE 32-BIT ISO](https://archive.org/details/debian-sarge-r8-32bit)

> **[!] SECURITY WARNING**
>
> Debian Sarge is an obsolete release and is no longer supported. Use it in an isolated virtual machine, preferably without unrestricted network access. Archived installation media and package repositories may require additional configuration.

---

# Source Code and Build Components

LVGD486 is intended to be understood, modified, and rebuilt. Rather than treating the operating system as a black box, you can work directly with the components that make it possible.

The following source archives are part of the proposed build workflow. Each component has a specific role in the system.

### Linux kernel 2.4.37.11

**[SRC]** [linux-2.4.37.11.tar.bz2](https://www.kernel.org/pub/linux/kernel/v2.4/linux-2.4.37.11.tar.bz2)

The Linux kernel provides the core operating system functionality and the device drivers used by LVGD486.

Version 2.4.37.11 is chosen to keep the system aligned with the project's minimal memory footprint and vintage hardware focus. The kernel is configured for the intended platform, with the required drivers compiled directly into the kernel where appropriate.

Working with the kernel source also lets you investigate its configuration and adapt the system to your own hardware and experiments.

### BusyBox 1.00

**[SRC]** [busybox-1.00.tar.bz2](https://busybox.net/downloads/busybox-1.00.tar.bz2)

BusyBox combines many common Unix command-line utilities into a single executable. It is well suited to minimal Linux systems where disk space and memory are limited.

For LVGD486, BusyBox helps provide the small userspace environment needed to boot the system, run essential commands, and launch the software without the overhead of a full desktop distribution.

### GNU Make 3.81

**[SRC]** [make-3.81.tar.gz](https://ftp.gnu.org/gnu/make/make-3.81.tar.gz)

GNU Make is used to automate compilation and other build tasks. The build workflow uses version 3.81 as part of the toolchain setup for building GCC 5.4.0.

Keeping the build tools and their versions documented makes it easier to reproduce the build environment and understand how the components fit together.

### GCC 5.4.0

**[SRC]** [gcc-5.4.0.tar.bz2](https://ftp.gnu.org/gnu/gcc/gcc-5.4.0/gcc-5.4.0.tar.bz2)

GCC is the compiler toolchain used for compiling software. Version 5.4.0 is selected to provide C++11 support and partial C++17 support for game development and other software built with the project's toolchain.

This makes it possible to use newer C++ language features while targeting a much older class of computer. The available language features do not imply that every modern C++ library or runtime facility will be available on the target; actual capabilities depend on the compiler configuration, target architecture, and supporting runtime libraries.

---

# Build LVGD486 from Source

One of the project's main goals is to make the complete process of creating a vintage gaming system understandable and reproducible.

We will provide a complete tutorial explaining how to build the installation media from source, starting with the development environment and moving through the individual components until you have a bootable LVGD486 system.

The tutorial will cover the build process for the kernel, the minimal userspace, the compiler toolchain, and the components required by the LVGD486 gaming environment.

The objective is not simply to tell you which commands to run. It is to help you understand why each component exists, how it is configured, and how the finished system comes together.

You will be able to use the resulting knowledge to customize the distribution, investigate the code, and adapt the system to your own development needs.

> **BUILD IT. STUDY IT. MAKE IT YOURS.**
>
> We encourage you to build it yourself. There is a lot to learn from putting an entire gaming operating system together.

[READ THE BUILD TUTORIAL](https://damianreloaded101.blogspot.com/p/lvgd486-from-sources-how-to.html)

---

# [INSTALL] :: Install LVGD486 to Disk

[Watch the video tutorial](https://www.youtube.com/watch?v=KM_DPYtLR3c)

Or download the [pre-installed VHD](https://github.com/DamianReloaded/vhd/raw/refs/heads/main/lvgd486dev.7z?download=).

This tutorial explains how to install LVGD486 from the bootable installation CD onto a hard disk. We will mount the installation media, identify the target disk, create a root partition and a swap partition, prepare both partitions, and run the installer.

> [!] **DATA LOSS WARNING**
>
> Partitioning and formatting can permanently destroy existing data. The commands below assume that `/dev/ide/host0/bus0/target0/lun0/disc` is the intended installation disk. Verify the device carefully before continuing. Do not proceed if you are unsure which disk you have selected.

## 00 > Boot up LVGD CD or Floppy Disk

*NOTE: Legacy PCs may not have a BIOS option for booting from a CD. In that case, you will need to boot from the LVGD floppy disk, let the system detect your CD-ROM drives, mount the CD, partition the disk, and run the installer. You will also need to do this when using an 86Box machine emulating a motherboard with an older BIOS.*

Insert the installation CD or floppy and boot from it. Once startup is complete, you should be presented with a bash prompt. The floppy is slow, so wait for it.

## 01 > Mount the Installation CD

First, mount the CD-ROM containing the LVGD486 installer. This is important because the `mke2fs` and `mkswap` utilities used to prepare the target partitions are provided by the installation media.

    mount /dev/cdroms/cdrom0 /install-media/

The installation files should now be accessible under `/install-media/`.

## 02 > Identify the Target Disk

List the available disks and partitions:

    fdisk -l

On this Linux 2.4-based system, an unpartitioned IDE disk may appear using the following device naming format:

    /dev/ide/host0/bus0/target0/lun0/disc

The `host0` component identifies an IDE host adapter. Depending on the hardware and driver configuration, additional disks may appear under other host, bus, target, or LUN numbers.

Partitions on this disk use the same device path, with `part1`, `part2`, and so on, at the end.

    /dev/ide/host0/bus0/target0/lun0/part1
    /dev/ide/host0/bus0/target0/lun0/part2

**>> Confirm the disk before partitioning.**

Make sure the selected device is the intended installation target, not a disk containing data you want to preserve.

## 03 > Create the Root and Swap Partitions

Start the partition editor for the target disk:

    fdisk /dev/ide/host0/bus0/target0/lun0/disc

We will create two primary partitions. The first will contain the LVGD486 root filesystem, while the second will be used for swap.

### Create the Root Partition

At the `fdisk` prompt, enter `n` to create a new partition:

    n

Select a primary partition:

    p

Choose partition number 1:

    1

When prompted for the first sector, press Enter to accept the default starting position:

    [Press Enter]

At the last sector prompt, specify the size of the root partition. For example, to create a 500 MB partition. Press Enter to select the default.

    +500M

If you have a 2 GB disk and you want to reserve around 100 MB for the swap partition, create the root partition starting at 1 (the default) and specify a size of `+2024M`.

    +2024M

Adjust this value according to the space required by your installation and the total capacity of the disk. Remember to leave enough unallocated space for the swap partition.

### Create the Swap Partition

The required swap size depends on the amount of physical RAM installed in the machine and the memory demands of the software you intend to run. A useful starting point for these memory-constrained systems is approximately twice the installed RAM.

For example, a machine with 8 MB of RAM could use approximately 16 MB of swap. This is a guideline, not a strict requirement. Choose a size that fits your hardware and available disk capacity.

On this particular disk, one cylinder represents approximately 4 MB. Therefore, 16 MB would require approximately 4 cylinders. Cylinder sizes vary depending on disk geometry, so these values should not be assumed to apply to every disk.

Enter `n` again to create the second partition:

    n

Select a primary partition:

    p

Choose partition number 2:

    2

Press Enter to accept the default starting sector:

    [Press Enter]

Specify the size of the swap partition. For example, to create 16 MB of swap, type `+16M`, or press Enter to use all the remaining available free space.

    +16M (or press [Enter])

### Set the Swap Partition Type

Change the partition type using the `t` command:

    t

Select partition 2:

    2

Enter `82`, the Linux swap partition type:

    82

Review the resulting partition layout by pressing `p` at the `fdisk` command prompt. Partition 1 should be the Linux root partition, and partition 2 should be marked as Linux swap.

If everything looks correct, write the changes to disk:

    w

This writes the new partition table to disk.

> [!] **PARTITION TABLE CHANGES**
>
> Writing the partition table can overwrite existing partition information. These steps are intended for a disk you are prepared to repartition. Stop if the displayed layout does not match your intentions.

## 04 > Verify the New Partitions

List the disks and partitions again:

    fdisk -l

You should now see two partitions:

    /dev/ide/host0/bus0/target0/lun0/part1
    /dev/ide/host0/bus0/target0/lun0/part2

Partition 1 will contain the root filesystem. Partition 2 will be used for swap. Verify their sizes and partition types before continuing.

## 05 > Format the Root Partition as ext2

LVGD486 uses the ext2 filesystem for its root partition. Create the filesystem on partition 1:

    mke2fs -F -r 0 -b 1024 /dev/ide/host0/bus0/target0/lun0/part1

The options specify a forced filesystem creation, revision 0 filesystem format, and a 1024-byte block size. These settings are intended for compatibility with the legacy target system.

> [!] **FORMATTING ERASES DATA**
>
> This command creates a new filesystem on partition 1 and destroys existing filesystem data there. Double-check the device path before running it.

### Initialize the Swap Partition

Initialize partition 2 as swap space:

    mkswap /dev/ide/host0/bus0/target0/lun0/part2

This prepares the partition for use as virtual memory. It does not activate swap immediately; we will configure the installed system to enable it automatically during startup.

## 06 > Run the LVGD486 Installer

Change to the directory containing the installer:

    cd /install-media

Run the installation script and pass the root partition as its argument, and optionally the word `lba` as the second parameter if you're installing this on an old PC or an 86Box VM. VirtualBox works without passing `lba`.

    ./install.sh /dev/ide/host0/bus0/target0/lun0/part1 [lba]

The installer will copy the LVGD486 system to partition 1. The swap partition is not passed to the installer because it is initialized separately and enabled through the startup script.

On slower 486-class machines, this process may take some time. Allow the installer to finish and check for any reported errors.

**>> Be patient. Older hardware takes time.**

### 06.1 > Users

    user: root
    password: root

    user: game
    password: game

You can become root by issuing:

    su root
    Password: root

In this humble game console style distro, the idea of running games with a less privileged user `game` is mainly to prevent accidental modification of system files.

## 07 > Configure Swap at Startup

LVGD486 uses a dedicated swap partition to provide additional virtual memory when physical RAM is insufficient. To enable it automatically, add the following command to the installed system's **`/etc/rcS`** startup script:

    swapon /dev/ide/host0/bus0/target0/lun0/part2

Place this command during system initialization, before services or applications that may need additional memory are started.

The command activates the swap partition each time LVGD486 boots. The kernel can then use it when additional memory is needed.

**>> Swap is now enabled automatically at boot.**

## 08 > Reboot into LVGD486

Once the installer has completed successfully and the startup script has been configured, remove the installation CD and reboot the machine.

If the hard disk is bootable, the bootloader has been installed correctly, and the BIOS is configured to boot from the hard disk, the system should start LVGD486 directly from the installed disk.

You can convert the VDI hard disk you created in VirtualBox to VHD using `qemu-img.exe`:

    .\qemu-img.exe convert -f vdi -O vpc disk.vdi disk.vhd

Then you can attach it to an 86Box virtual machine if you want to emulate the 486 hardware accurately. Remember to auto-detect your hard disk in the BIOS inside the running virtual machine.

In 86Box, remember to set the sound card to **[ISA 16] Sound Blaster 16 PnP**. Then go to the MIDI tab and set MIDI Out Device to **FluidSynth**, press the **Configure** button, and set a nice SoundFont for your MIDI emulation.

### 08.1 > GameLauncher

Right after the system boots up, the GameLauncher will run. You can quit it by pressing ESCAPE. You can configure the game list by editing the file:

    /games/lvgd/games.lst

**>> Installation complete.**

Your LVGD486 system is now ready to boot from the hard disk, with its root filesystem and swap space configured. Welcome to low-level gaming on vintage hardware.