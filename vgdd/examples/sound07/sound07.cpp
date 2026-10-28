#include "../../vgd/Display/DisplayMode13h.hpp"
#include "../../vgd/Input/Keyboard.hpp"
#include "../../vgd/Font/BmpFont.hpp"
#include "../../vgd/Font/BmpFont01.hpp"
#include "../../vgd/Sound/MidiSong.hpp"
#include "../../vgd/Sound/MidiFile.hpp"
#include "../../vgd/Sound/MidiPlayer.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

using namespace vgd;

static volatile sig_atomic_t StopRequested = 0;

static void HandleSignal(int n)
{
    (void)n;
    StopRequested = 1;
}

static double GetTimeSeconds()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

namespace
{
    enum { STEPS_BAR = 16 };
    enum { ROW_KICK, ROW_SNARE, ROW_HIHAT, ROW_BASS, ROW_PIANO, ROW_COUNT };

    static const char* RowNames[ROW_COUNT] = {
        "KICK", "SNARE", "HIHAT", "BASS", "PIANO"
    };

    static const int RowColors[ROW_COUNT] = {
        12, 14, 11, 10, 13
    };

    static const BMPFont DemoFont = {
        Font01::Width, Font01::Height,
        Font01::FirstDigit, Font01::DigitCount,
        Font01::FirstUpper, Font01::UpperCount,
        Font01::GlyphSpace,
        &Font01::GlyphDigits[0][0],
        &Font01::GlyphUpper[0][0]
    };

    static void DrawBackground(DisplayMode13h& d, unsigned long frame)
    {
        uint8_t* b = d.GetBackBuffer();
        const int pitch = d.GetBackBufferPitch();

        for (int y = 0; y < d.GetHeight(); ++y) {
            uint8_t* row = b + y * pitch;

            for (int x = 0; x < d.GetWidth(); ++x) {
                row[x] = (uint8_t)(
                    ((((x / 16) + (y / 16) +
                       (int)(frame / 8)) & 1) ? 1 : 0)
                );
            }
        }
    }

    static void DrawStepGrid(DisplayMode13h& d, uint64_t step)
    {
        const int sx = 67;
        const int sy = 57;
        const int cw = 14;
        const int ch = 12;
        const int gap = 1;
        const int rowGap = 18;

        const int active = (int)(step % STEPS_BAR);

        for (int row = 0; row < ROW_COUNT; ++row) {
            const int y = sy + row * rowGap;

            d.DrawText(DemoFont, RowNames[row], 7, y + 2,
                       RowColors[row]);

            for (int s = 0; s < STEPS_BAR; ++s) {
                const int x = sx + s * (cw + gap);
                int color = (s % 4 == 0) ? 8 : 1;

                /*
                 * Retain the original illustrative tracker pattern.
                 * This grid is a visual guide, not a decoded view
                 * of the loaded MIDI notes.
                 */
                if (row == ROW_KICK &&
                    (s == 0 || s == 8))
                    color = 12;

                if (row == ROW_SNARE &&
                    (s == 4 || s == 12))
                    color = 14;

                if (row == ROW_HIHAT && s % 2 == 0)
                    color = 11;

                if (row == ROW_BASS &&
                    (s == 2 || s == 6 || s == 10 || s == 14))
                    color = 10;

                if (row == ROW_PIANO && s % 4 == 0)
                    color = 13;

                d.DrawRectangle(x, y, cw, ch, color);

                if (s == active) {
                    d.DrawRectangle(
                        x - 1, y - 1, cw + 2, ch + 2, 15
                    );
                }
            }
        }
    }

    static void DrawFrame(DisplayMode13h& d,
                          const MidiSong& song,
                          double elapsed,
                          unsigned long frame,
                          int fps)
    {
        DrawBackground(d, frame);

        const double duration = song.GetDurationSeconds();
        const unsigned int lengthTicks = song.GetLengthTicks();
        const unsigned int ticksPerBeat = song.GetTicksPerBeat();

        double pos = 0.0;

        if (duration > 0.0) {
            pos = elapsed -
                  (double)((uint64_t)(elapsed / duration)) * duration;
        }

        if (pos < 0.0)
            pos = 0.0;

        /*
         * Estimate the current tick from elapsed song time.
         * This keeps the display responsive to arbitrary MIDI
         * lengths and tempo changes without changing the player.
         */
        uint64_t tick = 0;

        if (duration > 0.0 && lengthTicks > 0) {
            double fraction = pos / duration;

            if (fraction < 0.0)
                fraction = 0.0;

            if (fraction > 1.0)
                fraction = 1.0;

            tick = (uint64_t)(fraction * lengthTicks);
        }

        /*
         * The loader currently preserves PPQN timing and tempo
         * events, but does not expose MIDI time-signature events.
         * Use the conventional 4/4 assumption for the bar display.
         */
        const uint64_t ticksPerBar =
            (uint64_t)ticksPerBeat * 4ULL;

        const uint64_t totalBars =
            ticksPerBar > 0
                ? ((uint64_t)lengthTicks + ticksPerBar - 1) /
                      ticksPerBar
                : 1;

        const uint64_t bar =
            ticksPerBar > 0 ? tick / ticksPerBar + 1 : 1;

        /*
         * Map the estimated tick position to the original 16-step
         * visual grid. This is an approximation if tempo changes
         * occur during playback.
         */
        const uint64_t step =
            ticksPerBeat > 0
                ? (tick * STEPS_BAR) / ticksPerBar
                : 0;

        const int width = d.GetWidth();

        d.DrawRectangle(5, 4, width - 10, 24, 4);
        d.DrawRectangle(7, 6, width - 14, 20, 1);

        const char* title = "MIDI TRACKER - LOOP";

        d.DrawText(
            DemoFont, title,
            (width - (int)strlen(title) * DemoFont.Width) / 2,
            8, 15
        );

        char info[48];

        snprintf(info, sizeof(info), "%d BPM BAR %llu/%llu",
                 song.GetTempo(),
                 (unsigned long long)bar,
                 (unsigned long long)totalBars);

        d.DrawText(DemoFont, info, 7, 36, 11);

        char fpsText[16];

        snprintf(fpsText, sizeof(fpsText), "FPS %d", fps);

        int fx = width - (int)strlen(fpsText) *
                         DemoFont.Width - 7;

        const int minFx =
            7 + (int)strlen(info) * DemoFont.Width + 8;

        if (fx < minFx)
            fx = minFx;

        d.DrawText(
            DemoFont, fpsText, fx, 36,
            fps >= 55 ? 10 : (fps >= 30 ? 14 : 12)
        );

        DrawStepGrid(d, step);

        const int pw = width - 14;
        int progress = duration > 0.0
            ? (int)(pos * pw / duration)
            : 0;

        if (progress < 0)
            progress = 0;

        if (progress > pw)
            progress = pw;

        d.DrawRectangle(7, 153, pw, 3, 8);

        if (progress > 0)
            d.DrawRectangle(7, 153, progress, 3, 10);

        d.DrawText(DemoFont, "ESC TO EXIT", 7, 165, 14);
        d.DrawText(
            DemoFont, "WALL-CLOCK MIDI - GM DRUMS BASS PIANO",
            7, 181, 10
        );
    }
}

