
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <sys/sysmacros.h>

#include <linux/input.h>

static FILE* g_Log = NULL;

static void Log(const char* format, ...)
{
    va_list args;
    va_list logArgs;

    va_start(args, format);
    va_copy(logArgs, args);

    vprintf(format, args);
    fflush(stdout);

    if (g_Log != NULL)
    {
        vfprintf(g_Log, format, logArgs);
        fflush(g_Log);
    }

    va_end(logArgs);
    va_end(args);
}

static void PrintError(const char* operation)
{
    int errorNumber = errno;

    Log("%s failed: errno=%d (%s)\n",
        operation,
        errorNumber,
        strerror(errorNumber));
}

static int IsBitSet(
    const unsigned long* bits,
    unsigned int bit)
{
    const unsigned int bitsPerLong =
        sizeof(unsigned long) * 8;

    return (bits[bit / bitsPerLong] &
            (1UL << (bit % bitsPerLong))) != 0;
}

static void DumpFile(const char* path)
{
    FILE* file = fopen(path, "r");

    if (file == NULL)
    {
        Log("\n--- %s ---\n", path);
        Log("Cannot open: %s\n", strerror(errno));
        return;
    }

    Log("\n--- %s ---\n", path);

    char buffer[256];

    while (fgets(buffer, sizeof(buffer), file) != NULL)
        Log("%s", buffer);

    fclose(file);
}

static void DumpDeviceNode(const char* path)
{
    struct stat st;

    Log("\nPATH CHECK: %s\n", path);

    if (lstat(path, &st) < 0)
    {
        Log("  Does not exist: %s\n", strerror(errno));
        return;
    }

    if (S_ISCHR(st.st_mode))
    {
        Log("  Character device: major=%u minor=%u\n",
            (unsigned int)major(st.st_rdev),
            (unsigned int)minor(st.st_rdev));
    }
    else if (S_ISDIR(st.st_mode))
    {
        Log("  Directory\n");
    }
    else if (S_ISLNK(st.st_mode))
    {
        Log("  Symbolic link\n");
    }
    else
    {
        Log("  Exists, but is not a character device.\n");
    }

    Log("  Permissions: %04o\n",
        (unsigned int)(st.st_mode & 07777));
}

static void PrintDeviceInformation(
    const char* path,
    int fd)
{
    struct stat st;

    Log("\nDevice: %s\n", path);
    Log("File descriptor: %d\n", fd);

    if (fstat(fd, &st) < 0)
    {
        PrintError("fstat");
    }
    else
    {
        if (S_ISCHR(st.st_mode))
        {
            Log("File type: character device\n");
            Log("Device number: major=%u minor=%u\n",
                (unsigned int)major(st.st_rdev),
                (unsigned int)minor(st.st_rdev));
        }
        else if (S_ISREG(st.st_mode))
        {
            Log("File type: regular file\n");
        }
        else
        {
            Log("File type: other\n");
        }

        Log("Permissions: %04o\n",
            (unsigned int)(st.st_mode & 07777));
    }

    Log("isatty: %s\n", isatty(fd) ? "yes" : "no");
}

/*
 * Probe an input device's event capabilities.
 *
 * This uses the Linux input ioctl interface available in
 * Linux 2.4 kernels configured with the input subsystem.
 */
static int GetEventCapabilities(
    int fd,
    int* hasRelative,
    int* hasAbsolute,
    int* hasButtons,
    int* hasMouseKeys)
{
    unsigned long eventBits[8];

    memset(eventBits, 0, sizeof(eventBits));

    *hasRelative = 0;
    *hasAbsolute = 0;
    *hasButtons = 0;
    *hasMouseKeys = 0;

    if (ioctl(
            fd,
            EVIOCGBIT(0, sizeof(eventBits)),
            eventBits) < 0)
    {
        return 0;
    }

    *hasRelative = IsBitSet(eventBits, EV_REL);
    *hasAbsolute = IsBitSet(eventBits, EV_ABS);
    *hasButtons = IsBitSet(eventBits, EV_KEY);

    if (*hasButtons)
    {
        unsigned long keyBits[128];

        memset(keyBits, 0, sizeof(keyBits));

        if (ioctl(
                fd,
                EVIOCGBIT(EV_KEY, sizeof(keyBits)),
                keyBits) >= 0)
        {
            if (IsBitSet(keyBits, BTN_LEFT) ||
                IsBitSet(keyBits, BTN_RIGHT) ||
                IsBitSet(keyBits, BTN_MIDDLE) ||
                IsBitSet(keyBits, BTN_MOUSE))
            {
                *hasMouseKeys = 1;
            }
        }
    }

    return 1;
}

