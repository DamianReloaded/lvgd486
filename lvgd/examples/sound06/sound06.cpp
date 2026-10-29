#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Sound/AudioMixer.hpp"
#include "../../vgd/Sound/WavLoader.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace vgd;

// -----------------------------------------------------------------------------
// Signal handling
// -----------------------------------------------------------------------------

static volatile sig_atomic_t StopRequested = 0;

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    StopRequested = 1;
}

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------

static double GetTimeSeconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return static_cast<double>(tv.tv_sec) +
           static_cast<double>(tv.tv_usec) / 1000000.0;
}

// -----------------------------------------------------------------------------
// Font
// -----------------------------------------------------------------------------

static const BMPFont FontSound06 =
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

// -----------------------------------------------------------------------------
// Instruments
// -----------------------------------------------------------------------------

enum InstrumentId
{
    InstrumentBassDrum = 0,
    InstrumentSnare,
    InstrumentHiHat,
    InstrumentBassGuitar,
    InstrumentPiano,
    InstrumentCount
};

static const int InstrumentChannels[InstrumentCount] =
{
    0, 1, 2, 3, 4
};

static const char* InstrumentFiles[InstrumentCount] =
{
    "bdrum.wav",
    "snare.wav",
    "hihat.wav",
    "bassgtr.wav",
    "piano.wav"
};

static const char* InstrumentNames[InstrumentCount] =
{
    "KICK",
    "SNARE",
    "HIHAT",
    "BASS",
    "PIANO"
};

static const int InstrumentColors[InstrumentCount] =
{
    12, 14, 11, 10, 13
};

// -----------------------------------------------------------------------------
// Song configuration
// -----------------------------------------------------------------------------

static const int TempoBPM = 110;
static const int StepsPerBeat = 4;
static const int StepsPerBar = 16;
static const int SongBars = 16;
static const int SongLengthSteps = StepsPerBar * SongBars;

static const int DrumVolume = 26000;
static const int BassVolume = 22000;
static const int PianoVolume = 20000;
static const int HiHatVolume = 12500;

// Keep events scheduled well ahead of the audio worker's current position.
static const uint64_t ScheduleLookAheadSeconds = 4;

// -----------------------------------------------------------------------------
// Track event
// -----------------------------------------------------------------------------

struct TrackEvent
{
    int Step;
    int Instrument;
    int SemitoneOffset;
    int Volume;
};

// -----------------------------------------------------------------------------
// Accompaniment pattern
// -----------------------------------------------------------------------------

static const TrackEvent Pattern[] =
{
    {  0, InstrumentBassDrum,  0, DrumVolume },
    {  0, InstrumentHiHat,     0, HiHatVolume },
    {  0, InstrumentPiano,     0, PianoVolume },

    {  2, InstrumentHiHat,     0, HiHatVolume },
    {  2, InstrumentBassGuitar,0, BassVolume },

    {  4, InstrumentSnare,     0, DrumVolume },
    {  4, InstrumentHiHat,     0, HiHatVolume },
    {  4, InstrumentPiano,    0, PianoVolume },

    {  6, InstrumentHiHat,     0, HiHatVolume },
    {  6, InstrumentBassGuitar,0, BassVolume },

    {  8, InstrumentBassDrum,  0, DrumVolume },
    {  8, InstrumentHiHat,     0, HiHatVolume },
    {  8, InstrumentPiano,    0, PianoVolume },

    { 10, InstrumentHiHat,     0, HiHatVolume },
    { 10, InstrumentBassGuitar,0, BassVolume },

    { 12, InstrumentSnare,     0, DrumVolume },
    { 12, InstrumentHiHat,     0, HiHatVolume },
    { 12, InstrumentPiano,    0, PianoVolume },

    { 14, InstrumentHiHat,     0, HiHatVolume },
    { 14, InstrumentBassGuitar,0, BassVolume }
};

static const uint64_t PatternEventCount =
    sizeof(Pattern) / sizeof(Pattern[0]);

// -----------------------------------------------------------------------------
// Melody
// -----------------------------------------------------------------------------

static const int MelodyBars = 8;

