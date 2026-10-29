
#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Sound/AudioMixer.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

using namespace vgd;

static volatile sig_atomic_t StopRequested = 0;

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    StopRequested = 1;
}

static const BMPFont FontChipstorm =
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

/*
 * CHIPSTORM - NEON DREAMS
 *
 * Audio is synthesized by AudioMixer's square-wave
 * oscillator. The spectrum remains a visual effect,
 * not an FFT of the audio output.
 */

static const int SongNotes[] =
{
    659, 784, 988, 784, 659, 523, 587, 659,
    784, 988, 1175, 988, 784, 659, 587, 523,
    659, 784, 988, 1318, 1175, 988, 784, 659,
    587, 659, 784, 659, 523, 587, 659, 523,

    784, 988, 1175, 988, 784, 659, 784, 988,
    1047, 988, 784, 659, 587, 659, 784, 988,
    1175, 1318, 1568, 1318, 1175, 988, 784, 659,
    784, 988, 784, 659, 587, 523, 587, 659
};

static const int SongNoteCount =
    static_cast<int>(sizeof(SongNotes) / sizeof(SongNotes[0]));

static const int SongTempoMilliseconds = 170;
static const int SongLengthMilliseconds =
    SongNoteCount * SongTempoMilliseconds;

static const int SpectrumBarCount = 24;
static const int SpectrumBarWidth = 10;
static const int SpectrumBarGap = 2;
static const int SpectrumBarStep =
    SpectrumBarWidth + SpectrumBarGap;

static const int MelodyVoice = 0;
static const int MelodyVolume = 6500;

static const uint64_t AudioLeadFrames = 128;
static const int LookAheadSteps = 3;

static const uint8_t SpectrumColors[] =
{
    2, 3, 3, 10, 10, 11, 11, 14,
    14, 15, 15, 12, 12, 14, 11, 10,
    10, 3, 3, 2, 2, 6, 6, 5
};

static unsigned long GetMilliseconds()
{
    struct timeval timeValue;
    gettimeofday(&timeValue, NULL);

    return static_cast<unsigned long>(timeValue.tv_sec) * 1000UL +
           static_cast<unsigned long>(timeValue.tv_usec / 1000);
}

static int GetNote(int step)
{
    if (step < 0 || step >= SongNoteCount)
        return 0;

    return SongNotes[step];
}

static void DrawBackground(DisplayMode13h& display,
                           unsigned long frameNumber)
{
    uint8_t* buffer = display.GetBackBuffer();
    const int pitch = display.GetBackBufferPitch();
    const int width = display.GetWidth();
    const int height = display.GetHeight();

    for (int y = 0; y < height; ++y)
    {
        uint8_t* row = buffer + y * pitch;
        const int band =
            ((y + static_cast<int>(frameNumber / 3)) / 24) & 1;

        for (int x = 0; x < width; ++x)
        {
            const int pattern = ((x / 32) + band) & 1;
            row[x] = static_cast<uint8_t>(pattern ? 1 : 0);
        }
    }
}

static void DrawSpectrum(DisplayMode13h& display,
                         int step,
                         unsigned long frameNumber,
                         bool playing)
{
    const int width = display.GetWidth();
    const int baseY = 163;
    const int startX =
        (width - SpectrumBarCount * SpectrumBarStep) / 2;

    for (int i = 0; i < SpectrumBarCount; ++i)
    {
        unsigned int seed =
            static_cast<unsigned int>(i * 37 + step * 19);

        seed ^= static_cast<unsigned int>(
            (frameNumber / 3 + i * 7) * (i + 3));

        seed ^= seed >> 7;
        seed *= 2654435761U;
        seed ^= seed >> 11;

        int height = 8 + static_cast<int>(seed % 49);

        const int distance = i < 12 ? i : 23 - i;
        height = height * (12 + distance * 3) / 36;

        if (i == (step % SpectrumBarCount))
            height += 15;

        if (i == ((step + 5) % SpectrumBarCount))
            height += 9;

        if (!playing)
            height = 4 + (height / 4);

        if (height > 70)
            height = 70;

        const int x = startX + i * SpectrumBarStep;
        const int y = baseY - height;
        const uint8_t color = SpectrumColors[i];

        display.DrawRectangle(x, y, SpectrumBarWidth, height, color);

        if (height > 5)
        {
            const uint8_t capColor =
                SpectrumColors[(i + 8) % SpectrumBarCount];

            display.DrawRectangle(
                x, y, SpectrumBarWidth, 2, capColor);
        }
    }

    display.DrawRectangle(startX - 2, baseY + 1,
                          SpectrumBarCount * SpectrumBarStep, 1, 8);
}