static int IsLikelyMouse(
    int hasRelative,
    int hasAbsolute,
    int hasMouseKeys)
{
    /*
     * Relative movement plus mouse buttons is the usual mouse
     * configuration. Absolute movement plus mouse buttons can
     * indicate a touch device or tablet.
     */
    if (hasRelative && hasMouseKeys)
        return 1;

    if (hasAbsolute && hasMouseKeys)
        return 1;

    return 0;
}

/*
 * Test a raw PS/2-style byte stream for five seconds.
 *
 * This does not assume every received byte is valid mouse data.
 * It prints the raw bytes and also counts plausible three-byte
 * packets for devices that use the standard PS/2 packet format.
 */
static void TestRawMouseDevice(
    const char* path,
    int durationSeconds)
{
    Log("\n");
    Log("====================================================\n");
    Log("RAW MOUSE TEST: %s\n", path);
    Log("Move the mouse and press its buttons.\n");
    Log("Test duration: %d seconds.\n", durationSeconds);
    Log("====================================================\n");

    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
    {
        PrintError(path);
        Log("Cannot run live test on this device.\n");
        return;
    }

    PrintDeviceInformation(path, fd);

    unsigned long totalBytes = 0;
    unsigned long plausiblePackets = 0;
    unsigned long packetErrors = 0;

    unsigned char packet[3];
    int packetCount = 0;

    struct timeval start;
    gettimeofday(&start, NULL);

    for (;;)
    {
        struct timeval now;

        gettimeofday(&now, NULL);

        long elapsed =
            (now.tv_sec - start.tv_sec) * 1000000L +
            (now.tv_usec - start.tv_usec);

        long remaining =
            (long)durationSeconds * 1000000L - elapsed;

        if (remaining <= 0)
            break;

        struct timeval timeout;

        timeout.tv_sec = remaining / 1000000L;
        timeout.tv_usec = remaining % 1000000L;

        fd_set readSet;

        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);

        int result = select(
            fd + 1,
            &readSet,
            NULL,
            NULL,
            &timeout);

        if (result < 0)
        {
            if (errno == EINTR)
                continue;

            PrintError("select");
            break;
        }

        if (result == 0)
            continue;

        unsigned char buffer[64];

        ssize_t bytes = read(fd, buffer, sizeof(buffer));

        if (bytes < 0)
        {
            if (errno == EINTR ||
                errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                continue;
            }

            PrintError("read mouse device");
            break;
        }

        if (bytes == 0)
        {
            /*
             * Nonblocking device reads can return zero when no
             * data is currently available. Continue until timeout.
             */
            continue;
        }

        totalBytes += (unsigned long)bytes;

        Log("RAW:");

        ssize_t i;

        for (i = 0; i < bytes; ++i)
            Log(" %02x", (unsigned int)buffer[i]);

        Log("\n");

        for (i = 0; i < bytes; ++i)
        {
            unsigned char byte = buffer[i];

            /*
             * Standard PS/2 packet headers have bit 3 set.
             * Use this to attempt packet synchronization.
             */
            if (packetCount == 0)
            {
                if ((byte & 0x08) == 0)
                {
                    ++packetErrors;
                    continue;
                }

                packet[packetCount++] = byte;
                continue;
            }

            packet[packetCount++] = byte;

            if (packetCount == 3)
            {
                int dx = (packet[1] & 0x80) ?
                    (int)packet[1] - 256 :
                    (int)packet[1];

                int dy = (packet[2] & 0x80) ?
                    (int)packet[2] - 256 :
                    (int)packet[2];

                Log("PS/2 candidate: "
                    "flags=0x%02x dx=%d dy=%d "
                    "left=%u right=%u middle=%u\n",
                    (unsigned int)packet[0],
                    dx,
                    dy,
                    (unsigned int)(packet[0] & 0x01),
                    (unsigned int)((packet[0] >> 1) & 0x01),
                    (unsigned int)((packet[0] >> 2) & 0x01));

                ++plausiblePackets;
                packetCount = 0;
            }
        }
    }

    Log("\nRAW TEST SUMMARY: %s\n", path);
    Log("Bytes received: %lu\n", totalBytes);
    Log("Plausible three-byte packet candidates: %lu\n",
        plausiblePackets);
    Log("Bytes rejected while seeking packet alignment: %lu\n",
        packetErrors);

    if (totalBytes == 0)
    {
        Log("RESULT: no bytes received during the test.\n");
        Log("This does not by itself prove that the hardware is faulty.\n");
    }
    else if (plausiblePackets == 0)
    {
        Log("RESULT: bytes arrived, but no complete PS/2-style "
            "packets were decoded.\n");
        Log("The stream may use another protocol or may not be "
            "mouse packet data.\n");
    }
    else
    {
        Log("RESULT: bytes and plausible PS/2-style packets received.\n");
    }

    close(fd);
}

/*
 * Test an evdev node for five seconds.
 *
 * Only devices advertising mouse buttons and relative or absolute
 * movement are treated as likely mouse devices.
 */
