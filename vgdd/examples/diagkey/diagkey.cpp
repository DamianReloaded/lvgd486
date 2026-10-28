
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <sys/sysmacros.h>

#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>

static FILE* g_Log = NULL;

static volatile sig_atomic_t g_KeyboardModeChanged = 0;
static volatile sig_atomic_t g_TermiosChanged = 0;

static int g_KeyboardModeFd = -1;
static int g_OriginalKeyboardMode = 0;

static int g_TermiosFd = -1;
static struct termios g_OriginalTermios;

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

static void RestoreSettings()
{
    if (g_KeyboardModeChanged && g_KeyboardModeFd >= 0)
    {
        if (ioctl(
                g_KeyboardModeFd,
                KDSKBMODE,
                g_OriginalKeyboardMode) < 0)
        {
            /* Avoid logging from signal-handler cleanup. */
        }

        g_KeyboardModeChanged = 0;
    }

    if (g_TermiosChanged && g_TermiosFd >= 0)
    {
        tcsetattr(
            g_TermiosFd,
            TCSANOW,
            &g_OriginalTermios);

        g_TermiosChanged = 0;
    }
}

static void HandleSignal(int signalNumber)
{
    RestoreSettings();
    _exit(128 + signalNumber);
}

static void InstallSignalHandlers()
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));

    action.sa_handler = HandleSignal;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
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
        Log("File type: ");

        if (S_ISCHR(st.st_mode))
            Log("character device\n");
        else if (S_ISBLK(st.st_mode))
            Log("block device\n");
        else if (S_ISREG(st.st_mode))
            Log("regular file\n");
        else if (S_ISFIFO(st.st_mode))
            Log("FIFO\n");
        else if (S_ISSOCK(st.st_mode))
            Log("socket\n");
        else
            Log("other\n");

        if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
        {
            Log("Device number: major=%u minor=%u\n",
                (unsigned int)major(st.st_rdev),
                (unsigned int)minor(st.st_rdev));
        }

        Log("Permissions: %04o\n",
            (unsigned int)(st.st_mode & 07777));
    }

    Log("isatty: %s\n", isatty(fd) ? "yes" : "no");

    const char* name = ttyname(fd);

    if (name != NULL)
        Log("ttyname: %s\n", name);
    else
        Log("ttyname: unavailable (%s)\n", strerror(errno));

    struct termios term;

    if (tcgetattr(fd, &term) == 0)
    {
        Log("tcgetattr: succeeded\n");
        Log("  c_iflag = 0x%lx\n",
            (unsigned long)term.c_iflag);
        Log("  c_oflag = 0x%lx\n",
            (unsigned long)term.c_oflag);
        Log("  c_cflag = 0x%lx\n",
            (unsigned long)term.c_cflag);
        Log("  c_lflag = 0x%lx\n",
            (unsigned long)term.c_lflag);
        Log("  VMIN    = %u\n",
            (unsigned int)term.c_cc[VMIN]);
        Log("  VTIME   = %u\n",
            (unsigned int)term.c_cc[VTIME]);
    }
    else
    {
        PrintError("tcgetattr");
    }

    struct vt_mode vtMode;

    memset(&vtMode, 0, sizeof(vtMode));

    if (ioctl(fd, VT_GETMODE, &vtMode) == 0)
    {
        Log("VT_GETMODE: succeeded\n");
        Log("  mode=%u waitv=%u relsig=%u acqsig=%u frsig=%u\n",
            (unsigned int)vtMode.mode,
            (unsigned int)vtMode.waitv,
            (unsigned int)vtMode.relsig,
            (unsigned int)vtMode.acqsig,
            (unsigned int)vtMode.frsig);
    }
    else
    {
        PrintError("VT_GETMODE");
    }

    struct vt_stat vtState;

    memset(&vtState, 0, sizeof(vtState));

    if (ioctl(fd, VT_GETSTATE, &vtState) == 0)
    {
        Log("VT_GETSTATE: succeeded\n");
        Log("  active VT=%u\n",
            (unsigned int)vtState.v_active);
        Log("  signal=%u\n",
            (unsigned int)vtState.v_signal);
        Log("  open VT mask=0x%x\n",
            (unsigned int)vtState.v_state);
    }
    else
    {
        PrintError("VT_GETSTATE");
    }

    int keyboardMode = -1;

    if (ioctl(fd, KDGKBMODE, &keyboardMode) == 0)
    {
        Log("KDGKBMODE: succeeded; current mode=%d\n",
            keyboardMode);
    }
    else
    {
        PrintError("KDGKBMODE");
    }
}

