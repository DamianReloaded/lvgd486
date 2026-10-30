// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// LVGD486 - Doom sound backend
// Uses the native VGD Sound, AudioMixer and MIDI libraries.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include "../../vgd/Sound/Sound.hpp"
#include "../../vgd/Sound/AudioMixer.hpp"
#include "../../vgd/Sound/Midi.hpp"
#include "../../vgd/Sound/MidiSong.hpp"
#include "../../vgd/Sound/MidiPlayer.hpp"
#include "../../vgd/Sound/MusFile.hpp"

extern "C"
{
#include "z_zone.h"
#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "doomdef.h"
#include "sounds.h"
}

enum
{
    DoomChannelCount = 8,
    DoomSoundHeaderSize = 8,
    DoomDefaultSampleRate = 11025,
    DoomMusicSongCount = 16
};

static Sound SoundOutput;
static vgd::AudioMixer Mixer;

static bool SoundInitialized = false;

static int SoundStepTable[256];

struct DoomSoundSample
{
    int16_t* Samples;
    uint64_t SampleFrames;
    int SampleRate;

    DoomSoundSample()
        : Samples(NULL),
          SampleFrames(0),
          SampleRate(0)
    {
    }
};

struct DoomSoundChannel
{
    int Handle;
    int SoundId;
    int StartTic;

    uint64_t EndFrame;

    int Volume;
    int Separation;
    int Pitch;
    int SourceRate;

    bool Active;
    bool Loop;

    DoomSoundChannel()
        : Handle(0),
          SoundId(-1),
          StartTic(0),
          EndFrame(0),
          Volume(0),
          Separation(128),
          Pitch(128),
          SourceRate(DoomDefaultSampleRate),
          Active(false),
          Loop(false)
    {
    }
};

struct DoomMusicSong
{
    int Handle;
    bool Used;
    vgd::MidiSong Song;

    DoomMusicSong()
        : Handle(0),
          Used(false)
    {
    }
};

static DoomSoundSample SoundSamples[NUMSFX];
static DoomSoundChannel SoundChannels[DoomChannelCount];

static unsigned int NextSoundHandle = 100;

static vgd::Midi MusicOutput;
static vgd::MidiPlayer MusicPlayer;

static DoomMusicSong MusicSongs[DoomMusicSongCount];

static bool MusicInitialized = false;
static bool MusicPlaybackActive = false;
static bool MusicPlaybackLooping = false;

static int MusicCurrentHandle = 0;
static int MusicNextHandle = 1;

static double MusicStartTime = 0.0;
static double MusicDuration = 0.0;


//-----------------------------------------------------------------------------
//
// Sound sample management
//
//-----------------------------------------------------------------------------

static int ReadLittleEndian16(const unsigned char* data)
{
    return static_cast<int>(data[0]) |
           (static_cast<int>(data[1]) << 8);
}

