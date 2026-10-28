
#include "../../vgd/Sound/Midi.hpp"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/soundcard.h>

using namespace vgd;

static FILE* g_Log = 0;

static void Log(const char* format, ...)
{
    va_list args;

    va_start(args, format);

    vprintf(format, args);

    if (g_Log != 0)
    {
        va_list logArgs;

        va_start(logArgs, format);
        vfprintf(g_Log, format, logArgs);
        va_end(logArgs);

        fflush(g_Log);
    }

    va_end(args);
}

static void LogFile(const char* path)
{
    FILE* file;
    char line[512];

    Log("\n--- %s ---\n", path);

    file = fopen(path, "r");

    if (file == 0)
    {
        Log("Unable to open: %s\n", strerror(errno));
        return;
    }

    while (fgets(line, sizeof(line), file) != 0)
    {
        Log("%s", line);
    }

    fclose(file);
}

static void CheckPath(const char* path)
{
    struct stat info;

    Log("\nPath: %s\n", path);

    if (stat(path, &info) != 0)
    {
        Log("  stat failed: %s\n", strerror(errno));
        return;
    }

    Log("  Type: ");

    if (S_ISCHR(info.st_mode))
    {
        Log("character device\n");
    }
    else if (S_ISBLK(info.st_mode))
    {
        Log("block device\n");
    }
    else if (S_ISREG(info.st_mode))
    {
        Log("regular file\n");
    }
    else if (S_ISDIR(info.st_mode))
    {
        Log("directory\n");
    }
    else
    {
        Log("other\n");
    }

    Log("  Mode: %o\n", (unsigned int)(info.st_mode & 07777));
    Log("  Owner UID: %u\n", (unsigned int)info.st_uid);
    Log("  Owner GID: %u\n", (unsigned int)info.st_gid);
    Log("  Device major/minor: %u/%u\n",
        (unsigned int)((info.st_rdev >> 8) & 0xfff),
        (unsigned int)(info.st_rdev & 0xff));
}

static void TestSequencer(const char* path)
{
    int fd;
    int synthCount;
    int midiCount;

    Log("\n========================================\n");
    Log("Testing sequencer: %s\n", path);

    fd = open(path, O_WRONLY);

    if (fd < 0)
    {
        Log("open(O_WRONLY) failed: %s (errno=%d)\n",
            strerror(errno), errno);
        return;
    }

    Log("open(O_WRONLY) succeeded, fd=%d\n", fd);

    synthCount = -1;

    errno = 0;

    if (ioctl(fd, SNDCTL_SEQ_NRSYNTHS, &synthCount) == 0)
    {
        Log("SNDCTL_SEQ_NRSYNTHS succeeded: %d\n", synthCount);
    }
    else
    {
        Log("SNDCTL_SEQ_NRSYNTHS failed: %s (errno=%d)\n",
            strerror(errno), errno);
    }

    midiCount = -1;

    errno = 0;

    if (ioctl(fd, SNDCTL_SEQ_NRMIDIS, &midiCount) == 0)
    {
        Log("SNDCTL_SEQ_NRMIDIS succeeded: %d\n", midiCount);
    }
    else
    {
        Log("SNDCTL_SEQ_NRMIDIS failed: %s (errno=%d)\n",
            strerror(errno), errno);
    }

    /*
     * Inspect each synthesizer reported by the sequencer.
     */
    if (synthCount > 0)
    {
        int i;

        for (i = 0; i < synthCount; ++i)
        {
            struct synth_info info;

            memset(&info, 0, sizeof(info));
            info.device = i;

            errno = 0;

            if (ioctl(fd, SNDCTL_SYNTH_INFO, &info) == 0)
            {
                Log("Synth %d: name='%s', type=%d, subtype=%d\n",
                    i, info.name, info.synth_type, info.synth_subtype);
            }
            else
            {
                Log("Synth %d: SNDCTL_SYNTH_INFO failed: %s\n",
                    i, strerror(errno));
            }
        }
    }

    /*
     * Inspect each MIDI port reported by the sequencer.
     */
    if (midiCount > 0)
    {
        int i;

        for (i = 0; i < midiCount; ++i)
        {
            struct midi_info info;

            memset(&info, 0, sizeof(info));
            info.device = i;

            errno = 0;

            if (ioctl(fd, SNDCTL_MIDI_INFO, &info) == 0)
            {
                Log("MIDI port %d: name='%s'\n", i, info.name);
            }
            else
            {
                Log("MIDI port %d: SNDCTL_MIDI_INFO failed: %s\n",
                    i, strerror(errno));
            }
        }
    }

    close(fd);
    Log("Closed sequencer device.\n");
}

