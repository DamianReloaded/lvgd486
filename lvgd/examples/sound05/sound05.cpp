#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Sound/AudioMixer.hpp"
#include "../../vgd/Sound/WavLoader.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>

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

static const BMPFont FontSound05 =
{
    Font01::Width, Font01::Height,
    Font01::FirstDigit, Font01::DigitCount,
    Font01::FirstUpper, Font01::UpperCount,
    Font01::GlyphSpace,
    &Font01::GlyphDigits[0][0],
    &Font01::GlyphUpper[0][0]
};

// -----------------------------------------------------------------------------
// Demo configuration
// -----------------------------------------------------------------------------

static const int WaveformWidth = 288;
static const int WaveformCenterY = 119;
static const int WaveformHalfHeight = 25;

static const int InitialVolume = 20000;
static const int VolumeStep = 2048;
static const int MaximumVolume = 32767;

static const uint8_t WaveformColors[] =
{
    3, 10, 11, 14, 15, 14, 11, 10
};

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------

static const uint64_t MaxElapsedNs = 100000000ULL;

static uint64_t GetMonotonicNanoseconds()
{
    // gettimeofday() is wall-clock time, not a true monotonic clock.
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return static_cast<uint64_t>(tv.tv_sec) * 1000000000ULL
         + static_cast<uint64_t>(tv.tv_usec) * 1000ULL;
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
                (static_cast<uint64_t>(Frames) * 1000000000ULL
                 + elapsedNs / 2) / elapsedNs);

            WindowStartNs = currentTimeNs;
            Frames = 0;
        }
    }
};

// -----------------------------------------------------------------------------
// Precomputed waveform
// -----------------------------------------------------------------------------

struct WaveformColumn
{
    int16_t Minimum;
    int16_t Maximum;
};

static void BuildWaveform(
    const WavLoader& wav,
    WaveformColumn* waveform,
    int width)
{
    const int16_t* samples = wav.GetSamples();
    const uint64_t frames = wav.GetSampleFrames();

    for (int x = 0; x < width; ++x)
    {
        uint64_t begin =
            static_cast<uint64_t>(x) * frames / width;

        uint64_t end =
            static_cast<uint64_t>(x + 1) * frames / width;

        if (end <= begin)
            end = begin + 1;

        if (begin >= frames)
            begin = frames - 1;

        if (end > frames)
            end = frames;

        int16_t minimum = INT16_MAX;
        int16_t maximum = INT16_MIN;

        for (uint64_t i = begin; i < end; ++i)
        {
            if (samples[i] < minimum)
                minimum = samples[i];

            if (samples[i] > maximum)
                maximum = samples[i];
        }

        waveform[x].Minimum = minimum;
        waveform[x].Maximum = maximum;
    }
}

// -----------------------------------------------------------------------------
// Background
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

// -----------------------------------------------------------------------------
// Waveform rendering
// -----------------------------------------------------------------------------

static void DrawWaveform(
    DisplayMode13h& display,
    const WaveformColumn* waveform,
    uint64_t playbackFrame,
    uint64_t totalFrames,
    bool playing)
{
    const int startX =
        (display.GetWidth() - WaveformWidth) / 2;

    const int topY =
        WaveformCenterY - WaveformHalfHeight - 3;

    const int bottomY =
        WaveformCenterY + WaveformHalfHeight + 3;

    display.DrawRectangle(
        startX - 3, topY - 3,
        WaveformWidth + 6,
        bottomY - topY + 6, 4);

    display.DrawRectangle(
        startX - 1, topY - 1,
        WaveformWidth + 2,
        bottomY - topY + 2, 1);

    // Zero-amplitude reference line.
    display.DrawRectangle(
        startX, WaveformCenterY,
        WaveformWidth, 1, 8);

    for (int x = 0; x < WaveformWidth; ++x)
    {
        const int maximum = waveform[x].Maximum;
        const int minimum = waveform[x].Minimum;

        int upper = WaveformCenterY
            - maximum * WaveformHalfHeight / 32768;

        int lower = WaveformCenterY
            - minimum * WaveformHalfHeight / 32768;

        if (upper < topY + 2)
            upper = topY + 2;

        if (lower > bottomY - 2)
            lower = bottomY - 2;

        int height = lower - upper + 1;

        if (height < 1)
            height = 1;

        const int color =
            WaveformColors[(x / 36) % 8];

        display.DrawRectangle(
            startX + x, upper, 1, height, color);
    }

    uint64_t positionX = 0;

    if (totalFrames > 0)
    {
        positionX =
            playbackFrame * static_cast<uint64_t>(WaveformWidth)
            / totalFrames;
    }

    if (positionX >= static_cast<uint64_t>(WaveformWidth))
        positionX = WaveformWidth - 1;

    const int markerX = startX + static_cast<int>(positionX);

    display.DrawRectangle(
        markerX, topY, 1, bottomY - topY + 1,
        playing ? 15 : 12);
}

