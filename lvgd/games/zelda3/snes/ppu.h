
#ifndef ZELDA3_SNES_PPU_H_
#define ZELDA3_SNES_PPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../snes/saveload.h"
#include "../src/types.h"

typedef struct Ppu Ppu;

enum
{
  PPU_RENDER_WIDTH = 320,
  PPU_RENDER_HEIGHT = 200,

  PPU_SOURCE_WIDTH = 256,
  PPU_SOURCE_HEIGHT = 224,
  PPU_SOURCE_HEIGHT_EXTENDED = 240,

  PPU_RENDER_BPP = 1
};

typedef struct BgLayer
{
  uint16_t hScroll;
  uint16_t vScroll;

  // -- snapshot starts here
  bool tilemapWider;
  bool tilemapHigher;
  uint16_t tilemapAdr;
  // -- snapshot ends here

  uint16_t tileAdr;
} BgLayer;

enum
{
  kPpuXPixels = PPU_SOURCE_WIDTH + kPpuExtraLeftRight * 2,
};

typedef uint16_t PpuZbufType;

typedef struct PpuPixelPrioBufs
{
  // Upper 8 bits: priority. Lower 8 bits: color.
  PpuZbufType data[kPpuXPixels];
} PpuPixelPrioBufs;

enum
{
  kPpuRenderFlags_NewRenderer = 1,
  // Render Mode 7 upsampled by 4x4.
  kPpuRenderFlags_4x4Mode7 = 2,
  // Use 240 source lines instead of 224.
  kPpuRenderFlags_Height240 = 4,
  // Disable sprite render limits.
  kPpuRenderFlags_NoSpriteLimits = 8,
};

struct Ppu
{
  bool lineHasSprites;
  uint8_t lastBrightnessMult;
  uint8_t lastMosaicModulo;
  uint8_t renderFlags;

  // Native indexed framebuffer.
  uint8_t *renderBuffer;
  size_t renderPitch;

  // Destination dimensions.
  uint16_t renderWidth;
  uint16_t renderHeight;

  // Source-to-destination viewport offsets.
  // Negative vertical offsets crop the SNES image.
  int16_t renderOffsetX;
  int16_t renderOffsetY;

  uint8_t extraLeftCur;
  uint8_t extraRightCur;
  uint8_t extraLeftRight;
  uint8_t extraBottomCur;

  float mode7PerspectiveLow;
  float mode7PerspectiveHigh;

  // TMW / TSW etc.
  uint8 screenEnabled[2];
  uint8 screenWindowed[2];
  uint8 mosaicEnabled;
  uint8 mosaicSize;

  // Objects / sprites.
  uint16_t objTileAdr1;
  uint16_t objTileAdr2;
  uint8_t objSize;

  // Window.
  uint8_t window1left;
  uint8_t window1right;
  uint8_t window2left;
  uint8_t window2right;
  uint32_t windowsel;

  // Color math.
  uint8_t clipMode;
  uint8_t preventMathMode;
  bool addSubscreen;
  bool subtractColor;
  bool halfColor;
  uint8 mathEnabled;
  uint8_t fixedColorR;
  uint8_t fixedColorG;
  uint8_t fixedColorB;

  // Settings.
  bool forcedBlank;
  uint8_t brightness;
  uint8_t mode;

  // VRAM access.
  uint16_t vramPointer;
  uint16_t vramIncrement;
  bool vramIncrementOnHigh;

  // CGRAM access.
  uint8_t cgramPointer;
  bool cgramSecondWrite;
  uint8_t cgramBuffer;

  // OAM access.
  uint16_t oamAdr;
  bool oamSecondWrite;
  uint8_t oamBuffer;

  // Background layers.
  BgLayer bgLayer[4];
  uint8_t scrollPrev;
  uint8_t scrollPrev2;

  // Mode 7.
  int16_t m7matrix[8]; // a, b, c, d, x, y, h, v
  uint8_t m7prev;
  bool m7largeField;
  bool m7charFill;
  bool m7xFlip;
  bool m7yFlip;
  bool m7extBg_always_zero;

  // Mode 7 internal.
  int32_t m7startX;
  int32_t m7startY;

  uint16_t oam[0x110];

  // Store 31 extra entries to remove the need for clamp.
  uint8_t brightnessMult[32 + 31];
  uint8_t brightnessMultHalf[32 * 2];

  uint16_t cgram[0x100];
  uint8_t mosaicModulo[kPpuXPixels];

  // Retained temporarily until all renderer references are migrated.
  uint32_t colorMapRgb[256];

  // Internal priority buffers.
  PpuPixelPrioBufs bgBuffers[2];
  PpuPixelPrioBufs objBuffer;

  uint16_t vram[0x8000];
};

Ppu* ppu_init();
void ppu_free(Ppu* ppu);
void ppu_reset(Ppu* ppu);
void ppu_handleVblank(Ppu* ppu);
void ppu_runLine(Ppu* ppu, int line);

uint8_t ppu_read(Ppu* ppu, uint8_t adr);
void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val);

void ppu_saveload(Ppu *ppu, SaveLoadFunc *func, void *ctx);

void PpuBeginDrawing(
    Ppu *ppu,
    uint8_t *buffer,
    size_t pitch,
    uint32_t render_flags
);

// Returns the current render scale, 1x = 256px, 2x = 512px, 4x = 1024px.
int PpuGetCurrentRenderScale(Ppu *ppu, uint32_t render_flags);

void PpuSetMode7PerspectiveCorrection(Ppu *ppu, int low, int high);
void PpuSetExtraSideSpace(Ppu *ppu, int left, int right, int bottom);

bool PpuUpdatePalette(Ppu* ppu);
const uint8_t* PpuGetVgaPalette(void);

void SetMathEnabled(bool val);

#endif  // ZELDA3_SNES_PPU_H_
