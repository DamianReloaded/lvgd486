
#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Sound/AudioMixer.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

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
// Font
// -----------------------------------------------------------------------------

static const BMPFont FontChipfusion =
{
    Font01::Width, Font01::Height,
    Font01::FirstDigit, Font01::DigitCount,
    Font01::FirstUpper, Font01::UpperCount,
    Font01::GlyphSpace,
    &Font01::GlyphDigits[0][0],
    &Font01::GlyphUpper[0][0]
};

// -----------------------------------------------------------------------------
// Built-in demonstration song
// -----------------------------------------------------------------------------

static const int NotesLead[] =
{
    659, 784, 988, 784, 659, 523, 587, 659,
    784, 988, 1175, 988, 784, 659, 587, 523,
    659, 784, 988, 1318, 1175, 988, 784, 659,
    587, 659, 784, 659, 523, 587, 659, 523
};

static const int NotesHarmony[] =
{
    392, 392, 494, 494, 392, 392, 330, 330,
    392, 392, 494, 494, 523, 523, 494, 494,
    440, 440, 494, 494, 523, 523, 392, 392,
    330, 330, 392, 392, 262, 262, 330, 330
};

static const int NotesBass[] =
{
    131, 131, 165, 165, 196, 196, 165, 165,
    131, 131, 165, 165, 196, 196, 262, 262,
    220, 220, 196, 196, 165, 165, 131, 131,
    147, 147, 165, 165, 131, 131, 131, 131
};

static const int NotesPercussion[] =
{
    1, 0, 0, 1, 0, 0, 1, 0,
    1, 0, 0, 1, 0, 1, 0, 0,
    1, 0, 0, 1, 0, 0, 1, 0,
    1, 0, 1, 0, 0, 1, 0, 0
};

static const int NoteCount = 32;
static const int TempoMs = 180;

// -----------------------------------------------------------------------------
// Mixer voices
// -----------------------------------------------------------------------------

static const int LeadVoice = 0;
static const int HarmonyVoice = 1;
static const int BassVoice = 2;
static const int PercussionVoice = 3;

static const int LeadVolume = 6500;
static const int HarmonyVolume = 3500;
static const int BassVolume = 5000;
static const int PercussionVolume = 6000;

// -----------------------------------------------------------------------------
// Demo configuration
// -----------------------------------------------------------------------------

static const int SpectrumBars = 24;
static const int BarWidth = 10;
static const int BarGap = 2;
static const int BarStep = BarWidth + BarGap;

static const uint8_t SpectrumColors[SpectrumBars] =
{
    1, 2, 3, 3, 10, 10, 11, 11,
    14, 14, 15, 15, 12, 12, 14, 11,
    10, 10, 3, 3, 2, 6, 6, 5
};

// -----------------------------------------------------------------------------
// Fixed-timestep simulation
// -----------------------------------------------------------------------------

static const uint64_t UpdateStepNs = 16666667ULL;
static const uint64_t MaxElapsedNs = 100000000ULL;
static const int MaxCatchUpSteps = 5;

static const int AudioLookAheadSteps = 3;
static const int MaxScheduledStepsPerFrame = 8;

static uint64_t GetMonotonicNanoseconds()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return static_cast<uint64_t>(tv.tv_sec) * 1000000000ULL
         + static_cast<uint64_t>(tv.tv_usec) * 1000ULL;
}

// -----------------------------------------------------------------------------
// Demo and tracker state
// -----------------------------------------------------------------------------

struct DemoState
{
    uint64_t Tick;
    uint64_t NextStepFrame;
    int CurrentStep;

    DemoState()
        : Tick(0),
          NextStepFrame(0),
          CurrentStep(0)
    {
    }
};

static void UpdateDemo(DemoState& state)
{
    ++state.Tick;
}

// -----------------------------------------------------------------------------
// Tracker scheduling
// -----------------------------------------------------------------------------

