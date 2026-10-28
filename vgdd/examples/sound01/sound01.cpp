
#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Sound/AudioMixer.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

using namespace vgd;

static volatile sig_atomic_t StopRequested = 0;

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    StopRequested = 1;
}

/*
 * Diagnostic instrumentation.
 *
 * Timing uses gettimeofday(), which is available on older Linux
 * systems. Wall-clock adjustments can affect measurements, so
 * negative elapsed intervals are clamped to zero.
 *
 * All diagnostic events are stored in a fixed-size array.
 * No file I/O is performed during the main loop.
 */

static const int MaxDiagnosticEvents = 256;

struct DiagnosticEvent
{
    uint64_t DetectedUs;
    uint64_t ScheduledUs;
    uint64_t MixedFrameAtDetection;
    uint64_t EventFrame;

    int Key;
    int Frequency;
    int ScheduleSucceeded;
};

struct DiagnosticData
{
    uint64_t StartUs;
    uint64_t LastLoopStartUs;

    uint64_t LoopCount;
    uint64_t MaxLoopGapUs;

    uint64_t TotalKeyboardUpdateUs;
    uint64_t MaxKeyboardUpdateUs;

    uint64_t TotalPresentUs;
    uint64_t MaxPresentUs;

    uint64_t TotalScheduleUs;
    uint64_t MaxScheduleUs;

    uint64_t EventCount;
    uint64_t DroppedEvents;

    DiagnosticEvent Events[MaxDiagnosticEvents];
};

static DiagnosticData Diagnostics;

static uint64_t GetTimeUs()
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        return 0;

    return
        static_cast<uint64_t>(tv.tv_sec) * 1000000ULL +
        static_cast<uint64_t>(tv.tv_usec);
}

static void InitializeDiagnostics()
{
    memset(&Diagnostics, 0, sizeof(Diagnostics));

    Diagnostics.StartUs = GetTimeUs();
    Diagnostics.LastLoopStartUs = 0;
}

static uint64_t ElapsedUs(uint64_t startUs)
{
    const uint64_t nowUs = GetTimeUs();

    if (nowUs < startUs)
        return 0;

    return nowUs - startUs;
}

static uint64_t DifferenceUs(
    uint64_t earlierUs,
    uint64_t laterUs)
{
    if (laterUs < earlierUs)
        return 0;

    return laterUs - earlierUs;
}

static void RecordDiagnosticEvent(
    uint64_t detectedUs,
    uint64_t scheduledUs,
    uint64_t mixedFrame,
    uint64_t eventFrame,
    int key,
    int frequency,
    bool succeeded)
{
    if (Diagnostics.EventCount >= MaxDiagnosticEvents)
    {
        ++Diagnostics.DroppedEvents;
        return;
    }

    DiagnosticEvent& event =
        Diagnostics.Events[Diagnostics.EventCount++];

    event.DetectedUs =
        DifferenceUs(Diagnostics.StartUs, detectedUs);

    event.ScheduledUs =
        DifferenceUs(Diagnostics.StartUs, scheduledUs);

    event.MixedFrameAtDetection = mixedFrame;
    event.EventFrame = eventFrame;
    event.Key = key;
    event.Frequency = frequency;
    event.ScheduleSucceeded = succeeded ? 1 : 0;
}

