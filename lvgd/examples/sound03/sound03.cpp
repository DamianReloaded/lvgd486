#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

using namespace vgd;

static volatile sig_atomic_t StopRequested = 0;

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    StopRequested = 1;
}

static const BMPFont FontChipfusion =
{
    Font01::Width, Font01::Height,
    Font01::FirstDigit, Font01::DigitCount,
    Font01::FirstUpper, Font01::UpperCount,
    Font01::GlyphSpace,
    &Font01::GlyphDigits[0][0],
    &Font01::GlyphUpper[0][0]
};

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
static const int ChannelCount = 4;
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

struct AudioChannel
{
    const int* Notes;
    uint32_t Phase;
    uint32_t PhaseIncrement;
    int Frequency;
    int Volume;
};

struct AudioMixer
{
    int SampleRate;
    bool Playing;

    AudioChannel Channels[ChannelCount];

    uint64_t SongFrame;

    volatile unsigned long RenderedFrames;
    volatile int Levels[ChannelCount];
    volatile int CurrentStep;

    uint32_t NoiseState;
};

static uint64_t GetFramesPerStep(const AudioMixer& mixer)
{
    return (static_cast<uint64_t>(mixer.SampleRate) * TempoMs) / 1000U;
}

static int GetChannelNote(const AudioMixer&, int channel, int step)
{
    if (step < 0 || step >= NoteCount)
        return 0;

    if (channel == 0)
        return NotesLead[step];

    if (channel == 1)
        return NotesHarmony[step];

    if (channel == 2)
        return NotesBass[step];

    return NotesPercussion[step] ? 1 : 0;
}

static void UpdateChannel(AudioMixer& mixer, int channel, int step)
{
    AudioChannel& voice = mixer.Channels[channel];
    const int frequency = GetChannelNote(mixer, channel, step);

    voice.Frequency = frequency;

    if (channel == 3 || frequency <= 0)
    {
        voice.PhaseIncrement = 0;
        return;
    }

    voice.PhaseIncrement = static_cast<uint32_t>(
        (static_cast<uint64_t>(frequency) << 32) /
        static_cast<unsigned int>(mixer.SampleRate));
}

static void MixAudio(void* context, int16_t* output, int frames)
{
    AudioMixer* mixer = static_cast<AudioMixer*>(context);
    const uint64_t framesPerStep = GetFramesPerStep(*mixer);

    if (framesPerStep == 0)
    {
        memset(output, 0, static_cast<size_t>(frames) * sizeof(int16_t));
        return;
    }

    for (int frame = 0; frame < frames; ++frame)
    {
        int mixed = 0;
        uint64_t position = 0;

        if (mixer->Playing)
        {
            position = mixer->SongFrame;

            const int step = static_cast<int>(
                (position / framesPerStep) % NoteCount);

            if (step != mixer->CurrentStep)
            {
                mixer->CurrentStep = step;

                for (int channel = 0; channel < ChannelCount; ++channel)
                    UpdateChannel(*mixer, channel, step);
            }

            for (int channel = 0; channel < 3; ++channel)
            {
                AudioChannel& voice = mixer->Channels[channel];

                if (voice.PhaseIncrement != 0)
                {
                    const int sample =
                        (voice.Phase & 0x80000000U)
                        ? -voice.Volume : voice.Volume;

                    mixed += sample;
                    voice.Phase += voice.PhaseIncrement;
                }
            }

            if (NotesPercussion[mixer->CurrentStep])
            {
                mixer->NoiseState =
                    mixer->NoiseState * 1664525U + 1013904223U;

                const int noise = (mixer->NoiseState & 0x80000000U)
                    ? 1 : -1;

                const uint64_t frameInStep = position % framesPerStep;

                const int decay = static_cast<int>(
                    frameInStep * 5000U / framesPerStep);

                mixed += noise * (6000 - decay);
            }

            ++mixer->SongFrame;
        }

        if (mixed > 30000)
            mixed = 30000;

        if (mixed < -30000)
            mixed = -30000;

        output[frame] = static_cast<int16_t>(mixed);
        ++mixer->RenderedFrames;
    }

    for (int channel = 0; channel < ChannelCount; ++channel)
    {
        const AudioChannel& voice = mixer->Channels[channel];

        mixer->Levels[channel] =
            mixer->Playing && voice.PhaseIncrement != 0
            ? voice.Volume : 0;
    }
}