static const int Melody[MelodyBars][4] =
{
    { 0, 4, 5, 7 },
    { 0, 4, 5, 7 },
    { 0, 4, 5, 7 },
    { 0, 4, 2, 2 },
    { 4, 4, 2, 0 },
    { 0, 4, 7, 7 },
    { 7, 5, 4, 5 },
    { 7, 4, 0, 0 }
};

static const int BassRoots[MelodyBars] =
{
    0, 0, 5, 5, 0, 0, 7, 7
};

// -----------------------------------------------------------------------------
// Pitch adjustment
// -----------------------------------------------------------------------------

static int GetPitchedSampleRate(
    int sampleRate,
    int semitoneOffset)
{
    static const uint32_t SemitoneRatios[25] =
    {
        10000, 10595, 11225, 11892, 12599,
        13348, 14142, 14983, 15874, 16818,
        17818, 18878, 20000, 21189, 22449,
        23784, 25198, 26697, 28284, 29966,
        31748, 33636, 35636, 37756, 40000
    };

    if (sampleRate <= 0)
        return 0;

    if (semitoneOffset < 0)
        semitoneOffset = 0;

    if (semitoneOffset > 24)
        semitoneOffset = 24;

    const uint64_t scaledRate =
        static_cast<uint64_t>(sampleRate) *
        SemitoneRatios[semitoneOffset];

    return static_cast<int>(
        (scaledRate + 5000ULL) / 10000ULL);
}

// -----------------------------------------------------------------------------
// Tracker step to absolute output frame
// -----------------------------------------------------------------------------

static uint64_t StepToFrame(
    uint64_t step,
    int sampleRate)
{
    return step * static_cast<uint64_t>(sampleRate) * 60ULL /
           (static_cast<uint64_t>(TempoBPM) * StepsPerBeat);
}

// -----------------------------------------------------------------------------
// Infinite repeating song
// -----------------------------------------------------------------------------