static void ListDirectory(const char* path)
{
    DIR* dir;
    struct dirent* entry;

    Log("\n--- Directory listing: %s ---\n", path);

    dir = opendir(path);

    if (dir == 0)
    {
        Log("Unable to open directory: %s\n", strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != 0)
    {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0)
        {
            Log("%s/%s\n", path, entry->d_name);
        }
    }

    closedir(dir);
}

int main()
{
    struct utsname systemInfo;
    Midi midi;

    const char* sequencerPaths[] =
    {
        "/dev/sequencer",
        "/dev/sound/sequencer",
        "/dev/sequencer2",
        "/dev/sound/sequencer2"
    };

    const char* devicePaths[] =
    {
        "/dev/dsp",
        "/dev/dsp0",
        "/dev/sound/dspW",
        "/dev/sound/dsp",
        "/dev/midi",
        "/dev/midi0",
        "/dev/midi1",
        "/dev/midi2",
        "/dev/midi3",
        "/dev/midi00",
        "/dev/midi01",
        "/dev/midi02",
        "/dev/midi03",
        "/dev/mixer",
        "/dev/sound/mixer",
        "/dev/sound/sequencer",
        "/dev/sound/sequencer2",
        "/dev/sequencer",
        "/dev/sequencer2"
    };

    unsigned int i;

    g_Log = fopen("log.txt", "w");

    if (g_Log == 0)
    {
        printf("Could not create log.txt: %s\n", strerror(errno));
        return 1;
    }

    Log("OSS MIDI diagnostic report\n");
    Log("==========================\n");

    Log("Effective UID: %u\n", (unsigned int)geteuid());
    Log("Effective GID: %u\n", (unsigned int)getegid());

    if (uname(&systemInfo) == 0)
    {
        Log("\nSystem information:\n");
        Log("  System: %s\n", systemInfo.sysname);
        Log("  Node: %s\n", systemInfo.nodename);
        Log("  Release: %s\n", systemInfo.release);
        Log("  Version: %s\n", systemInfo.version);
        Log("  Machine: %s\n", systemInfo.machine);
    }
    else
    {
        Log("uname failed: %s\n", strerror(errno));
    }

    LogFile("/proc/version");
    LogFile("/proc/devices");
    LogFile("/proc/modules");
    LogFile("/proc/asound/cards");
    LogFile("/proc/asound/devices");
    LogFile("/proc/asound/seq/clients");
    LogFile("/proc/interrupts");
    LogFile("/dev/sndstat");

    ListDirectory("/dev");
    ListDirectory("/dev/sound");
    ListDirectory("/dev/snd");

    Log("\n========================================\n");
    Log("Checking device nodes\n");

    for (i = 0; i < sizeof(devicePaths) / sizeof(devicePaths[0]); ++i)
    {
        CheckPath(devicePaths[i]);
    }

    Log("\n========================================\n");
    Log("Testing OSS sequencer interfaces\n");

    for (i = 0; i < sizeof(sequencerPaths) / sizeof(sequencerPaths[0]); ++i)
    {
        TestSequencer(sequencerPaths[i]);
    }

    Log("\n========================================\n");
    Log("Testing Midi class initialization\n");

    if (midi.Initialize())
    {
        Log("Midi::Initialize succeeded.\n");
        Log("Selected path: %s\n", midi.GetDevicePath());
        Log("Synth count: %d\n", midi.GetSynthCount());
        Log("MIDI port count: %d\n", midi.GetMidiDeviceCount());

        if (midi.GetOutputType() == Midi::OutputSynthesizer)
        {
            Log("Selected output type: OSS synthesizer\n");
        }
        else if (midi.GetOutputType() == Midi::OutputMidiPort)
        {
            Log("Selected output type: raw MIDI port\n");
        }

        Log("Synth device index: %d\n", midi.GetSynthDevice());
        Log("MIDI port index: %d\n", midi.GetMidiDevice());

        /*
         * Don't send notes during diagnostics. We only want to
         * discover and inspect devices without producing sound.
         */
        midi.Shutdown();
    }
    else
    {
        Log("Midi::Initialize failed: %s\n", midi.GetError());
    }

    Log("\n========================================\n");
    Log("Diagnostic report complete.\n");

    fclose(g_Log);
    g_Log = 0;

    printf("\nDiagnostic report written to log.txt\n");

    return 0;
}