/*
 * Read input for five seconds.
 *
 * Event devices are decoded as struct input_event.
 * Console medium-raw input is printed as hexadecimal bytes.
 */
static void ReadForFiveSeconds(
    int fd,
    const char* path,
    int eventDevice)
{
    Log("\n");
    Log("====================================================\n");
    Log("LIVE INPUT TEST: %s\n", path);
    Log("Press keys now. Test duration: 5 seconds.\n");
    Log("====================================================\n");
    Log("Start typing now.\n");

    struct timeval start;
    gettimeofday(&start, NULL);

    unsigned long totalBytes = 0;
    unsigned long totalEvents = 0;

    for (;;)
    {
        struct timeval now;
        gettimeofday(&now, NULL);

        long elapsedMicroseconds =
            (now.tv_sec - start.tv_sec) * 1000000L +
            (now.tv_usec - start.tv_usec);

        long remainingMicroseconds =
            5000000L - elapsedMicroseconds;

        if (remainingMicroseconds <= 0)
            break;

        struct timeval timeout;

        timeout.tv_sec = remainingMicroseconds / 1000000L;
        timeout.tv_usec = remainingMicroseconds % 1000000L;

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

        if (eventDevice)
        {
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
                    continue;

                PrintError("read event device");
                break;
            }

            if (bytes == 0)
            {
                Log("Event device returned EOF.\n");
                break;
            }

            totalBytes += (unsigned long)bytes;

            if (bytes % sizeof(struct input_event) != 0)
            {
                Log("WARNING: read %ld bytes, not a whole "
                    "number of input_event records.\n",
                    (long)bytes);
            }

            size_t count =
                (size_t)bytes / sizeof(struct input_event);

            size_t i;

            for (i = 0; i < count; ++i)
            {
                Log("EVENT: time=%ld.%06ld type=%u "
                    "code=%u value=%d\n",
                    (long)events[i].time.tv_sec,
                    (long)events[i].time.tv_usec,
                    (unsigned int)events[i].type,
                    (unsigned int)events[i].code,
                    events[i].value);

                ++totalEvents;
            }
        }
        else
        {
            unsigned char buffer[64];

            ssize_t bytes = read(
                fd,
                buffer,
                sizeof(buffer));

            if (bytes < 0)
            {
                if (errno == EINTR ||
                    errno == EAGAIN ||
                    errno == EWOULDBLOCK)
                    continue;

                PrintError("read console");
                break;
            }

            if (bytes == 0)
                continue;

            totalBytes += (unsigned long)bytes;

            ssize_t i;

            Log("RAW:");

            for (i = 0; i < bytes; ++i)
                Log(" %02x", (unsigned int)buffer[i]);

            Log("\n");
        }
    }

    Log("Live test finished for %s\n", path);
    Log("Total bytes read: %lu\n", totalBytes);
    Log("Total input events: %lu\n", totalEvents);

    if (totalBytes == 0)
        Log("RESULT: no input was received during this test.\n");
    else
        Log("RESULT: input was received.\n");
}

static void TestMediumRaw(
    int fd,
    const char* path,
    int originalMode)
{
    Log("\nAttempting KDSKBMODE K_MEDIUMRAW on %s\n", path);

    if (ioctl(fd, KDSKBMODE, K_MEDIUMRAW) < 0)
    {
        PrintError("KDSKBMODE K_MEDIUMRAW");
        return;
    }

    g_KeyboardModeFd = fd;
    g_OriginalKeyboardMode = originalMode;
    g_KeyboardModeChanged = 1;

    Log("KDSKBMODE K_MEDIUMRAW succeeded.\n");

    ReadForFiveSeconds(fd, path, 0);

    if (ioctl(
            fd,
            KDSKBMODE,
            originalMode) < 0)
    {
        PrintError("restore KDSKBMODE");
    }
    else
    {
        Log("Original keyboard mode restored: %d\n",
            originalMode);
        g_KeyboardModeChanged = 0;
    }

    g_KeyboardModeFd = -1;
}