static void TestEventMouseDevice(const char* path)
{
    Log("\n");
    Log("====================================================\n");
    Log("EVDEV MOUSE CHECK: %s\n", path);
    Log("====================================================\n");

    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
    {
        PrintError(path);
        return;
    }

    PrintDeviceInformation(path, fd);

    int version = 0;

    if (ioctl(fd, EVIOCGVERSION, &version) == 0)
    {
        Log("EVIOCGVERSION: %d.%d.%d\n",
            (version >> 16) & 0xff,
            (version >> 8) & 0xff,
            version & 0xff);
    }
    else
    {
        PrintError("EVIOCGVERSION");
    }

    int hasRelative;
    int hasAbsolute;
    int hasButtons;
    int hasMouseKeys;

    if (!GetEventCapabilities(
            fd,
            &hasRelative,
            &hasAbsolute,
            &hasButtons,
            &hasMouseKeys))
    {
        PrintError("EVIOCGBIT");
        Log("This node does not expose the expected evdev capability interface.\n");
        close(fd);
        return;
    }

    Log("EV_REL support: %s\n",
        hasRelative ? "yes" : "no");
    Log("EV_ABS support: %s\n",
        hasAbsolute ? "yes" : "no");
    Log("EV_KEY support: %s\n",
        hasButtons ? "yes" : "no");
    Log("Mouse button codes detected: %s\n",
        hasMouseKeys ? "yes" : "no");

    if (!IsLikelyMouse(
            hasRelative,
            hasAbsolute,
            hasMouseKeys))
    {
        Log("RESULT: this node does not advertise a typical mouse "
            "capability combination; live test skipped.\n");
        close(fd);
        return;
    }

    Log("\nMove the mouse and press its buttons for five seconds.\n");

    struct timeval start;
    gettimeofday(&start, NULL);

    unsigned long totalEvents = 0;
    unsigned long relativeEvents = 0;
    unsigned long buttonEvents = 0;
    unsigned long otherEvents = 0;

    for (;;)
    {
        struct timeval now;
        gettimeofday(&now, NULL);

        long elapsed =
            (now.tv_sec - start.tv_sec) * 1000000L +
            (now.tv_usec - start.tv_usec);

        long remaining = 5000000L - elapsed;

        if (remaining <= 0)
            break;

        struct timeval timeout;

        timeout.tv_sec = remaining / 1000000L;
        timeout.tv_usec = remaining % 1000000L;

        fd_set readSet;

        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);

        int result = select(
            fd + 1,
            &readSet,
            NULL,
            NULL,
            &timeout);

        if (result < 0)
        {
            if (errno == EINTR)
                continue;

            PrintError("select evdev");
            break;
        }

        if (result == 0)
            continue;

        struct input_event events[16];

        ssize_t bytes = read(
            fd,
            events,
            sizeof(events));

        if (bytes < 0)
        {
            if (errno == EINTR ||
                errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                continue;
            }

            PrintError("read evdev");
            break;
        }

        if (bytes == 0)
            continue;

        if (bytes % sizeof(struct input_event) != 0)
        {
            Log("WARNING: read %ld bytes, not a whole number "
                "of input_event records.\n",
                (long)bytes);
        }

        size_t count =
            (size_t)bytes / sizeof(struct input_event);

        size_t i;

        for (i = 0; i < count; ++i)
        {
            const struct input_event* event = &events[i];

            Log("EVENT: time=%ld.%06ld type=%u code=%u value=%d",
                (long)event->time.tv_sec,
                (long)event->time.tv_usec,
                (unsigned int)event->type,
                (unsigned int)event->code,
                event->value);

            if (event->type == EV_REL)
            {
                if (event->code == REL_X)
                    Log("  [REL_X]");
                else if (event->code == REL_Y)
                    Log("  [REL_Y]");

                ++relativeEvents;
            }
            else if (event->type == EV_ABS)
            {
                if (event->code == ABS_X)
                    Log("  [ABS_X]");
                else if (event->code == ABS_Y)
                    Log("  [ABS_Y]");

                ++otherEvents;
            }
            else if (event->type == EV_KEY)
            {
                if (event->code == BTN_LEFT)
                    Log("  [BTN_LEFT]");
                else if (event->code == BTN_RIGHT)
                    Log("  [BTN_RIGHT]");
                else if (event->code == BTN_MIDDLE)
                    Log("  [BTN_MIDDLE]");

                ++buttonEvents;
            }
            else
            {
                ++otherEvents;
            }

            Log("\n");
            ++totalEvents;
        }
    }

    Log("\nEVDEV TEST SUMMARY: %s\n", path);
    Log("Total events: %lu\n", totalEvents);
    Log("Relative movement events: %lu\n", relativeEvents);
    Log("Button events: %lu\n", buttonEvents);
    Log("Other events: %lu\n", otherEvents);

    if (totalEvents == 0)
        Log("RESULT: no events received during the test.\n");
    else
        Log("RESULT: input events received.\n");

    close(fd);
}