static void WriteDiagnosticLog(
    int result,
    int sampleRate,
    int channels)
{
    FILE* file = fopen("log.txt", "w");

    if (!file)
    {
        perror("Could not create log.txt");
        return;
    }

    const uint64_t elapsedUs =
        ElapsedUs(Diagnostics.StartUs);

    fprintf(file, "VGDD SOUND DIAGNOSTIC LOG\n");
    fprintf(file, "=========================\n\n");

    fprintf(file, "Result: %d\n", result);
    fprintf(file, "Sample rate: %d\n", sampleRate);
    fprintf(file, "Channels: %d\n", channels);

    fprintf(file, "Elapsed time: %llu us\n",
        static_cast<unsigned long long>(elapsedUs));

    fprintf(file, "\nMAIN LOOP\n");

    fprintf(file, "Loop iterations: %llu\n",
        static_cast<unsigned long long>(
            Diagnostics.LoopCount));

    fprintf(file, "Maximum loop-start gap: %llu us\n",
        static_cast<unsigned long long>(
            Diagnostics.MaxLoopGapUs));

    fprintf(file, "\nKEYBOARD UPDATE\n");

    fprintf(file, "Total time: %llu us\n",
        static_cast<unsigned long long>(
            Diagnostics.TotalKeyboardUpdateUs));

    fprintf(file, "Maximum update time: %llu us\n",
        static_cast<unsigned long long>(
            Diagnostics.MaxKeyboardUpdateUs));

    fprintf(file, "\nDISPLAY PRESENT\n");

    fprintf(file, "Total time: %llu us\n",
        static_cast<unsigned long long>(
            Diagnostics.TotalPresentUs));

    fprintf(file, "Maximum Present time: %llu us\n",
        static_cast<unsigned long long>(
            Diagnostics.MaxPresentUs));

    fprintf(file, "\nEVENT SCHEDULING\n");

    fprintf(file, "Total scheduling time: %llu us\n",
        static_cast<unsigned long long>(
            Diagnostics.TotalScheduleUs));

    fprintf(file, "Maximum scheduling time: %llu us\n",
        static_cast<unsigned long long>(
            Diagnostics.MaxScheduleUs));

    fprintf(file, "Recorded events: %llu\n",
        static_cast<unsigned long long>(
            Diagnostics.EventCount));

    fprintf(file, "Events not recorded (capacity): %llu\n",
        static_cast<unsigned long long>(
            Diagnostics.DroppedEvents));

    fprintf(file, "\nEVENT DETAILS\n");

    fprintf(file,
        "Index,DetectedUs,ScheduledUs,MainScheduleUs,"
        "Key,Frequency,MixedFrameAtDetection,EventFrame,Success\n");

    {
        uint64_t i;

        for (i = 0; i < Diagnostics.EventCount; ++i)
        {
            const DiagnosticEvent& event =
                Diagnostics.Events[i];

            const uint64_t scheduleUs =
                DifferenceUs(
                    event.DetectedUs,
                    event.ScheduledUs);

            fprintf(file,
                "%llu,%llu,%llu,%llu,%d,%d,%llu,%llu,%d\n",
                static_cast<unsigned long long>(i),
                static_cast<unsigned long long>(
                    event.DetectedUs),
                static_cast<unsigned long long>(
                    event.ScheduledUs),
                static_cast<unsigned long long>(
                    scheduleUs),
                event.Key,
                event.Frequency,
                static_cast<unsigned long long>(
                    event.MixedFrameAtDetection),
                static_cast<unsigned long long>(
                    event.EventFrame),
                event.ScheduleSucceeded);
        }
    }

    fclose(file);
}

static const BMPFont FontSound =
{
    Font01::Width,
    Font01::Height,
    Font01::FirstDigit,
    Font01::DigitCount,
    Font01::FirstUpper,
    Font01::UpperCount,
    Font01::GlyphSpace,
    &Font01::GlyphDigits[0][0],
    &Font01::GlyphUpper[0][0]
};

static char* AppendText(char* p, const char* s)
{
    while (*s)
        *p++ = *s++;

    return p;
}

static char* AppendUnsigned(char* p, unsigned int value)
{
    char digits[10];
    unsigned int count = 0;

    do
    {
        digits[count++] =
            static_cast<char>('0' + value % 10);

        value /= 10;
    }
    while (value);

    while (count)
        *p++ = digits[--count];

    return p;
}

static const int NoteFrequencies[] =
{
    262, 294, 330, 349, 392, 440, 494,
    523, 587, 659, 698, 784, 880, 988
};

static const char* NoteNames[] =
{
    "C4", "D4", "E4", "F4", "G4", "A4", "B4",
    "C5", "D5", "E5", "F5", "G5", "A5", "B5"
};

static const int NoteCount =
    static_cast<int>(
        sizeof(NoteFrequencies) /
        sizeof(NoteFrequencies[0]));

static const int NoteVoice = 0;
static const int NoteVolume = 6500;

/*
 * Zero scheduling lead: submit events at the mixer's current
 * frame position instead of intentionally scheduling them ahead.
 *
 * Physical output latency still depends on OSS buffering.
 */
static const uint64_t AudioLeadFrames = 0;

static void DrawFrame(
    DisplayMode13h& display,
    int keyIndex,
    int frequency,
    const char* noteName,
    int sampleRate,
    int channels,
    const char* status)
{
    display.Clear(1);

    display.DrawText(
        FontSound, "VGDD SOUND DEMO", 72, 24, 15);

    display.DrawText(
        FontSound, "OSS PCM OUTPUT", 80, 40, 14);

    display.DrawText(
        FontSound, "KEY INDEX", 32, 72, 11);

    char text[32];
    char* p = text;

    if (keyIndex < 0)
        p = AppendText(p, "NONE");
    else
        p = AppendUnsigned(
            p, static_cast<unsigned int>(keyIndex));

    *p = '\0';

    display.DrawText(FontSound, text, 122, 72, 15);
    display.DrawText(FontSound, "NOTE", 32, 88, 11);

    display.DrawText(
        FontSound,
        noteName,
        122,
        88,
        frequency ? 10 : 8);

    display.DrawText(
        FontSound, "FREQUENCY", 32, 104, 11);

    p = text;
    p = AppendUnsigned(
        p, static_cast<unsigned int>(frequency));
    p = AppendText(p, " HZ");
    *p = '\0';

    display.DrawText(FontSound, text, 122, 104, 15);

    display.DrawText(
        FontSound, "SAMPLE RATE", 32, 128, 11);

    p = text;
    p = AppendUnsigned(
        p, static_cast<unsigned int>(sampleRate));
    p = AppendText(p, " HZ");
    *p = '\0';

    display.DrawText(FontSound, text, 142, 128, 15);

    display.DrawText(
        FontSound, "CHANNELS", 32, 144, 11);

    display.DrawText(
        FontSound,
        channels == 2 ? "STEREO" : "MONO",
        112,
        144,
        15);

    display.DrawText(FontSound, status, 24, 176, 14);
    display.DrawText(FontSound, "ESC TO EXIT", 104, 200, 12);
}