static void DrawBackground(DisplayMode13h& display, unsigned long frame)
{
    uint8_t* buffer = display.GetBackBuffer();
    const int pitch = display.GetBackBufferPitch();

    for (int y = 0; y < display.GetHeight(); ++y)
    {
        uint8_t* row = buffer + y * pitch;
        const int band = ((y + static_cast<int>(frame / 3)) / 24) & 1;

        for (int x = 0; x < display.GetWidth(); ++x)
            row[x] = static_cast<uint8_t>(
                (((x / 32) + band) & 1) ? 1 : 0);
    }
}

static void DrawSpectrum(DisplayMode13h& display,
                         const AudioMixer& mixer,
                         unsigned long frame)
{
    const int startX =
        (display.GetWidth() - SpectrumBars * BarStep) / 2;
    const int baseY = 153;

    for (int i = 0; i < SpectrumBars; ++i)
    {
        uint32_t seed = static_cast<uint32_t>(
            i * 37 + mixer.CurrentStep * 19 + frame * (i + 3));

        seed ^= seed >> 7;
        seed *= 2654435761U;
        seed ^= seed >> 11;

        int height = 7 + static_cast<int>(seed % 48);

        if (i % 6 == 0)
            height += 14;

        if (!mixer.Playing)
            height /= 3;

        if (height > 68)
            height = 68;

        const int x = startX + i * BarStep;
        const int y = baseY - height;

        display.DrawRectangle(x, y, BarWidth, height, SpectrumColors[i]);

        if (height > 4)
            display.DrawRectangle(x, y, BarWidth, 2,
                SpectrumColors[(i + 8) % SpectrumBars]);
    }

    display.DrawRectangle(startX - 2, baseY + 1,
                          SpectrumBars * BarStep, 1, 8);
}

static void DrawFrame(DisplayMode13h& display,
                      const AudioMixer& mixer,
                      unsigned long frame)
{
    DrawBackground(display, frame);

    const int width = display.GetWidth();

    display.DrawRectangle(8, 8, width - 16, 40, 4);
    display.DrawRectangle(10, 10, width - 20, 36, 1);

    const char* title = "CHIPFUSION";
    const char* subtitle = "DIGITAL HORIZON";

    display.DrawText(FontChipfusion, title,
        (width - 10 * FontChipfusion.Width) / 2, 14,
        (frame / 8) % 2 ? 14 : 15);

    display.DrawText(FontChipfusion, subtitle,
        (width - 15 * FontChipfusion.Width) / 2, 30, 11);

    display.DrawText(FontChipfusion, "4 CHANNEL MIXER", 18, 58, 11);
    display.DrawText(FontChipfusion, "SPECTRUM", 18, 73, 14);

    DrawSpectrum(display, mixer, frame);

    static const char* ChannelNames[ChannelCount] =
    {
        "LEAD", "HARMONY", "BASS", "DRUMS"
    };

    for (int i = 0; i < ChannelCount; ++i)
    {
        const int x = 20 + i * 75;
        const int level = mixer.Playing
            ? (mixer.Levels[i] > 0 ? 8 : 2) : 1;

        display.DrawRectangle(x, 166, 9, 9, SpectrumColors[i * 5]);
        display.DrawText(FontChipfusion, ChannelNames[i], x + 12, 166, 11);
        display.DrawRectangle(x, 179, 62, 3, 8);
        display.DrawRectangle(x, 179, level * 7, 3, SpectrumColors[i * 5]);
    }

    display.DrawRectangle(12, 192, width - 24, 12, 4);

    display.DrawText(FontChipfusion,
        mixer.Playing ? "PLAYING" : "PAUSED", 18, 194,
        mixer.Playing ? 10 : 12);

    display.DrawText(FontChipfusion, "P PAUSE", 112, 194, 11);
    display.DrawText(FontChipfusion, "ESC EXIT", 222, 194, 15);

    const int progressWidth = width - 32;
    const int progress = mixer.CurrentStep * progressWidth / NoteCount;

    display.DrawRectangle(16, 211, progressWidth, 2, 8);

    if (progress > 0)
        display.DrawRectangle(16, 211, progress, 2, 10);
}