static bool GetSongEvent(
    uint64_t eventIndex,
    uint64_t& absoluteStep,
    TrackEvent& event)
{
    const uint64_t absoluteBar =
        eventIndex / PatternEventCount;

    const uint64_t patternIndex =
        eventIndex % PatternEventCount;

    const int songBar =
        static_cast<int>(absoluteBar % SongBars);

    const int melodyBar =
        songBar % MelodyBars;

    event = Pattern[patternIndex];

    absoluteStep =
        absoluteBar * StepsPerBar +
        static_cast<uint64_t>(event.Step);

    if (event.Instrument == InstrumentPiano)
    {
        const int beat = event.Step / StepsPerBeat;

        event.SemitoneOffset = Melody[melodyBar][beat];

        if (songBar >= MelodyBars)
            event.SemitoneOffset += 12;
    }
    else if (event.Instrument == InstrumentBassGuitar)
    {
        const int beatPosition = event.Step / 4;

        event.SemitoneOffset =
            BassRoots[melodyBar] +
            ((beatPosition & 1) ? 7 : 0);

        if (songBar >= MelodyBars && beatPosition == 3)
            event.SemitoneOffset += 2;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Schedule ahead
//
// Keep this function short and deterministic. The main loop schedules events;
// the audio worker generates and plays the actual audio.
// -----------------------------------------------------------------------------

static bool ScheduleSongAhead(
    AudioMixer& mixer,
    WavLoader samples[InstrumentCount],
    int sampleRate,
    uint64_t& nextEventIndex)
{
    const uint64_t currentFrame =
        mixer.GetMixedFramePosition();

    const uint64_t horizon =
        currentFrame +
        static_cast<uint64_t>(sampleRate) *
        ScheduleLookAheadSeconds;

    uint64_t absoluteStep = 0;
    TrackEvent event;

    while (GetSongEvent(
        nextEventIndex,
        absoluteStep,
        event))
    {
        const uint64_t eventFrame =
            StepToFrame(absoluteStep, sampleRate);

        if (eventFrame > horizon)
            break;

        WavLoader& sample = samples[event.Instrument];

        const int pitchedSampleRate =
            GetPitchedSampleRate(
                sample.GetSampleRate(),
                event.SemitoneOffset);

        if (!mixer.SchedulePlay(
                eventFrame,
                InstrumentChannels[event.Instrument],
                sample.GetSamples(),
                sample.GetSampleFrames(),
                pitchedSampleRate,
                event.Volume,
                event.Volume,
                false))
        {
            // Do not skip an event if the queue is temporarily full.
            return false;
        }

        ++nextEventIndex;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Lightweight display
// -----------------------------------------------------------------------------

static void DrawFrame(
    DisplayMode13h& display,
    unsigned long frame,
    int fps,
    int sampleRate,
    int channels,
    uint64_t nextEventIndex)
{
    (void)frame;

    const int width = display.GetWidth();
    const int height = display.GetHeight();

    // Clear the screen with a simple solid background.
    display.Clear(1);

    display.DrawRectangle(5, 5, width - 10, 25, 4);
    display.DrawRectangle(7, 7, width - 14, 21, 1);

    const char* title = "SAMPLE TRACKER";

    display.DrawText(
        FontSound06,
        title,
        (width -
         static_cast<int>(strlen(title)) * FontSound06.Width) / 2,
        13,
        15);

    char text[48];

    snprintf(
        text,
        sizeof(text),
        "%d BPM  %d Hz  %d CH",
        TempoBPM,
        sampleRate,
        channels);

    display.DrawText(
        FontSound06,
        text,
        8,
        43,
        11);

    display.DrawText(
        FontSound06,
        "INSTRUMENTS",
        8,
        65,
        15);

    for (int i = 0; i < InstrumentCount; ++i)
    {
        const int y = 82 + i * 15;

        display.DrawRectangle(
            10, y, 7, 7, InstrumentColors[i]);

        display.DrawText(
            FontSound06,
            InstrumentNames[i],
            24,
            y,
            InstrumentColors[i]);
    }

    char fpsText[20];

    snprintf(
        fpsText,
        sizeof(fpsText),
        "FPS %d",
        fps);

    display.DrawText(
        FontSound06,
        fpsText,
        width - static_cast<int>(strlen(fpsText)) *
            FontSound06.Width - 8,
        height - 20,
        fps >= 55 ? 10 : (fps >= 30 ? 14 : 12));

    display.DrawText(
        FontSound06,
        "ESC TO EXIT",
        8,
        height - 20,
        14);

    (void)nextEventIndex;
}

// -----------------------------------------------------------------------------
// Application
// -----------------------------------------------------------------------------

static int RunGame(
    DisplayMode13h& display,
    const Sound::Configuration& configuration)
{
    Keyboard keyboard;
    Sound sound;
    AudioMixer mixer;
    WavLoader samples[InstrumentCount];

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool soundInitialized = false;

    int result = 0;
    uint64_t nextEventIndex = 0;

    // Load samples before starting the audio worker.
    for (int i = 0; i < InstrumentCount; ++i)
    {
        if (!samples[i].Load(InstrumentFiles[i]))
        {
            fprintf(
                stderr,
                "SOUND06: could not load %s: %s\n",
                InstrumentFiles[i],
                samples[i].GetError());

            return 1;
        }

        if (samples[i].GetSampleFrames() == 0 ||
            samples[i].GetSampleRate() <= 0)
        {
            fprintf(
                stderr,
                "SOUND06: invalid or empty sample: %s\n",
                InstrumentFiles[i]);

            return 1;
        }

        fprintf(
            stdout,
            "SOUND06: loaded %-12s %d Hz, %llu frames.\n",
            InstrumentFiles[i],
            samples[i].GetSampleRate(),
            static_cast<unsigned long long>(
                samples[i].GetSampleFrames()));
    }

    if (!display.Initialize())
    {
        fprintf(stderr,
                "SOUND06: display initialization failed.\n");
        return 1;
    }

    displayInitialized = true;

    if (!display.SetFrameRate(60))
    {
        fprintf(stderr,
                "SOUND06: could not set display frame rate.\n");
        result = 1;
        goto cleanup;
    }

    if (!keyboard.Initialize())
    {
        fprintf(stderr,
                "SOUND06: keyboard initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    keyboardInitialized = true;

    if (!sound.Initialize(configuration))
    {
        fprintf(stderr,
                "SOUND06: sound initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    soundInitialized = true;

    if (!mixer.Initialize(
            sound.GetSampleRate(),
            configuration.Channels))
    {
        fprintf(stderr,
                "SOUND06: mixer initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    if (!mixer.Attach(sound))
    {
        fprintf(stderr,
                "SOUND06: PCM callback setup failed.\n");
        result = 1;
        goto cleanup;
    }

    // Schedule the initial events before playback starts.
    if (!ScheduleSongAhead(
            mixer,
            samples,
            sound.GetSampleRate(),
            nextEventIndex))
    {
        fprintf(stderr,
                "SOUND06: initial event scheduling failed.\n");
        result = 1;
        goto cleanup;
    }

    mixer.Play();

    if (!sound.StartWorker())
    {
        fprintf(stderr,
                "SOUND06: audio worker could not start.\n");
        result = 1;
        goto cleanup;
    }

    // Main loop: keyboard, scheduling, drawing, presentation, FPS.
    {
        double fpsStart = GetTimeSeconds();
        int fpsFrames = 0;
        int fps = 0;
        unsigned long frame = 0;

        while (!StopRequested)
        {
            const double now = GetTimeSeconds();

            keyboard.Update();

            if (!keyboard.IsConsoleActive())
                break;

            if (keyboard.IsDown(Keyboard::KeyEscape) ||
                ((keyboard.IsDown(Keyboard::KeyLeftCtrl) ||
                  keyboard.IsDown(Keyboard::KeyRightCtrl)) &&
                 keyboard.IsDown(Keyboard::KeyC)))
            {
                break;
            }

            if (sound.WorkerFailed())
            {
                fprintf(stderr,
                        "SOUND06: audio worker failed.\n");
                result = 1;
                break;
            }

            // Behave like the chiptune tracker: schedule from the main
            // loop and let the audio worker handle PCM generation.
            if (!ScheduleSongAhead(
                    mixer,
                    samples,
                    sound.GetSampleRate(),
                    nextEventIndex))
            {
                // Queue pressure is diagnostic; the unscheduled event
                // remains pending and will be retried next iteration.
                fprintf(stderr,
                        "SOUND06: event scheduling temporarily failed.\n");
            }

            DrawFrame(
                display,
                frame,
                fps,
                sound.GetSampleRate(),
                configuration.Channels,
                nextEventIndex);

            if (!display.Present())
            {
                fprintf(stderr,
                        "SOUND06: display presentation failed.\n");
                result = 1;
                break;
            }

            ++frame;
            ++fpsFrames;

            if (now - fpsStart >= 1.0)
            {
                const double elapsed = now - fpsStart;

                fps = static_cast<int>(
                    fpsFrames / elapsed + 0.5);

                fpsFrames = 0;
                fpsStart = now;
            }
        }
    }

cleanup:
    // Stop the worker before destroying the mixer or WAV sample buffers.
    if (soundInitialized)
        sound.Shutdown();

    if (keyboardInitialized)
        keyboard.Shutdown();

    if (displayInitialized)
        display.Shutdown();

    return result;
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    if (setpriority(PRIO_PROCESS, 0, -10) != 0)
        perror("setpriority");

    Sound::Configuration configuration;

    if (argc > 3)
    {
        fprintf(stderr,
                "Usage: %s [sample_rate] [channels]\n",
                argv[0]);
        return 1;
    }

    if (argc >= 2)
    {
        char* end = NULL;
        const long rate = strtol(argv[1], &end, 10);

        if (!argv[1][0] || *end ||
            rate < 4000 || rate > 192000)
        {
            fprintf(stderr,
                    "SOUND06: invalid sample rate.\n");
            return 1;
        }

        configuration.SampleRate = static_cast<int>(rate);
    }

    if (argc >= 3)
    {
        char* end = NULL;
        const long channels = strtol(argv[2], &end, 10);

        if (!argv[2][0] || *end ||
            (channels != 1 && channels != 2))
        {
            fprintf(stderr,
                    "SOUND06: channels must be 1 or 2.\n");
            return 1;
        }

        configuration.Channels = static_cast<int>(channels);
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));

    action.sa_handler = HandleSignal;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    DisplayMode13h display;

    return RunGame(display, configuration);
}