static int RunGame()
{
    DisplayMode13h display;
    Keyboard keyboard;
    MidiSong song;
    MidiFile midiFile;
    MidiPlayer player;
    Midi midi;

    bool displayReady = false;
    bool keyboardReady = false;

    int result = 0;
    int fps = 0;
    int fpsFrames = 0;

    unsigned long frame = 0;
    double fpsStart = 0.0;

    StopRequested = 0;

    /*
     * Load the Standard MIDI File into MidiSong.
     * The file is resolved relative to the current working directory.
     */
    if (!midiFile.Load("music.mid", song)) {
        fprintf(stderr, "SOUND07: could not load music.mid: %s\n",
                midiFile.GetError());
        return 1;
    }

    if (!song.SetLooping(true)) {
        fprintf(stderr, "SOUND07: could not enable MIDI looping.\n");
        return 1;
    }

    fprintf(stdout,
            "SOUND07: loaded music.mid "
            "(format %u, %u tracks, %u ticks/quarter, "
            "%d events, %.2f seconds, %d BPM)\n",
            midiFile.GetFormat(),
            midiFile.GetTrackCount(),
            midiFile.GetTicksPerQuarter(),
            song.GetMidiEventCount(),
            song.GetDurationSeconds(),
            song.GetTempo());

    if (!midi.Initialize("/dev/sound/sequencer")) {
        fprintf(stderr,
                "SOUND07: MIDI output initialization failed.\n");
        return 1;
    }

    if (!player.Initialize(&midi)) {
        fprintf(stderr,
                "SOUND07: MIDI player initialization failed.\n");
        return 1;
    }

    if (!display.Initialize()) {
        fprintf(stderr, "SOUND07: display initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    displayReady = true;

    if (!display.SetFrameRate(60)) {
        fprintf(stderr, "SOUND07: frame-rate setup failed.\n");
        result = 1;
        goto cleanup;
    }

    if (!keyboard.Initialize()) {
        fprintf(stderr, "SOUND07: keyboard initialization failed.\n");
        result = 1;
        goto cleanup;
    }

    keyboardReady = true;

    if (!player.Play(song)) {
        fprintf(stderr, "SOUND07: could not start playback.\n");
        result = 1;
        goto cleanup;
    }

    fpsStart = GetTimeSeconds();

    while (!StopRequested && !player.HasError()) {
        double now = GetTimeSeconds();

        keyboard.Update();

        if (!keyboard.IsConsoleActive() ||
            keyboard.IsDown(Keyboard::KeyEscape) ||
            ((keyboard.IsDown(Keyboard::KeyLeftCtrl) ||
              keyboard.IsDown(Keyboard::KeyRightCtrl)) &&
             keyboard.IsDown(Keyboard::KeyC))) {
            break;
        }

        DrawFrame(display, song,
                  now - player.GetStartTime(), frame, fps);

        if (!display.Present()) {
            fprintf(stderr,
                    "SOUND07: frame presentation failed.\n");
            result = 1;
            break;
        }

        ++frame;
        ++fpsFrames;

        if (now - fpsStart >= 1.0) {
            const double elapsed = now - fpsStart;

            fps = (int)(fpsFrames / elapsed + 0.5);

            fpsFrames = 0;
            fpsStart = now;
        }
    }

    if (player.HasError()) {
        fprintf(stderr, "SOUND07: MIDI playback error.\n");
        result = 1;
    }

cleanup:
    StopRequested = 1;
    player.Shutdown();

    midi.Shutdown(); 

    if (keyboardReady)
        keyboard.Shutdown();

    if (displayReady)
        display.Shutdown();

    return result;
}

int main(int argc, char* argv[])
{
    struct sigaction action;

    if (argc != 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        return 1;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = HandleSignal;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, 0);
    sigaction(SIGTERM, &action, 0);
    sigaction(SIGHUP, &action, 0);

    return RunGame();
}