static void DrawBanner(DisplayMode13h& display,
                       unsigned long frameNumber)
{
    const int width = display.GetWidth();

    display.DrawRectangle(8, 8, width - 16, 40, 4);
    display.DrawRectangle(10, 10, width - 20, 36, 1);

    const char* title = "CHIPSTORM";
    const char* subtitle = "NEON DREAMS";

    const int titleX = (width - 9 * FontChipstorm.Width) / 2;
    const int subtitleX = (width - 11 * FontChipstorm.Width) / 2;

    const int flash = static_cast<int>((frameNumber / 8) % 2);

    display.DrawText(FontChipstorm, title, titleX, 14,
                     flash ? 15 : 14);

    display.DrawText(FontChipstorm, subtitle, subtitleX, 30, 11);

    const int accent = static_cast<int>((frameNumber / 4) % 14) + 1;

    display.DrawRectangle(12, 12, 3, 3, accent);
    display.DrawRectangle(width - 15, 12, 3, 3, accent);
    display.DrawRectangle(12, 41, 3, 3, accent);
    display.DrawRectangle(width - 15, 41, 3, 3, accent);
}

static void DrawFrame(DisplayMode13h& display,
                      int step,
                      int note,
                      unsigned long frameNumber,
                      bool playing)
{
    DrawBackground(display, frameNumber);
    DrawBanner(display, frameNumber);

    display.DrawText(FontChipstorm, "SYNTH", 18, 58, 11);
    display.DrawText(FontChipstorm, "SPECTRUM", 18, 73, 14);

    DrawSpectrum(display, step, frameNumber, playing);

    display.DrawRectangle(12, 177, display.GetWidth() - 24, 14, 4);

    display.DrawText(FontChipstorm,
                     playing ? "PLAYING" : "PAUSED",
                     18, 180, playing ? 10 : 12);

    display.DrawText(FontChipstorm, "ESC EXIT", 222, 180, 15);

    const int progressWidth = display.GetWidth() - 32;
    const int progressX = 16;
    const int progressY = 197;

    display.DrawRectangle(progressX, progressY, progressWidth, 2, 8);

    int progress = 0;

    if (SongNoteCount > 0)
        progress = step * progressWidth / SongNoteCount;

    if (progress > progressWidth)
        progress = progressWidth;

    if (progress > 0)
        display.DrawRectangle(progressX, progressY, progress, 2, 10);

    const int cursorX =
        12 + static_cast<int>((frameNumber * 3) %
                              (display.GetWidth() - 24));

    display.DrawRectangle(cursorX, 168, 3, 3, 14);

    if (note > 0)
        display.DrawText(FontChipstorm, "NOTE ON", 228, 58, 10);
    else
        display.DrawText(FontChipstorm, "REST", 228, 58, 8);
}

static uint64_t GetEventFrame(AudioMixer& mixer,
                              uint64_t lastScheduledFrame)
{
    uint64_t frame =
        mixer.GetMixedFramePosition() + AudioLeadFrames;

    if (frame <= lastScheduledFrame)
        frame = lastScheduledFrame + 1;

    return frame;
}