static int RunGame(DisplayMode13h& display,
                   const Sound::Configuration& configuration)
{
    Keyboard keyboard;
    Sound sound;

    bool displayInitialized = false;
    bool keyboardInitialized = false;
    bool soundInitialized = false;
    bool previousPDown = false;
    int result = 0;

    AudioMixer mixer;
    memset(&mixer, 0, sizeof(mixer));

    mixer.SampleRate = configuration.SampleRate;
    mixer.Playing = true;
    mixer.SongFrame = 0;
    mixer.NoiseState = 0x13579BDFU;
    mixer.CurrentStep = 0;

    mixer.Channels[0].Notes = NotesLead;
    mixer.Channels[0].Volume = 6500;

    mixer.Channels[1].Notes = NotesHarmony;
    mixer.Channels[1].Volume = 3500;

    mixer.Channels[2].Notes = NotesBass;
    mixer.Channels[2].Volume = 5000;

    mixer.Channels[3].Notes = NotesPercussion;
    mixer.Channels[3].Volume = 6000;

    for (int channel = 0; channel < ChannelCount; ++channel)
        UpdateChannel(mixer, channel, 0);

    if (!display.Initialize())
    {
        fprintf(stderr, "CHIPFUSION: display initialization failed.\n");
        display.Shutdown();
        return 1;
    }

    displayInitialized = true;

    if (!keyboard.Initialize())
    {
        fprintf(stderr, "CHIPFUSION: keyboard initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    keyboardInitialized = true;

    if (!sound.Initialize(configuration))
    {
        fprintf(stderr, "CHIPFUSION: sound initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    soundInitialized = true;

    if (!sound.SetPcmCallback(MixAudio, &mixer))
    {
        fprintf(stderr, "CHIPFUSION: PCM callback setup failed.\n");
        result = 1;
        goto cleanup;
    }

    if (!sound.StartWorker())
    {
        fprintf(stderr, "CHIPFUSION: audio worker could not start.\n");
        result = 1;
        goto cleanup;
    }

    {
        unsigned long frame = 0;

        while (!StopRequested)
        {
            keyboard.Update();

            if (!keyboard.IsConsoleActive())
                break;

            if (keyboard.IsDown(Keyboard::KeyEscape) ||
                ((keyboard.IsDown(Keyboard::KeyLeftCtrl) ||
                  keyboard.IsDown(Keyboard::KeyRightCtrl)) &&
                 keyboard.IsDown(Keyboard::KeyC)))
                break;

            const bool pDown = keyboard.IsDown(Keyboard::KeyP);

            if (pDown && !previousPDown)
                mixer.Playing = !mixer.Playing;

            previousPDown = pDown;

            DrawFrame(display, mixer, frame++);

            if (!display.Present())
            {
                fprintf(stderr, "CHIPFUSION: display presentation failed.\n");
                result = 1;
                break;
            }

            if (sound.WorkerFailed())
            {
                fprintf(stderr, "CHIPFUSION: audio worker failed.\n");
                result = 1;
                break;
            }
        }
    }

cleanup:
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

    if (argc > 3)
    {
        fprintf(stderr, "Usage: %s [sample_rate] [channels]\n", argv[0]);
        return 1;
    }

    if (argc >= 2)
    {
        char* end = NULL;
        const long rate = strtol(argv[1], &end, 10);

        if (!argv[1][0] || *end || rate < 4000 || rate > 192000)
        {
            fprintf(stderr, "CHIPFUSION: invalid sample rate.\n");
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
            fprintf(stderr, "CHIPFUSION: channels must be 1 or 2.\n");
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