static void TestTermiosInput(
    int fd,
    const char* path)
{
    struct termios original;

    if (tcgetattr(fd, &original) < 0)
    {
        PrintError("tcgetattr before terminal test");
        return;
    }

    struct termios raw = original;

    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
    {
        PrintError("tcsetattr raw terminal test");
        return;
    }

    g_TermiosFd = fd;
    g_OriginalTermios = original;
    g_TermiosChanged = 1;

    Log("Terminal set to noncanonical, no-echo mode.\n");

    ReadForFiveSeconds(fd, path, 0);

    if (tcsetattr(fd, TCSANOW, &original) < 0)
    {
        PrintError("restore terminal settings");
    }
    else
    {
        Log("Original terminal settings restored.\n");
        g_TermiosChanged = 0;
    }

    g_TermiosFd = -1;
}

static void TestConsoleDevice(const char* path)
{
    Log("\n\n####################################################\n");
    Log("CONSOLE DEVICE CHECK: %s\n", path);
    Log("####################################################\n");

    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
    {
        PrintError(path);
        Log("Device cannot be opened; live test skipped.\n");
        return;
    }

    PrintDeviceInformation(path, fd);

    int keyboardMode = -1;

    if (ioctl(fd, KDGKBMODE, &keyboardMode) == 0)
    {
        Log("\nKeyboard-mode ioctl is supported.\n");
        Log("Original keyboard mode: %d\n", keyboardMode);

        TestMediumRaw(fd, path, keyboardMode);
    }
    else
    {
        Log("\nKDGKBMODE is not supported on this descriptor.\n");
        Log("Trying ordinary terminal input if termios works.\n");

        struct termios term;

        if (tcgetattr(fd, &term) == 0)
            TestTermiosInput(fd, path);
        else
            Log("Neither keyboard-mode ioctl nor termios "
                "input is available on this descriptor.\n");
    }

    close(fd);
}

static void TestEventDevice(const char* path)
{
    Log("\n\n####################################################\n");
    Log("EVDEV DEVICE CHECK: %s\n", path);
    Log("####################################################\n");

    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
    {
        PrintError(path);
        Log("Event device cannot be opened.\n");
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

    unsigned long eventBits[8];
    memset(eventBits, 0, sizeof(eventBits));

    if (ioctl(
            fd,
            EVIOCGBIT(0, sizeof(eventBits)),
            eventBits) < 0)
    {
        PrintError("EVIOCGBIT event types");
        Log("Cannot verify that this is a usable evdev device.\n");
        close(fd);
        return;
    }

    Log("EVIOCGBIT event types: succeeded.\n");

    int hasKeys = IsBitSet(eventBits, EV_KEY);

    Log("Supports EV_KEY: %s\n",
        hasKeys ? "yes" : "no");

    if (!hasKeys)
    {
        Log("Not a keyboard event device; live keyboard test skipped.\n");
        close(fd);
        return;
    }

    unsigned long keyBits[128];
    memset(keyBits, 0, sizeof(keyBits));

    int keyQuerySucceeded =
        ioctl(
            fd,
            EVIOCGBIT(EV_KEY, sizeof(keyBits)),
            keyBits) >= 0;

    if (!keyQuerySucceeded)
    {
        PrintError("EVIOCGBIT key codes");
    }
    else
    {
        Log("EVIOCGBIT key codes: succeeded.\n");
        Log("KEY_A supported: %s\n",
            IsBitSet(keyBits, KEY_A) ? "yes" : "no");
        Log("KEY_SPACE supported: %s\n",
            IsBitSet(keyBits, KEY_SPACE) ? "yes" : "no");
        Log("KEY_ENTER supported: %s\n",
            IsBitSet(keyBits, KEY_ENTER) ? "yes" : "no");
        Log("KEY_ESC supported: %s\n",
            IsBitSet(keyBits, KEY_ESC) ? "yes" : "no");
    }

    if (keyQuerySucceeded &&
        IsBitSet(keyBits, KEY_A) &&
        IsBitSet(keyBits, KEY_SPACE))
    {
        ReadForFiveSeconds(fd, path, 1);
    }
    else
    {
        Log("Expected keyboard keys unavailable; live test skipped.\n");
    }

    close(fd);
}

static void CheckSimpleDevice(const char* path)
{
    Log("\n--- Checking %s ---\n", path);

    struct stat st;

    if (stat(path, &st) < 0)
    {
        PrintError(path);
        return;
    }

    Log("Exists: yes\n");

    if (S_ISCHR(st.st_mode))
    {
        Log("Character device: major=%u minor=%u\n",
            (unsigned int)major(st.st_rdev),
            (unsigned int)minor(st.st_rdev));
    }
    else
    {
        Log("Not a character device.\n");
    }

    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
    {
        PrintError("open");
        return;
    }

    Log("Open succeeded.\n");

    unsigned long eventBits[8];
    memset(eventBits, 0, sizeof(eventBits));

    if (ioctl(
            fd,
            EVIOCGBIT(0, sizeof(eventBits)),
            eventBits) == 0)
    {
        Log("EVIOCGBIT succeeded; supports EV_KEY: %s\n",
            IsBitSet(eventBits, EV_KEY) ? "yes" : "no");
    }
    else
    {
        Log("EVIOCGBIT unavailable on this device: %s\n",
            strerror(errno));
    }

    close(fd);
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
        Log("  Character device major=%u minor=%u mode=%04o\n",
            (unsigned int)major(st.st_rdev),
            (unsigned int)minor(st.st_rdev),
            (unsigned int)(st.st_mode & 07777));
    }
    else if (S_ISDIR(st.st_mode))
    {
        Log("  Directory mode=%04o\n",
            (unsigned int)(st.st_mode & 07777));
    }
    else if (S_ISLNK(st.st_mode))
    {
        Log("  Symbolic link\n");
    }
    else
    {
        Log("  Exists, but is not a character device or directory.\n");
    }
}