// -----------------------------------------------------------------------------
// Volume meter
// -----------------------------------------------------------------------------

static void DrawVolume(
    DisplayMode13h& display,
    int volume)
{
    const int x = 92;
    const int y = 177;
    const int width = 216;
    const int height = 7;

    display.DrawRectangle(x, y, width, height, 8);

    const int filled = volume * width / MaximumVolume;

    if (filled > 0)
    {
        display.DrawRectangle(
            x, y, filled, height, 10);

        if (filled > 4)
        {
            display.DrawRectangle(
                x, y, filled, 2, 14);
        }
    }

    char volumeText[24];

    snprintf(
        volumeText,
        sizeof(volumeText),
        "VOLUME %d",
        volume * 100 / MaximumVolume);

    display.DrawText(
        FontSound05, volumeText, 18, 174, 11);
}

// -----------------------------------------------------------------------------
// Frame rendering
// -----------------------------------------------------------------------------

static void DrawFrame(
    DisplayMode13h& display,
    const WavLoader& wav,
    const WaveformColumn* waveform,
    const AudioMixer& mixer,
    uint64_t playbackFrame,
    int volume,
    unsigned long frame,
    unsigned int fps)
{
    DrawBackground(display, frame);

    const int width = display.GetWidth();
    const bool playing = mixer.IsPlaying();

    display.DrawRectangle(8, 8, width - 16, 39, 4);
    display.DrawRectangle(10, 10, width - 20, 35, 1);

    display.DrawText(
        FontSound05, "WAV PLAYER",
        (width - 10 * FontSound05.Width) / 2,
        14,
        (frame / 8) % 2 ? 14 : 15);

    display.DrawText(
        FontSound05, "DIGITAL AUDIO",
        (width - 13 * FontSound05.Width) / 2,
        30, 11);

    display.DrawText(
        FontSound05, "MUSIC WAV", 18, 58, 14);

    char rateText[24];

    snprintf(
        rateText,
        sizeof(rateText),
        "%d HZ",
        wav.GetSampleRate());

    display.DrawText(
        FontSound05, rateText, 18, 72, 11);

    char fpsText[16];

    snprintf(
        fpsText,
        sizeof(fpsText),
        "FPS %u",
        fps);

int x = width - 8 - 6 * FontSound05.Width;

display.DrawText(
        FontSound05, fpsText,
        x-1,
        71, 0);

display.DrawText(
        FontSound05, fpsText,
        x+1,
        73, 0);

    display.DrawText(
        FontSound05, fpsText,
        width - 8 - 6 * FontSound05.Width,
        72, 10);
        
    DrawWaveform(
        display,
        waveform,
        playbackFrame,
        wav.GetSampleFrames(),
        playing);

    DrawVolume(display, volume);

    // Playback status and controls.
    display.DrawRectangle(12, 194, width - 24, 12, 4);

    display.DrawText(
        FontSound05,
        playing ? "PLAYING" : "PAUSED",
        18, 196, playing ? 10 : 12);

    display.DrawText(
        FontSound05, "P PAUSE", 104, 196, 11);

    display.DrawText(
        FontSound05, "ESC EXIT", 222, 196, 15);

    // Whole-track progress bar.
    const int progressWidth = width - 32;

    uint64_t progress = 0;

    if (wav.GetSampleFrames() > 0)
    {
        progress =
            playbackFrame * static_cast<uint64_t>(progressWidth)
            / wav.GetSampleFrames();
    }

    if (progress > static_cast<uint64_t>(progressWidth))
        progress = progressWidth;

    display.DrawRectangle(
        16, 216, progressWidth, 3, 8);

    if (progress > 0)
    {
        display.DrawRectangle(
            16, 216, static_cast<int>(progress), 3, 10);
    }

}