static uint32_t ReadLittleEndian32(const unsigned char* data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

static bool LoadSoundSample(int soundId)
{
    if (soundId <= 0 || soundId >= NUMSFX)
    {
        return false;
    }

    if (SoundSamples[soundId].Samples)
    {
        return true;
    }

    char lumpName[9];
    snprintf(lumpName, sizeof(lumpName), "ds%.6s", S_sfx[soundId].name);

    int lumpNumber;

    if (W_CheckNumForName(lumpName) == -1)
    {
        lumpNumber = W_GetNumForName("dspistol");
    }
    else
    {
        lumpNumber = W_GetNumForName(lumpName);
    }

    int lumpLength = W_LumpLength(lumpNumber);

    if (lumpLength <= DoomSoundHeaderSize)
    {
        return false;
    }

    unsigned char* lumpData =
        static_cast<unsigned char*>(W_CacheLumpNum(lumpNumber, PU_STATIC));

    if (!lumpData)
    {
        return false;
    }

    int sampleRate = ReadLittleEndian16(lumpData + 2);
    uint32_t sampleFrames = ReadLittleEndian32(lumpData + 4);

    uint32_t availableFrames =
        static_cast<uint32_t>(lumpLength - DoomSoundHeaderSize);

    if (sampleFrames == 0 || sampleFrames > availableFrames)
    {
        sampleFrames = availableFrames;
    }

    if (sampleRate < 4000 ||
        sampleRate > 192000 ||
        sampleFrames == 0 ||
        sampleFrames > 0x7FFFFFFFUL / sizeof(int16_t))
    {
        Z_Free(lumpData);
        return false;
    }

    int16_t* samples = new int16_t[sampleFrames];

    const unsigned char* source = lumpData + DoomSoundHeaderSize;

    for (uint32_t i = 0; i < sampleFrames; ++i)
    {
        samples[i] = static_cast<int16_t>(
            (static_cast<int>(source[i]) - 128) * 256);
    }

    Z_Free(lumpData);

    SoundSamples[soundId].Samples = samples;
    SoundSamples[soundId].SampleFrames = sampleFrames;
    SoundSamples[soundId].SampleRate = sampleRate;

    S_sfx[soundId].data = samples;

    return true;
}

static int GetSoundLinkIndex(int soundId)
{
    if (!S_sfx[soundId].link)
    {
        return -1;
    }

    ptrdiff_t index = S_sfx[soundId].link - S_sfx;

    if (index < 0 || index >= NUMSFX)
    {
        return -1;
    }

    return static_cast<int>(index);
}

static bool LoadAllSoundSamples()
{
    for (int i = 1; i < NUMSFX; ++i)
    {
        if (!S_sfx[i].link)
        {
            if (!LoadSoundSample(i))
            {
                fprintf(stderr, "Doom sound: failed to load ds%s\n",
                        S_sfx[i].name);
                return false;
            }
        }
    }

    for (int i = 1; i < NUMSFX; ++i)
    {
        if (!S_sfx[i].link)
        {
            continue;
        }

        int linkIndex = GetSoundLinkIndex(i);

        if (linkIndex < 0)
        {
            fprintf(stderr, "Doom sound: invalid sound link for %s\n",
                    S_sfx[i].name);
            return false;
        }

        while (S_sfx[linkIndex].link)
        {
            linkIndex = GetSoundLinkIndex(linkIndex);

            if (linkIndex < 0)
            {
                return false;
            }
        }

        if (!SoundSamples[linkIndex].Samples)
        {
            return false;
        }

        SoundSamples[i] = SoundSamples[linkIndex];
        S_sfx[i].data = SoundSamples[i].Samples;
    }

    return true;
}

static void FreeSoundSamples()
{
    for (int i = 1; i < NUMSFX; ++i)
    {
        if (S_sfx[i].link)
        {
            SoundSamples[i] = DoomSoundSample();
            S_sfx[i].data = NULL;
            continue;
        }

        delete[] SoundSamples[i].Samples;

        SoundSamples[i] = DoomSoundSample();
        S_sfx[i].data = NULL;
    }
}


//-----------------------------------------------------------------------------
//
// Channel management
//
//-----------------------------------------------------------------------------

static void StopChannel(int channel)
{
    if (channel < 0 || channel >= DoomChannelCount)
    {
        return;
    }

    DoomSoundChannel& state = SoundChannels[channel];

    if (state.Active && SoundInitialized)
    {
        Mixer.ScheduleStop(Mixer.GetMixedFramePosition(), channel);
    }

    state.Active = false;
    state.SoundId = -1;
    state.Handle = 0;
}

static bool IsChannelPlaying(int channel, uint64_t currentFrame)
{
    DoomSoundChannel& state = SoundChannels[channel];

    if (!state.Active)
    {
        return false;
    }

    if (!state.Loop && currentFrame >= state.EndFrame)
    {
        state.Active = false;
        state.SoundId = -1;
        state.Handle = 0;
        return false;
    }

    return true;
}

static void CalculateStereoVolume(
    int volume,
    int separation,
    int& leftVolume,
    int& rightVolume)
{
    if (volume < 0)
    {
        volume = 0;
    }
    else if (volume > 127)
    {
        volume = 127;
    }

    if (separation < 0)
    {
        separation = 0;
    }
    else if (separation > 255)
    {
        separation = 255;
    }

    separation += 1;

    leftVolume =
        volume - ((volume * separation * separation) >> 16);

    separation -= 257;

    rightVolume =
        volume - ((volume * separation * separation) >> 16);

    if (leftVolume < 0)
    {
        leftVolume = 0;
    }

    if (rightVolume < 0)
    {
        rightVolume = 0;
    }

    if (leftVolume > 127)
    {
        leftVolume = 127;
    }

    if (rightVolume > 127)
    {
        rightVolume = 127;
    }
}

static int ConvertDoomVolume(int volume)
{
    if (volume < 0)
    {
        volume = 0;
    }
    else if (volume > 15)
    {
        volume = 15;
    }

    return (volume * 32767) / 15;
}

static int CalculatePlaybackRate(int soundId, int pitch)
{
    if (pitch < 0)
    {
        pitch = 0;
    }
    else if (pitch > 255)
    {
        pitch = 255;
    }

    int step = SoundStepTable[pitch];

    if (step <= 0)
    {
        step = 65536;
    }

    int sampleRate = SoundSamples[soundId].SampleRate;

    int64_t playbackRate =
        (static_cast<int64_t>(sampleRate) * step + 32768) / 65536;

    if (playbackRate < 4000)
    {
        playbackRate = 4000;
    }
    else if (playbackRate > 192000)
    {
        playbackRate = 192000;
    }

    return static_cast<int>(playbackRate);
}

static int FindSoundChannel(int soundId, uint64_t currentFrame)
{
    if (soundId == sfx_sawup ||
        soundId == sfx_sawidl ||
        soundId == sfx_sawful ||
        soundId == sfx_sawhit ||
        soundId == sfx_stnmov ||
        soundId == sfx_pistol)
    {
        for (int i = 0; i < DoomChannelCount; ++i)
        {
            if (IsChannelPlaying(i, currentFrame) &&
                SoundChannels[i].SoundId == soundId)
            {
                StopChannel(i);
                break;
            }
        }
    }

    int oldestChannel = 0;
    int oldestTic = gametic;

    for (int i = 0; i < DoomChannelCount; ++i)
    {
        if (!IsChannelPlaying(i, currentFrame))
        {
            return i;
        }

        if (SoundChannels[i].StartTic < oldestTic)
        {
            oldestTic = SoundChannels[i].StartTic;
            oldestChannel = i;
        }
    }

    return oldestChannel;
}

static int StartSoundOnChannel(
    int soundId,
    int volume,
    int separation,
    int pitch)
{
    if (!SoundInitialized ||
        soundId <= 0 ||
        soundId >= NUMSFX ||
        !SoundSamples[soundId].Samples)
    {
        return -1;
    }

    uint64_t currentFrame = Mixer.GetMixedFramePosition();

    int channel = FindSoundChannel(soundId, currentFrame);

    StopChannel(channel);

    int leftVolume;
    int rightVolume;

    CalculateStereoVolume(
        volume,
        separation,
        leftVolume,
        rightVolume);

    int playbackRate = CalculatePlaybackRate(soundId, pitch);

    int mixerLeftVolume = ConvertDoomVolume(leftVolume);
    int mixerRightVolume = ConvertDoomVolume(rightVolume);

    DoomSoundSample& sample = SoundSamples[soundId];

    if (!Mixer.SchedulePlay(
            currentFrame,
            channel,
            sample.Samples,
            sample.SampleFrames,
            playbackRate,
            mixerLeftVolume,
            mixerRightVolume,
            false))
    {
        return -1;
    }

    unsigned int handle = NextSoundHandle++;

    if (NextSoundHandle < 100)
    {
        NextSoundHandle = 100;
    }

    uint64_t durationFrames =
        (sample.SampleFrames * SoundOutput.GetSampleRate() +
         static_cast<uint64_t>(playbackRate) - 1) /
        static_cast<uint64_t>(playbackRate);

    DoomSoundChannel& state = SoundChannels[channel];

    state.Handle = static_cast<int>(handle);
    state.SoundId = soundId;
    state.StartTic = gametic;
    state.EndFrame = currentFrame + durationFrames;
    state.Volume = volume;
    state.Separation = separation;
    state.Pitch = pitch;
    state.SourceRate = playbackRate;
    state.Active = true;
    state.Loop = false;

    return state.Handle;
}


//-----------------------------------------------------------------------------
//
// Music management
//
//-----------------------------------------------------------------------------

static double MusicClockSeconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (double)tv.tv_sec +
           (double)tv.tv_usec / 1000000.0;
}