static int RunGame(DisplayMode13h& display,
                   const Sound::Configuration& soundConfiguration)
{
    Keyboard keyboard;
    Sound sound;
    AudioMixer mixer;

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool soundInitialized = false;
    int result = 0;

    bool playing = true;
    bool previousPDown = false;

    const uint64_t FramesPerStepNumerator = 0;
    (void)FramesPerStepNumerator;

    int sampleRate = 0;
    uint64_t stepFrames = 0;
    uint64_t songLengthFrames = 0;

    /*
     * nextStep and nextStepFrame identify the next melody
     * event that has not yet been submitted to the mixer.
     */
    int nextStep = 0;
    uint64_t nextStepFrame = 0;
    uint64_t lastScheduledFrame = 0;
    uint64_t songOriginFrame = 0;

    int currentStep = 0;
    int currentNote = SongNotes[0];

    unsigned long frameNumber = 0;

    if (!display.Initialize())
    {
        fprintf(stderr, "CHIPSTORM: display initialization failed.\n");
        display.Shutdown();
        return 1;
    }

    displayInitialized = true;

    if (!keyboard.Initialize())
    {
        fprintf(stderr, "CHIPSTORM: keyboard initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    keyboardInitialized = true;

    if (!sound.Initialize(soundConfiguration))
    {
        fprintf(stderr, "CHIPSTORM: sound initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    soundInitialized = true;

    if (!mixer.Initialize(sound.GetSampleRate(), sound.GetChannels()))
    {
        fprintf(stderr, "CHIPSTORM: audio mixer initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    mixer.Attach(sound);

    sampleRate = sound.GetSampleRate();

    /*
     * Use integer output-frame durations so the melody timing
     * is independent of the rendering loop.
     */
    stepFrames =
        static_cast<uint64_t>(sampleRate) *
        SongTempoMilliseconds / 1000;

    if (stepFrames == 0)
        stepFrames = 1;

    songLengthFrames = stepFrames * SongNoteCount;

    if (!sound.StartWorker())
    {
        fprintf(stderr, "CHIPSTORM: audio worker could not start.\n");
        result = 1;
        goto cleanup;
    }

    mixer.Play();

    /*
     * Begin slightly ahead of the current audio frame, then
     * submit a few steps of events to keep the queue supplied.
     */
    songOriginFrame =
        mixer.GetMixedFramePosition() + AudioLeadFrames;

    nextStepFrame = songOriginFrame;
    nextStep = 0;

    while (!StopRequested)
    {
        keyboard.Update();

        if (!keyboard.IsConsoleActive())
            break;

        const bool ctrlDown =
            keyboard.IsDown(Keyboard::KeyLeftCtrl) ||
            keyboard.IsDown(Keyboard::KeyRightCtrl);

        if ((ctrlDown && keyboard.IsDown(Keyboard::KeyC)) ||
            keyboard.IsDown(Keyboard::KeyEscape))
        {
            break;
        }

        const bool pDown = keyboard.IsDown(Keyboard::KeyP);

        if (pDown && !previousPDown)
        {
            if (playing)
            {
                /*
                 * Determine the currently audible step from
                 * the audio clock, not from the display frame.
                 */
                const uint64_t audioFrame =
                    mixer.GetMixedFramePosition();

                if (audioFrame >= songOriginFrame)
                {
                    const uint64_t position =
                        (audioFrame - songOriginFrame) %
                        songLengthFrames;

                    currentStep =
                        static_cast<int>(position / stepFrames);
                }

                currentNote = GetNote(currentStep);

                /*
                 * The queue can already contain future melody
                 * events. Schedule silence after the last queued
                 * event to preserve strict event ordering.
                 */
                const uint64_t silenceFrame =
                    lastScheduledFrame + 1;

                if (!mixer.ScheduleOscillator(
                        silenceFrame, MelodyVoice, 0, MelodyVolume))
                {
                    fprintf(stderr,
                            "CHIPSTORM: could not schedule pause event.\n");
                    result = 1;
                    break;
                }

                lastScheduledFrame = silenceFrame;
                mixer.Pause();
                playing = false;
            }
            else
            {
                mixer.Play();
                playing = true;

                /*
                 * Restart at the paused step. The event begins
                 * after both the current audio frame and any
                 * previously queued events.
                 */
                const uint64_t resumeFrame =
                    GetEventFrame(mixer, lastScheduledFrame);

                if (!mixer.ScheduleOscillator(
                        resumeFrame,
                        MelodyVoice,
                        GetNote(currentStep),
                        MelodyVolume))
                {
                    fprintf(stderr,
                            "CHIPSTORM: could not schedule resume event.\n");
                    result = 1;
                    break;
                }

                lastScheduledFrame = resumeFrame;

                /*
                 * Rebase the song timeline so the resumed note
                 * is step currentStep and the next step follows
                 * one full step duration later.
                 */
                songOriginFrame =
                    resumeFrame -
                    static_cast<uint64_t>(currentStep) * stepFrames;

                nextStep = (currentStep + 1) % SongNoteCount;
                nextStepFrame = resumeFrame + stepFrames;
            }
        }

        previousPDown = pDown;

        if (playing)
        {
            const uint64_t audioFrame =
                mixer.GetMixedFramePosition();

            /*
             * Submit only a short look-ahead. Events are inserted
             * in timestamp order, and the bounded queue is never
             * filled with an entire looping song.
             */
            const uint64_t lookAheadFrames =
                stepFrames * LookAheadSteps;

            const uint64_t horizon =
                audioFrame + lookAheadFrames;

            int scheduledThisLoop = 0;

            while (nextStepFrame <= horizon &&
                   scheduledThisLoop < LookAheadSteps + 2)
            {
                if (!mixer.ScheduleOscillator(
                        nextStepFrame,
                        MelodyVoice,
                        GetNote(nextStep),
                        MelodyVolume))
                {
                    fprintf(stderr,
                            "CHIPSTORM: could not schedule melody note.\n");
                    result = 1;
                    break;
                }

                lastScheduledFrame = nextStepFrame;

                ++nextStep;
                if (nextStep >= SongNoteCount)
                    nextStep = 0;

                nextStepFrame += stepFrames;
                ++scheduledThisLoop;
            }

            if (result != 0)
                break;

            /*
             * Display position follows the mixer frame clock.
             */
            if (audioFrame >= songOriginFrame)
            {
                const uint64_t position =
                    (audioFrame - songOriginFrame) %
                    songLengthFrames;

                currentStep =
                    static_cast<int>(position / stepFrames);
            }

            currentNote = GetNote(currentStep);
        }

        if (sound.WorkerFailed())
        {
            fprintf(stderr, "CHIPSTORM: audio worker failed.\n");
            result = 1;
            break;
        }

        DrawFrame(display,
                  currentStep,
                  currentNote,
                  frameNumber,
                  playing);

        if (!display.Present())
        {
            fprintf(stderr, "CHIPSTORM: display presentation failed.\n");
            result = 1;
            break;
        }

        ++frameNumber;
    }

cleanup:
    /*
     * Stop and join the audio worker before releasing OSS.
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
     * chipstorm [sample_rate] [channels]
     *
     * Examples:
     * chipstorm
     * chipstorm 22050 1
     * chipstorm 44100 2
     */
    if (argc > 3)
    {
        fprintf(stderr, "Usage: %s [sample_rate] [channels]\n", argv[0]);
        return 1;
    }

    if (argc >= 2)
    {
        char* end = NULL;
        long rate = strtol(argv[1], &end, 10);

        if (!argv[1][0] || *end != '\0' || rate < 1 || rate > 192000)
        {
            fprintf(stderr, "CHIPSTORM: invalid sample rate.\n");
            return 1;
        }

        configuration.SampleRate = static_cast<int>(rate);
    }

    if (argc >= 3)
    {
        char* end = NULL;
        long channels = strtol(argv[2], &end, 10);

        if (!argv[2][0] || *end != '\0' ||
            (channels != 1 && channels != 2))
        {
            fprintf(stderr,
                    "CHIPSTORM: channels must be 1 (mono) or 2 (stereo).\n");
            return 1;
        }

        configuration.Channels = static_cast<int>(channels);
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

    DisplayMode13h display;

    return RunGame(display, configuration);
}