int main(void)
{
    g_Log = fopen("log.txt", "w");

    if (g_Log == NULL)
    {
        fprintf(stderr,
                "Cannot create log.txt: %s\n",
                strerror(errno));
        return 1;
    }

    InstallSignalHandlers();
    atexit(RestoreSettings);

    Log("====================================================\n");
    Log("Linux Keyboard and Console Diagnostic\n");
    Log("====================================================\n");
    Log("Log file: log.txt\n");
    Log("This program does not create device nodes.\n");
    Log("Console tests temporarily change keyboard mode.\n");
    Log("New tests include devfs virtual-console paths.\n");

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
    DumpFile("/proc/mounts");
    DumpFile("/proc/bus/input/devices");

    Log("\n\n====================================================\n");
    Log("PHASE 1: CONSOLE AND DEVFS DEVICE PATHS\n");
    Log("====================================================\n");

    DumpDeviceNode("/dev/console");
    DumpDeviceNode("/dev/tty");
    DumpDeviceNode("/dev/tty0");
    DumpDeviceNode("/dev/tty1");
    DumpDeviceNode("/dev/tts/0");
    DumpDeviceNode("/dev/tts/1");
    DumpDeviceNode("/dev/vc/0");
    DumpDeviceNode("/dev/vc/1");

    Log("\nTesting standard console paths first.\n");

    TestConsoleDevice("/dev/console");
    TestConsoleDevice("/dev/tty");
    TestConsoleDevice("/dev/tty0");
    TestConsoleDevice("/dev/tty1");

    Log("\nTesting devfs terminal paths.\n");
    Log("These tests may change the keyboard mode of the associated VT.\n");

    TestConsoleDevice("/dev/tts/0");
    TestConsoleDevice("/dev/tts/1");

    Log("\nInspecting devfs /dev/vc entries without changing their mode.\n");
    CheckSimpleDevice("/dev/vc/0");
    CheckSimpleDevice("/dev/vc/1");

    Log("\n\n====================================================\n");
    Log("PHASE 2: MOUSE DEVICE\n");
    Log("====================================================\n");

    CheckSimpleDevice("/dev/input/mice");

    Log("\n\n====================================================\n");
    Log("PHASE 3: EVENT DEVICES\n");
    Log("====================================================\n");
    Log("Checking event0 through event31.\n");
    Log("Only devices advertising KEY_A and KEY_SPACE "
        "receive a five-second input test.\n");

    int i;

    for (i = 0; i < 32; ++i)
    {
        char path[64];

        snprintf(
            path,
            sizeof(path),
            "/dev/input/event%d",
            i);

        struct stat st;

        if (stat(path, &st) < 0)
        {
            Log("\n%s: does not exist (%s)\n",
                path,
                strerror(errno));
            continue;
        }

        TestEventDevice(path);
    }

    Log("\n\n====================================================\n");
    Log("DIAGNOSTIC COMPLETE\n");
    Log("====================================================\n");
    Log("Please send back the complete log.txt file.\n");

    RestoreSettings();

    fclose(g_Log);
    g_Log = NULL;

    return 0;
}