static DoomMusicSong* FindMusicSong(int handle)
{
    for (int i = 0; i < DoomMusicSongCount; ++i)
    {
        if (MusicSongs[i].Used &&
            MusicSongs[i].Handle == handle)
        {
            return &MusicSongs[i];
        }
    }

    return NULL;
}

static DoomMusicSong* AllocateMusicSong()
{
    for (int i = 0; i < DoomMusicSongCount; ++i)
    {
        if (!MusicSongs[i].Used)
        {
            return &MusicSongs[i];
        }
    }

    return NULL;
}

static void ApplyMusicVolume()
{
    if (!MusicInitialized)
    {
        return;
    }

    int volume = snd_MusicVolume;

    if (volume < 0)
    {
        volume = 0;
    }
    else if (volume > 127)
    {
        volume = 127;
    }

    for (int channel = 0; channel < 16; ++channel)
    {
        MusicOutput.ControlChange(channel, 7, volume);
    }
}

static void StopMusicPlayback()
{
    if (MusicInitialized)
    {
        MusicPlayer.Stop();
    }

    MusicPlaybackActive = false;
    MusicPlaybackLooping = false;
    MusicCurrentHandle = 0;
    MusicStartTime = 0.0;
    MusicDuration = 0.0;
}


//-----------------------------------------------------------------------------
//
// Doom sound API
//
//-----------------------------------------------------------------------------