// -----------------------------------------------------------------------------
// Voice scheduling
// -----------------------------------------------------------------------------

static bool ScheduleWav(
    AudioMixer& mixer,
    const WavLoader& wav,
    int volume)
{
    // The current mixer uses one mono sample value per source frame.
    // The WAV buffer must remain valid for the lifetime of the voice.
    return mixer.PlaySample(
        0,
        wav.GetSamples(),
        wav.GetSampleFrames(),
        wav.GetSampleRate(),
        volume,
        volume,
        true);
}

static bool ScheduleWavAtFrame(
    AudioMixer& mixer,
    const WavLoader& wav,
    int volume,
    uint64_t outputFrame)
{
    return mixer.SchedulePlay(
        outputFrame,
        0,
        wav.GetSamples(),
        wav.GetSampleFrames(),
        wav.GetSampleRate(),
        volume,
        volume,
        true);
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
    WavLoader wav;

    WaveformColumn waveform[WaveformWidth];

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool soundInitialized = false;

    int result = 0;
    int volume = InitialVolume;

    // -------------------------------------------------------------------------
    // Load WAV before starting the audio worker.
    // -------------------------------------------------------------------------

    if (!wav.Load("music.wav"))
    {
        fprintf(
            stderr,
            "SOUND05: WAV loading failed: %s\n",
            wav.GetError());

        return 1;
    }

    if (wav.GetSampleFrames() == 0 ||
        wav.GetSampleRate() <= 0)
    {
        fprintf(
            stderr,
            "SOUND05: WAV contains no playable samples.\n");

        return 1;
    }

    fprintf(
        stdout,
        "SOUND05: loaded music.wav, %d Hz, %llu frames.\n",
        wav.GetSampleRate(),
        static_cast<unsigned long long>(wav.GetSampleFrames()));

    BuildWaveform(wav, waveform, WaveformWidth);

    // -------------------------------------------------------------------------
    // Initialize display.
    // -------------------------------------------------------------------------

    if (!display.Initialize())
    {
        fprintf(
            stderr,
            "SOUND05: display initialization failed.\n");

        display.Shutdown();
        return 1;
    }

    displayInitialized = true;

    if (!display.SetFrameRate(60))
    {
        fprintf(
            stderr,
            "SOUND05: could not set display frame rate.\n");

        result = 1;
        goto cleanup;
    }

    // -------------------------------------------------------------------------
    // Initialize keyboard.
    // -------------------------------------------------------------------------

    if (!keyboard.Initialize())
    {
        fprintf(
            stderr,
            "SOUND05: keyboard initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    keyboardInitialized = true;

    // -------------------------------------------------------------------------
    // Initialize sound device.
    // -------------------------------------------------------------------------

    if (!sound.Initialize(configuration))
    {
        fprintf(
            stderr,
            "SOUND05: sound initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    soundInitialized = true;

    // -------------------------------------------------------------------------
    // Initialize mixer and attach it to the PCM output.
    // -------------------------------------------------------------------------

    if (!mixer.Initialize(
            sound.GetSampleRate(),
            configuration.Channels))
    {
        fprintf(
            stderr,
            "SOUND05: mixer initialization failed.\n");

        result = 1;
        goto cleanup;
    }

    if (!mixer.Attach(sound))
    {
        fprintf(
            stderr,
            "SOUND05: PCM callback setup failed.\n");

        result = 1;
        goto cleanup;
    }

    if (!ScheduleWav(mixer, wav, volume))
    {
        fprintf(
            stderr,
            "SOUND05: could not schedule WAV playback.\n");

        result = 1;
        goto cleanup;
    }

    // Start playback explicitly.
    mixer.Play();

    if (!sound.StartWorker())
    {
        fprintf(
            stderr,
            "SOUND05: audio worker could not start.\n");

        result = 1;
        goto cleanup;
    }

    // -------------------------------------------------------------------------
    // Main loop.
    // -------------------------------------------------------------------------

    {
        FpsCounter fpsCounter;

        uint64_t previousTime = GetMonotonicNanoseconds();
        uint64_t playbackElapsedNs = 0;

        fpsCounter.Reset(previousTime);

        bool previousPDown = false;
        bool previousUpDown = false;
        bool previousDownDown = false;

        while (!StopRequested)
        {
            const uint64_t currentTime =
                GetMonotonicNanoseconds();

            uint64_t elapsed = currentTime >= previousTime
                ? currentTime - previousTime
                : 0;

            previousTime = currentTime;

            if (elapsed > MaxElapsedNs)
                elapsed = MaxElapsedNs;

            // Track playback time independently of display frame rate.
            if (mixer.IsPlaying())
                playbackElapsedNs += elapsed;

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

            // P toggles playback on the key-down edge.
            const bool pDown =
                keyboard.IsDown(Keyboard::KeyP);

            if (pDown && !previousPDown)
                mixer.TogglePlayback();

            previousPDown = pDown;

            // Up increases volume once per key press.
            const bool upDown =
                keyboard.IsDown(Keyboard::KeyUp);

            if (upDown && !previousUpDown)
            {
                volume += VolumeStep;

                if (volume > MaximumVolume)
                    volume = MaximumVolume;

                // The current API has no volume-only event. Replacing the
                // voice applies the new volume and restarts the WAV.
                if (!ScheduleWavAtFrame(
                        mixer,
                        wav,
                        volume,
                        mixer.GetMixedFramePosition()))
                {
                    fprintf(
                        stderr,
                        "SOUND05: could not change volume.\n");

                    result = 1;
                    break;
                }
            }

            previousUpDown = upDown;

            // Down decreases volume once per key press.
            const bool downDown =
                keyboard.IsDown(Keyboard::KeyDown);

            if (downDown && !previousDownDown)
            {
                volume -= VolumeStep;

                if (volume < 0)
                    volume = 0;

                if (!ScheduleWavAtFrame(
                        mixer,
                        wav,
                        volume,
                        mixer.GetMixedFramePosition()))
                {
                    fprintf(
                        stderr,
                        "SOUND05: could not change volume.\n");

                    result = 1;
                    break;
                }
            }

            previousDownDown = downDown;

            // Convert accumulated playback time to source sample frames.
            // Splitting seconds and remainder avoids multiplying the
            // complete nanosecond counter by the sample rate.
            const uint64_t elapsedSeconds =
                playbackElapsedNs / 1000000000ULL;

            const uint64_t elapsedRemainder =
                playbackElapsedNs % 1000000000ULL;

            const uint64_t playbackFrame =
                (elapsedSeconds *
                    static_cast<uint64_t>(wav.GetSampleRate()) +
                 elapsedRemainder *
                    static_cast<uint64_t>(wav.GetSampleRate()) /
                    1000000000ULL)
                % wav.GetSampleFrames();

            DrawFrame(
                display,
                wav,
                waveform,
                mixer,
                playbackFrame,
                volume,
                static_cast<unsigned long>(
                    playbackElapsedNs / 16666667ULL),
                fpsCounter.CurrentFPS);

            if (!display.Present())
            {
                fprintf(
                    stderr,
                    "SOUND05: display presentation failed.\n");

                result = 1;
                break;
            }

            fpsCounter.FramePresented(
                GetMonotonicNanoseconds());

            if (sound.WorkerFailed())
            {
                fprintf(
                    stderr,
                    "SOUND05: audio worker failed.\n");

                result = 1;
                break;
            }
        }
    }

cleanup:
    // Stop the worker before destroying the mixer or releasing WAV samples.
    if (soundInitialized)
        sound.Shutdown();

    if (keyboardInitialized)
        keyboard.Shutdown();

    if (displayInitialized)
        display.Shutdown();

    // wav is destroyed after RunGame returns, after the audio worker stops.
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
                "SOUND05: invalid sample rate.\n");

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
                "SOUND05: channels must be 1 or 2.\n");

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
