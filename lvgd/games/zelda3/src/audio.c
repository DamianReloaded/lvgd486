#include "audio.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "zelda_features.h"
#include "../snes/snes_regs.h"
#include "spc_player.h"
#include "config.h"
#include "assets.h"

/*
 * Audio is intentionally disabled for the initial native port.
 *
 * The original MSU, Opus, SPC and APU implementation is preserved
 * in audio_original.c for later review and reimplementation.
 *
 * No Opus or audio backend dependencies should be required here.
 */

void zelda_apu_write(uint32_t adr, uint8_t val)
{
    (void)adr;
    (void)val;
}

uint8_t zelda_read_apui00(void)
{
    return 0;
}

uint8_t zelda_apu_read(uint32_t adr)
{
    (void)adr;
    return 0;
}

void ZeldaPushApuState(void)
{
}

void ZeldaDiscardUnusedAudioFrames(void)
{
}

void ZeldaRenderAudio(int16 *audio_buffer, int samples, int channels)
{
    (void)audio_buffer;
    (void)samples;
    (void)channels;
}

bool ZeldaIsMusicPlaying(void)
{
    return false;
}

bool ZeldaIsPlayingMusicTrack(uint8 track)
{
    (void)track;
    return false;
}

bool ZeldaIsPlayingMusicTrackWithBug(uint8 track)
{
    (void)track;
    return false;
}

uint8 ZeldaGetEntranceMusicTrack(int i)
{
    (void)i;
    return 0;
}

void ZeldaPlayMsuAudioTrack(uint8 music_ctrl)
{
    (void)music_ctrl;
}

void ZeldaRestoreMusicAfterLoad_Locked(bool is_reset)
{
    (void)is_reset;
}

void ZeldaSaveMusicStateToRam_Locked(void)
{
}

void ZeldaEnableMsu(uint8 enable)
{
    (void)enable;
}

void LoadSongBank(const uint8 *p)
{
    (void)p;
}