static bool ScheduleTrackerStep(
    AudioMixer& mixer,
    int sampleRate,
    int step,
    uint64_t frame)
{
    const uint64_t stepFrames =
        (static_cast<uint64_t>(sampleRate) * TempoMs) / 1000ULL;

    if (stepFrames == 0)
        return false;

    // Update the three oscillators at the same output frame.
    // The mixer preserves each oscillator's phase when its
    // frequency changes between notes.
    if (!mixer.ScheduleOscillator(
            frame,
            LeadVoice,
            NotesLead[step],
            LeadVolume))
    {
        return false;
    }

    if (!mixer.ScheduleOscillator(
            frame,
            HarmonyVoice,
            NotesHarmony[step],
            HarmonyVolume))
    {
        return false;
    }

    if (!mixer.ScheduleOscillator(
            frame,
            BassVoice,
            NotesBass[step],
            BassVolume))
    {
        return false;
    }

    // Percussion is generated directly by the mixer.
    // Each hit has its own deterministic noise state and envelope.
    if (NotesPercussion[step])
    {
        if (!mixer.ScheduleNoiseHit(
                frame,
                PercussionVoice,
                stepFrames,
                PercussionVolume))
        {
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// FPS counter
// -----------------------------------------------------------------------------

struct FpsCounter
{
    uint64_t WindowStartNs;
    unsigned int Frames;
    unsigned int CurrentFPS;

    FpsCounter()
        : WindowStartNs(0),
          Frames(0),
          CurrentFPS(0)
    {
    }

    void Reset(uint64_t currentTimeNs)
    {
        WindowStartNs = currentTimeNs;
        Frames = 0;
        CurrentFPS = 0;
    }

    void FramePresented(uint64_t currentTimeNs)
    {
        if (WindowStartNs == 0)
        {
            Reset(currentTimeNs);
            return;
        }

        ++Frames;

        if (currentTimeNs < WindowStartNs)
        {
            Reset(currentTimeNs);
            return;
        }

        const uint64_t elapsedNs =
            currentTimeNs - WindowStartNs;

        if (elapsedNs >= 1000000000ULL)
        {
            CurrentFPS = static_cast<unsigned int>(
                (static_cast<uint64_t>(Frames) *
                 1000000000ULL + elapsedNs / 2) /
                elapsedNs);

            WindowStartNs = currentTimeNs;
            Frames = 0;
        }
    }
};

// -----------------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------------

static void DrawBackground(
    DisplayMode13h& display,
    unsigned long frame)
{
    uint8_t* buffer = display.GetBackBuffer();
    const int pitch = display.GetBackBufferPitch();

    for (int y = 0; y < display.GetHeight(); ++y)
    {
        uint8_t* row = buffer + y * pitch;

        const int band =
            ((y + static_cast<int>(frame / 3)) / 24) & 1;

        for (int x = 0; x < display.GetWidth(); ++x)
        {
            row[x] = static_cast<uint8_t>(
                (((x / 32) + band) & 1) ? 1 : 0);
        }
    }
}

static void DrawSpectrum(
    DisplayMode13h& display,
    int currentStep,
    bool playing,
    unsigned long frame)
{
    const int startX =
        (display.GetWidth() - SpectrumBars * BarStep) / 2;

    const int baseY = 153;

    for (int i = 0; i < SpectrumBars; ++i)
    {
        uint32_t seed = static_cast<uint32_t>(
            i * 37 + currentStep * 19 + frame * (i + 3));

        seed ^= seed >> 7;
        seed *= 2654435761U;
        seed ^= seed >> 11;

        int height = 7 + static_cast<int>(seed % 48);

        if (i % 6 == 0)
            height += 14;

        if (!playing)
            height /= 3;

        if (height > 68)
            height = 68;

        const int x = startX + i * BarStep;
        const int y = baseY - height;

        display.DrawRectangle(
            x, y, BarWidth, height, SpectrumColors[i]);

        if (height > 4)
        {
            display.DrawRectangle(
                x, y, BarWidth, 2,
                SpectrumColors[(i + 8) % SpectrumBars]);
        }
    }

    display.DrawRectangle(
        startX - 2, baseY + 1,
        SpectrumBars * BarStep, 1, 8);
}

static void DrawFrame(
    DisplayMode13h& display,
    const AudioMixer& mixer,
    int currentStep,
    unsigned long frame,
    unsigned int fps)
{
    DrawBackground(display, frame);

    const int width = display.GetWidth();
    const bool playing = mixer.IsPlaying();

    display.DrawRectangle(8, 8, width - 16, 40, 4);
    display.DrawRectangle(10, 10, width - 20, 36, 1);

    const char* title = "CHIPFUSION";
    const char* subtitle = "DIGITAL HORIZON";

    display.DrawText(
        FontChipfusion, title,
        (width - 10 * FontChipfusion.Width) / 2,
        14, (frame / 8) % 2 ? 14 : 15);

    display.DrawText(
        FontChipfusion, subtitle,
        (width - 15 * FontChipfusion.Width) / 2,
        30, 11);

    display.DrawText(
        FontChipfusion, "4 CHANNEL MIXER",
        18, 58, 11);

    display.DrawText(
        FontChipfusion, "SPECTRUM",
        18, 73, 14);

    char fpsText[16];

    snprintf(
        fpsText, sizeof(fpsText),
        "FPS %u", fps);

    display.DrawText(
        FontChipfusion, fpsText,
        width - 8 - 6 * FontChipfusion.Width,
        73, 10);

    DrawSpectrum(display, currentStep, playing, frame);

    static const char* ChannelNames[4] =
    {
        "LEAD", "HARMONY", "BASS", "DRUMS"
    };

    for (int i = 0; i < 4; ++i)
    {
        const int x = 20 + i * 75;

        int level = 1;

        if (playing)
        {
            if (i == 3)
                level = NotesPercussion[currentStep] ? 8 : 2;
            else
                level = 8;
        }

        display.DrawRectangle(
            x, 166, 9, 9, SpectrumColors[i * 5]);

        display.DrawText(
            FontChipfusion, ChannelNames[i],
            x + 12, 166, 11);

        display.DrawRectangle(x, 179, 62, 3, 8);

        display.DrawRectangle(
            x, 179, level * 7, 3,
            SpectrumColors[i * 5]);
    }

    display.DrawRectangle(12, 192, width - 24, 12, 4);

    display.DrawText(
        FontChipfusion,
        playing ? "PLAYING" : "PAUSED",
        18, 194, playing ? 10 : 12);

    display.DrawText(
        FontChipfusion, "P PAUSE", 112, 194, 11);

    display.DrawText(
        FontChipfusion, "ESC EXIT", 222, 194, 15);

    const int progressWidth = width - 32;
    const int progress =
        currentStep * progressWidth / NoteCount;

    display.DrawRectangle(
        16, 211, progressWidth, 2, 8);

    if (progress > 0)
        display.DrawRectangle(16, 211, progress, 2, 10);
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

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool soundInitialized = false;

    int result = 0;

    DemoState demo;

    if (!display.Initialize())
    {
        fprintf(
            stderr,
            "CHIPFUSION: display initialization failed.\n");

        display.Shutdown();
        return 1;
    }

    displayInitialized = true;

    if (!display.SetFrameRate(60))
    {
        fprintf(
            stderr,
            "CHIPFUSION: could not set display frame rate.\n");

        result = 1;
        goto cleanup;
    }

    if (!keyboard.Initialize())
    {
        fprintf(
            stderr,
            "CHIPFUSION: keyboard initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    keyboardInitialized = true;

    if (!sound.Initialize(configuration))
    {
        fprintf(
            stderr,
            "CHIPFUSION: sound initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    soundInitialized = true;

    if (!mixer.Initialize(
            sound.GetSampleRate(),
            configuration.Channels))
    {
        fprintf(
            stderr,
            "CHIPFUSION: mixer initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    if (!mixer.Attach(sound))
    {
        fprintf(
            stderr,
            "CHIPFUSION: PCM callback setup failed.\n");

        result = 1;
        goto cleanup;
    }

    if (!sound.StartWorker())
    {
        fprintf(
            stderr,
            "CHIPFUSION: audio worker could not start.\n");

        result = 1;
        goto cleanup;
    }

    {
        FpsCounter fpsCounter;

        const int sampleRate = sound.GetSampleRate();

        const uint64_t stepFrames =
            (static_cast<uint64_t>(sampleRate) * TempoMs) / 1000ULL;

        const uint64_t lookAheadFrames =
            stepFrames * AudioLookAheadSteps;

        uint64_t previousTime = GetMonotonicNanoseconds();
        uint64_t accumulator = 0;

        uint64_t previousAudioFrame =
            mixer.GetMixedFramePosition();

        demo.NextStepFrame = previousAudioFrame + 128;

        fpsCounter.Reset(previousTime);

        bool previousPDown = false;

        mixer.Play();

        while (!StopRequested)
        {
            const uint64_t currentTime =
                GetMonotonicNanoseconds();

            uint64_t elapsed =
                currentTime >= previousTime
                    ? currentTime - previousTime
                    : 0;

            previousTime = currentTime;

            if (elapsed > MaxElapsedNs)
                elapsed = MaxElapsedNs;

            accumulator += elapsed;

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

            const bool pDown =
                keyboard.IsDown(Keyboard::KeyP);

            if (pDown && !previousPDown)
                mixer.TogglePlayback();

            previousPDown = pDown;

            const uint64_t audioFrame =
                mixer.GetMixedFramePosition();

            if (mixer.IsPlaying())
            {
                int scheduledSteps = 0;

                while (demo.NextStepFrame <=
                           audioFrame + lookAheadFrames &&
                       scheduledSteps < MaxScheduledStepsPerFrame)
                {
                    if (!ScheduleTrackerStep(
                            mixer,
                            sampleRate,
                            demo.CurrentStep,
                            demo.NextStepFrame))
                    {
                        fprintf(
                            stderr,
                            "CHIPFUSION: tracker scheduling failed "
                            "at step %d, frame %llu.\n",
                            demo.CurrentStep,
                            static_cast<unsigned long long>(
                                demo.NextStepFrame));

                        result = 1;
                        break;
                    }

                    demo.CurrentStep =
                        (demo.CurrentStep + 1) % NoteCount;

                    demo.NextStepFrame += stepFrames;
                    ++scheduledSteps;
                }

                if (result != 0)
                    break;

                // If the main loop falls far behind, restart the
                // scheduling timeline ahead of the audio clock.
                // Avoid flooding the fixed-size event queue.
                if (scheduledSteps == MaxScheduledStepsPerFrame &&
                    demo.NextStepFrame <=
                        audioFrame + lookAheadFrames)
                {
                    demo.NextStepFrame =
                        audioFrame + lookAheadFrames + 128;
                }
            }
            else
            {
                // The mixer output-frame clock continues during pause.
                // Shift the next tracker deadline by the same amount,
                // preserving its position relative to queued events.
                if (audioFrame >= previousAudioFrame)
                {
                    demo.NextStepFrame +=
                        audioFrame - previousAudioFrame;
                }
            }

            previousAudioFrame = audioFrame;

            int catchUpSteps = 0;

            while (accumulator >= UpdateStepNs &&
                   catchUpSteps < MaxCatchUpSteps)
            {
                UpdateDemo(demo);

                accumulator -= UpdateStepNs;
                ++catchUpSteps;
            }

            if (accumulator >= UpdateStepNs)
                accumulator %= UpdateStepNs;

            DrawFrame(
                display,
                mixer,
                demo.CurrentStep,
                static_cast<unsigned long>(demo.Tick),
                fpsCounter.CurrentFPS);

            if (!display.Present())
            {
                fprintf(
                    stderr,
                    "CHIPFUSION: display presentation failed.\n");

                result = 1;
                break;
            }

            fpsCounter.FramePresented(
                GetMonotonicNanoseconds());

            if (sound.WorkerFailed())
            {
                fprintf(
                    stderr,
                    "CHIPFUSION: audio worker failed.\n");

                result = 1;
                break;
            }
        }
    }

cleanup:
    // Stop the worker before destroying the mixer or its dependencies.
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
    Sound::Configuration configuration;

    if (argc > 3)
    {
        fprintf(
            stderr,
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
            fprintf(
                stderr,
                "CHIPFUSION: invalid sample rate.\n");

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
            fprintf(
                stderr,
                "CHIPFUSION: channels must be 1 or 2.\n");

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