void I_SetChannels()
{
    for (int i = -128; i < 128; ++i)
    {
        SoundStepTable[i + 128] =
            static_cast<int>(pow(2.0, i / 64.0) * 65536.0);
    }

    for (int i = 0; i < DoomChannelCount; ++i)
    {
        SoundChannels[i] = DoomSoundChannel();
    }
}

void I_SetSfxVolume(int volume)
{
    snd_SfxVolume = volume;
}

void I_SetMusicVolume(int volume)
{
    snd_MusicVolume = volume;
    ApplyMusicVolume();
}

int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];

    snprintf(namebuf, sizeof(namebuf), "ds%.6s", sfx->name);

    if (W_CheckNumForName(namebuf) == -1)
    {
        return W_GetNumForName("dspistol");
    }

    return W_GetNumForName(namebuf);
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    (void)priority;

    return StartSoundOnChannel(id, vol, sep, pitch);
}

void I_StopSound(int handle)
{
    if (handle == 0 || !SoundInitialized)
    {
        return;
    }

    for (int i = 0; i < DoomChannelCount; ++i)
    {
        if (SoundChannels[i].Active &&
            SoundChannels[i].Handle == handle)
        {
            StopChannel(i);
            return;
        }
    }
}

int I_SoundIsPlaying(int handle)
{
    if (handle == 0 || !SoundInitialized)
    {
        return 0;
    }

    uint64_t currentFrame = Mixer.GetMixedFramePosition();

    for (int i = 0; i < DoomChannelCount; ++i)
    {
        if (SoundChannels[i].Handle == handle &&
            IsChannelPlaying(i, currentFrame))
        {
            return 1;
        }
    }

    return 0;
}

void I_UpdateSound(void)
{
    // AudioMixer performs mixing on the Sound worker thread.
}

void I_SubmitSound(void)
{
    // Sound handles output asynchronously.
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    if (!SoundInitialized || handle == 0)
    {
        return;
    }

    for (int i = 0; i < DoomChannelCount; ++i)
    {
        DoomSoundChannel& state = SoundChannels[i];

        if (!state.Active || state.Handle != handle)
        {
            continue;
        }

        uint64_t currentFrame = Mixer.GetMixedFramePosition();

        if (!IsChannelPlaying(i, currentFrame))
        {
            return;
        }

        int soundId = state.SoundId;

        if (soundId <= 0 ||
            soundId >= NUMSFX ||
            !SoundSamples[soundId].Samples)
        {
            return;
        }

        StopChannel(i);

        int leftVolume;
        int rightVolume;

        CalculateStereoVolume(vol, sep, leftVolume, rightVolume);

        int playbackRate = CalculatePlaybackRate(soundId, pitch);

        DoomSoundSample& sample = SoundSamples[soundId];

        if (!Mixer.SchedulePlay(
                currentFrame,
                i,
                sample.Samples,
                sample.SampleFrames,
                playbackRate,
                ConvertDoomVolume(leftVolume),
                ConvertDoomVolume(rightVolume),
                false))
        {
            return;
        }

        uint64_t durationFrames =
            (sample.SampleFrames * SoundOutput.GetSampleRate() +
             static_cast<uint64_t>(playbackRate) - 1) /
            static_cast<uint64_t>(playbackRate);

        state.Handle = handle;
        state.SoundId = soundId;
        state.StartTic = gametic;
        state.EndFrame = currentFrame + durationFrames;
        state.Volume = vol;
        state.Separation = sep;
        state.Pitch = pitch;
        state.SourceRate = playbackRate;
        state.Active = true;

        return;
    }
}