static void CheckRawDevice(const char* path)
{
    Log("\n");
    Log("####################################################\n");
    Log("RAW DEVICE CHECK: %s\n", path);
    Log("####################################################\n");

    struct stat st;

    if (stat(path, &st) < 0)
    {
        Log("Device does not exist: %s\n", strerror(errno));
        return;
    }

    if (!S_ISCHR(st.st_mode))
    {
        Log("Path exists but is not a character device.\n");
        return;
    }

    Log("Character device: major=%u minor=%u\n",
        (unsigned int)major(st.st_rdev),
        (unsigned int)minor(st.st_rdev));

    Log("Permissions: %04o\n",
        (unsigned int)(st.st_mode & 07777));

    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
    {
        PrintError("open");
        Log("Live test skipped because open failed.\n");
        return;
    }

    Log("Open succeeded.\n");
    close(fd);

    TestRawMouseDevice(path, 5);
}

static void CheckEventDevice(const char* path)
{
    struct stat st;

    if (stat(path, &st) < 0)
    {
        Log("\n%s: does not exist (%s)\n",
            path,
            strerror(errno));
        return;
    }

    TestEventMouseDevice(path);
}

int main(void)
{
    g_Log = fopen("mouse-log.txt", "w");

    if (g_Log == NULL)
    {
        fprintf(stderr,
                "Cannot create mouse-log.txt: %s\n",
                strerror(errno));
        return 1;
    }

    Log("====================================================\n");
    Log("Linux Mouse Diagnostic\n");
    Log("====================================================\n");
    Log("Log file: mouse-log.txt\n");
    Log("This program does not create device nodes.\n");
    Log("Raw-device tests read from each device for five seconds.\n");
    Log("Move the mouse and click buttons during each live test.\n");
    Log("Only use this diagnostic while the desktop or mouse "
        "application is not relying on the same device.\n");

    struct utsname systemInfo;

    if (uname(&systemInfo) == 0)
    {
        Log("\nSystem information:\n");
        Log("  sysname:  %s\n", systemInfo.sysname);
        Log("  nodename: %s\n", systemInfo.nodename);
        Log("  release:  %s\n", systemInfo.release);
        Log("  version:  %s\n", systemInfo.version);
        Log("  machine:  %s\n", systemInfo.machine);
    }

    DumpFile("/proc/version");
    DumpFile("/proc/cmdline");
    DumpFile("/proc/interrupts");
    DumpFile("/proc/devices");
    DumpFile("/proc/modules");
    DumpFile("/proc/mounts");
    DumpFile("/proc/bus/input/devices");

    Log("\n\n====================================================\n");
    Log("PHASE 1: MOUSE DEVICE NODES\n");
    Log("====================================================\n");

    DumpDeviceNode("/dev/input");
    DumpDeviceNode("/dev/input/mice");
    DumpDeviceNode("/dev/input/mouse0");
    DumpDeviceNode("/dev/input/mouse1");
    DumpDeviceNode("/dev/psaux");
    DumpDeviceNode("/dev/misc/psaux");
    DumpDeviceNode("/dev/usbmouse");

    Log("\n\n====================================================\n");
    Log("PHASE 2: RAW MOUSE INTERFACES\n");
    Log("====================================================\n");

    /*
     * Each device is tested independently. A successful open
     * only proves that the node can be opened; the live test
     * determines whether data arrives.
     */
    CheckRawDevice("/dev/input/mice");
    CheckRawDevice("/dev/input/mouse0");
    CheckRawDevice("/dev/input/mouse1");
    CheckRawDevice("/dev/psaux");
    CheckRawDevice("/dev/misc/psaux");
    CheckRawDevice("/dev/usbmouse");

    Log("\n\n====================================================\n");
    Log("PHASE 3: EVDEV MOUSE INTERFACES\n");
    Log("====================================================\n");
    Log("Checking event0 through event31.\n");
    Log("Mouse-like event devices will receive a five-second test.\n");

    int i;

    for (i = 0; i < 32; ++i)
    {
        char path[64];

        snprintf(
            path,
            sizeof(path),
            "/dev/input/event%d",
            i);

        CheckEventDevice(path);
    }

    Log("\n\n====================================================\n");
    Log("DIAGNOSTIC COMPLETE\n");
    Log("====================================================\n");
    Log("Please inspect or send back mouse-log.txt.\n");
    Log("Important evidence: device major/minor numbers, "
        "kernel configuration, open errors, raw byte counts, "
        "and evdev event counts.\n");

    fclose(g_Log);
    g_Log = NULL;

    return 0;
}