static bool ParsePositiveInteger(
    const char* text,
    int& value)
{
    if (!text || !*text)
        return false;

    char* end = NULL;
    errno = 0;

    const long parsed = strtol(text, &end, 10);

    if (errno != 0 ||
        *end != '\0' ||
        parsed < 1 ||
        parsed > 192000)
    {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

static int RunGame(
    DisplayMode13h& display,
    const Sound::Configuration& soundConfiguration)
{
    Keyboard keyboard;
    Sound sound;
    AudioMixer mixer;

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool soundInitialized = false;

    int result = 0;
    int currentKey = -1;
    int currentNote = -1;

    /*
     * Track the last event timestamp so that rapid key changes
     * never submit timestamps that move backwards.
     */
    uint64_t lastScheduledFrame = 0;

    String<32> status;
    status.Assign("READY");

    if (!display.Initialize())
    {
        fprintf(
            stderr,
            "Game: display initialization failed.\n");

        display.Shutdown();
        return 1;
    }

    displayInitialized = true;

    if (!keyboard.Initialize())
    {
        fprintf(
            stderr,
            "Game: keyboard initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    keyboardInitialized = true;

    if (!sound.Initialize(soundConfiguration))
    {
        fprintf(
            stderr,
            "Game: sound initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    soundInitialized = true;

    if (!mixer.Initialize(
            sound.GetSampleRate(),
            sound.GetChannels()))
    {
        fprintf(
            stderr,
            "Game: audio mixer initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    mixer.Attach(sound);

    if (!sound.StartWorker())
    {
        fprintf(
            stderr,
            "Game: audio worker could not start.\n");

        result = 1;
        goto cleanup;
    }

    mixer.Play();
    status.Assign("PRESS A KEY");

    while (!StopRequested)
    {
        /*
         * Measure the time between consecutive main-loop starts.
         * This includes the preceding render and presentation.
         */
        const uint64_t loopStartUs = GetTimeUs();

        if (Diagnostics.LastLoopStartUs != 0)
        {
            const uint64_t gapUs =
                DifferenceUs(
                    Diagnostics.LastLoopStartUs,
                    loopStartUs);

            if (gapUs > Diagnostics.MaxLoopGapUs)
                Diagnostics.MaxLoopGapUs = gapUs;
        }

        Diagnostics.LastLoopStartUs = loopStartUs;
        ++Diagnostics.LoopCount;

        /*
         * Poll input before doing graphics work.
         */
        const uint64_t keyboardStartUs = GetTimeUs();

        keyboard.Update();

        const uint64_t keyboardUpdateUs =
            ElapsedUs(keyboardStartUs);

        Diagnostics.TotalKeyboardUpdateUs +=
            keyboardUpdateUs;

        if (keyboardUpdateUs >
            Diagnostics.MaxKeyboardUpdateUs)
        {
            Diagnostics.MaxKeyboardUpdateUs =
                keyboardUpdateUs;
        }

        if (!keyboard.IsConsoleActive())
            break;

        const bool ctrlDown =
            keyboard.IsDown(Keyboard::KeyLeftCtrl) ||
            keyboard.IsDown(Keyboard::KeyRightCtrl);

        if ((ctrlDown &&
             keyboard.IsDown(Keyboard::KeyC)) ||
            keyboard.IsDown(Keyboard::KeyEscape))
        {
            break;
        }

        /*
         * Identify the first currently pressed key.
         */
        int pressedKey = -1;

        for (int key = 1;
             key < Keyboard::KeyCount;
             ++key)
        {
            if (keyboard.IsDown(
                    static_cast<Keyboard::Key>(key)))
            {
                pressedKey = key;
                break;
            }
        }

        int noteIndex = -1;

        if (pressedKey >= 0)
            noteIndex = (pressedKey - 1) % NoteCount;

        /*
         * Record timing immediately around event scheduling.
         * This measures queue submission, not worker consumption.
         */
        if (pressedKey != currentKey)
        {
            const int frequency =
                noteIndex >= 0
                    ? NoteFrequencies[noteIndex]
                    : 0;

            const uint64_t mixedFrame =
                mixer.GetMixedFramePosition();

            uint64_t eventFrame =
                mixedFrame + AudioLeadFrames;

            if (eventFrame <= lastScheduledFrame)
                eventFrame = lastScheduledFrame + 1;

            const uint64_t detectedUs = GetTimeUs();

            const bool scheduled =
                mixer.ScheduleOscillator(
                    eventFrame,
                    NoteVoice,
                    frequency,
                    NoteVolume);

            const uint64_t scheduledUs = GetTimeUs();

            const uint64_t scheduleUs =
                DifferenceUs(detectedUs, scheduledUs);

            Diagnostics.TotalScheduleUs += scheduleUs;

            if (scheduleUs > Diagnostics.MaxScheduleUs)
                Diagnostics.MaxScheduleUs = scheduleUs;

            RecordDiagnosticEvent(
                detectedUs,
                scheduledUs,
                mixedFrame,
                eventFrame,
                pressedKey,
                frequency,
                scheduled);

            if (!scheduled)
            {
                fprintf(
                    stderr,
                    "Game: could not schedule oscillator event.\n");

                result = 1;
                break;
            }

            lastScheduledFrame = eventFrame;

            currentKey = pressedKey;
            currentNote = noteIndex;

            if (noteIndex >= 0)
                status.Assign("PLAYING NOTE");
            else
                status.Assign("NOTE RELEASED");
        }

        /*
         * Check for audio errors before doing graphics work.
         */
        if (sound.WorkerFailed())
        {
            fprintf(
                stderr,
                "Game: audio worker failed.\n");

            result = 1;
            break;
        }

        /*
         * Preserve the original demo UI.
         */
        const int frequency =
            currentNote >= 0
                ? NoteFrequencies[currentNote]
                : 0;

        const char* noteName =
            currentNote >= 0
                ? NoteNames[currentNote]
                : "---";

        DrawFrame(
            display,
            currentKey,
            frequency,
            noteName,
            sound.GetSampleRate(),
            sound.GetChannels(),
            status);

        /*
         * Measure presentation separately because a slow or
         * blocking Present() can delay the next input poll.
         */
        const uint64_t presentStartUs = GetTimeUs();

        const bool presented = display.Present();

        const uint64_t presentUs =
            ElapsedUs(presentStartUs);

        Diagnostics.TotalPresentUs += presentUs;

        if (presentUs > Diagnostics.MaxPresentUs)
            Diagnostics.MaxPresentUs = presentUs;

        if (!presented)
        {
            fprintf(
                stderr,
                "Game: display presentation failed.\n");

            result = 1;
            break;
        }
    }

cleanup:
    /*
     * Shutdown joins the audio worker before closing OSS.
     */
    if (soundInitialized)
        sound.Shutdown();

    if (keyboardInitialized)
        keyboard.Shutdown();

    if (displayInitialized)
        display.Shutdown();

    return result;
}

int main(int argc, char* argv[])
{
    Sound::Configuration configuration;

    /*
     * Usage:
     * sound1 [sample_rate] [channels]
     *
     * Defaults: 22050 Hz, mono.
     *
     * Examples:
     * sound1
     * sound1 44100 2
     */
    if (argc > 3)
    {
        fprintf(
            stderr,
            "Usage: %s [sample_rate] [channels]\n",
            argv[0]);

        return 1;
    }

    if (argc >= 2 &&
        !ParsePositiveInteger(
            argv[1],
            configuration.SampleRate))
    {
        fprintf(stderr, "Invalid sample rate.\n");
        return 1;
    }

    if (argc >= 3)
    {
        int channels = 0;

        if (!ParsePositiveInteger(argv[2], channels) ||
            (channels != 1 && channels != 2))
        {
            fprintf(
                stderr,
                "Channels must be 1 (mono) or 2 (stereo).\n");

            return 1;
        }

        configuration.Channels = channels;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_handler = HandleSignal;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) != 0)
        perror("sigaction SIGINT");

    if (sigaction(SIGTERM, &action, NULL) != 0)
        perror("sigaction SIGTERM");

    if (sigaction(SIGHUP, &action, NULL) != 0)
        perror("sigaction SIGHUP");

    /*
     * Initialize before RunGame so the log includes startup
     * and initialization time, including initialization failures.
     */
    InitializeDiagnostics();

    DisplayMode13h display;

    const int result = RunGame(display, configuration);

    WriteDiagnosticLog(
        result,
        configuration.SampleRate,
        configuration.Channels);

    return result;
}