//-----------------------------------------------------------------------------
//
// Initialization and shutdown
//
//-----------------------------------------------------------------------------

void I_InitSound()
{
    if (SoundInitialized)
    {
        return;
    }

    Sound::Configuration configuration;

    configuration.SampleRate = DoomDefaultSampleRate;
    configuration.Channels = 1;
    configuration.BufferFrames = 512;

    if (!SoundOutput.Initialize(configuration))
    {
        I_Error("I_InitSound: could not initialize audio device");
        return;
    }

    if (!Mixer.Initialize(SoundOutput.GetSampleRate(), 1))
    {
        SoundOutput.Shutdown();

        I_Error("I_InitSound: could not initialize audio mixer");
        return;
    }

    if (!Mixer.Attach(SoundOutput))
    {
        SoundOutput.Shutdown();

        I_Error("I_InitSound: could not attach audio mixer");
        return;
    }

    I_SetChannels();

    if (!LoadAllSoundSamples())
    {
        SoundOutput.Shutdown();
        FreeSoundSamples();

        I_Error("I_InitSound: could not load WAD sound effects");
        return;
    }

    Mixer.Play();

    if (!SoundOutput.StartWorker())
    {
        SoundOutput.Shutdown();
        FreeSoundSamples();

        I_Error("I_InitSound: could not start audio worker");
        return;
    }

    SoundInitialized = true;

    fprintf(stderr,
            "I_InitSound: audio initialized (%d Hz, %d channel)\n",
            SoundOutput.GetSampleRate(),
            SoundOutput.GetChannels());
}

void I_ShutdownSound(void)
{
    if (!SoundInitialized)
    {
        return;
    }

    SoundOutput.Shutdown();

    for (int i = 0; i < DoomChannelCount; ++i)
    {
        SoundChannels[i].Active = false;
        SoundChannels[i].Handle = 0;
        SoundChannels[i].SoundId = -1;
    }

    FreeSoundSamples();

    SoundInitialized = false;
}


//-----------------------------------------------------------------------------
//
// Music API
//
//-----------------------------------------------------------------------------

void I_InitMusic(void)
{
    fprintf(stderr, "Doom music: I_InitMusic called\n");

    if (MusicInitialized)
    {
        fprintf(stderr, "Doom music: already initialized\n");
        return;
    }

    if (!MusicOutput.Initialize("/dev/sound/sequencer"))
    {
        fprintf(stderr, "Doom music: MIDI initialization failed\n");
        return;
    }

    fprintf(stderr, "Doom music: MIDI device initialized\n");

    if (!MusicPlayer.Initialize(&MusicOutput))
    {
        fprintf(stderr, "Doom music: MIDI player initialization failed\n");
        MusicOutput.Shutdown();
        return;
    }

    MusicInitialized = true;
    MusicPlaybackActive = false;
    MusicPlaybackLooping = false;
    MusicCurrentHandle = 0;

    fprintf(stderr, "Doom music: initialization complete\n");
}

void I_ShutdownMusic(void)
{
    if (!MusicInitialized)
    {
        return;
    }

    StopMusicPlayback();

    MusicPlayer.Shutdown();
    MusicOutput.Shutdown();

    for (int i = 0; i < DoomMusicSongCount; ++i)
    {
        MusicSongs[i].Song.Clear();
        MusicSongs[i].Handle = 0;
        MusicSongs[i].Used = false;
    }

    MusicInitialized = false;

    fprintf(stderr, "Doom music: MIDI output shut down\n");
}

int I_RegisterSong(void* data)
{
    fprintf(stderr, "Doom music: I_RegisterSong called, data=%p initialized=%d\n",
            data, MusicInitialized ? 1 : 0);

    if (!data || !MusicInitialized)
    {
        return 0;
    }

    const unsigned char* musData =
        static_cast<const unsigned char*>(data);

    fprintf(stderr, "Doom music: signature %02X %02X %02X %02X\n",
            musData[0], musData[1], musData[2], musData[3]);

    if (musData[0] != 'M' ||
        musData[1] != 'U' ||
        musData[2] != 'S' ||
        musData[3] != 0x1A)
    {
        fprintf(stderr, "Doom music: invalid MUS signature\n");
        return 0;
    }

    unsigned int scoreLength =
        static_cast<unsigned int>(musData[4]) |
        (static_cast<unsigned int>(musData[5]) << 8);

    unsigned int scoreStart =
        static_cast<unsigned int>(musData[6]) |
        (static_cast<unsigned int>(musData[7]) << 8);

    fprintf(stderr, "Doom music: scoreLength=%u scoreStart=%u\n",
            scoreLength, scoreStart);

    if (scoreStart < 16 ||
        scoreLength == 0 ||
        scoreStart > UINT_MAX - scoreLength)
    {
        fprintf(stderr, "Doom music: invalid MUS header\n");
        return 0;
    }

    unsigned int musSize = scoreStart + scoreLength;

    DoomMusicSong* entry = AllocateMusicSong();

    if (!entry)
    {
        fprintf(stderr, "Doom music: song registry is full\n");
        return 0;
    }

    vgd::MusFile loader;

    if (!loader.Load(musData, musSize, entry->Song))
    {
        fprintf(stderr, "Doom music: MUS conversion failed: %s\n",
                loader.GetError());
        entry->Song.Clear();
        return 0;
    }

    fprintf(stderr,
            "Doom music: converted song: %d events, %u ticks, %.2f seconds\n",
            entry->Song.GetMidiEventCount(),
            entry->Song.GetLengthTicks(),
            entry->Song.GetDurationSeconds());

    if (entry->Song.GetMidiEventCount() == 0)
    {
        fprintf(stderr, "Doom music: converted song contains no events\n");
        entry->Song.Clear();
        return 0;
    }

    int handle = MusicNextHandle++;

    if (MusicNextHandle <= 0)
    {
        MusicNextHandle = 1;
    }

    while (FindMusicSong(handle))
    {
        handle = MusicNextHandle++;

        if (MusicNextHandle <= 0)
        {
            MusicNextHandle = 1;
        }
    }

    entry->Handle = handle;
    entry->Used = true;

    fprintf(stderr, "Doom music: registered handle %d\n", handle);

    return handle;
}

void I_PlaySong(int handle, int looping)
{
    fprintf(stderr, "Doom music: I_PlaySong handle=%d looping=%d\n",
            handle, looping);

    if (!MusicInitialized)
    {
        fprintf(stderr, "Doom music: not initialized\n");
        return;
    }

    DoomMusicSong* entry = FindMusicSong(handle);

    if (!entry)
    {
        fprintf(stderr, "Doom music: handle %d not found\n", handle);
        return;
    }

    StopMusicPlayback();

    if (!entry->Song.SetLooping(looping != 0))
    {
        fprintf(stderr, "Doom music: SetLooping failed\n");
        return;
    }

    if (!MusicPlayer.Play(entry->Song))
    {
        fprintf(stderr, "Doom music: MidiPlayer::Play failed\n");
        return;
    }

    MusicCurrentHandle = handle;
    MusicPlaybackActive = true;
    MusicPlaybackLooping = looping != 0;
    MusicStartTime = MusicPlayer.GetStartTime();
    MusicDuration = entry->Song.GetDurationSeconds();

    fprintf(stderr,
            "Doom music: playback started, duration=%.2f seconds\n",
            MusicDuration);
}

void I_PauseSong(int handle)
{
    (void)handle;

    // MidiPlayer currently has no position-preserving pause operation.
    // Playback continues until pause support is implemented.
}

void I_ResumeSong(int handle)
{
    (void)handle;

    // I_PauseSong does not stop the player.
}

void I_StopSong(int handle)
{
    if (handle != 0 && handle != MusicCurrentHandle)
    {
        return;
    }

    StopMusicPlayback();
}

void I_UnRegisterSong(int handle)
{
    DoomMusicSong* entry = FindMusicSong(handle);

    if (!entry)
    {
        return;
    }

    if (MusicCurrentHandle == handle)
    {
        StopMusicPlayback();
    }

    entry->Song.Clear();
    entry->Handle = 0;
    entry->Used = false;
}

int I_QrySongPlaying(int handle)
{
    if (!MusicInitialized ||
        !MusicPlaybackActive ||
        (handle != 0 && handle != MusicCurrentHandle) ||
        MusicPlayer.HasError())
    {
        return 0;
    }

    if (MusicPlaybackLooping)
    {
        return 1;
    }

    double elapsed = MusicClockSeconds() - MusicStartTime;

    if (elapsed >= MusicDuration)
    {
        MusicPlaybackActive = false;
        return 0;
    }

    return 1;
}