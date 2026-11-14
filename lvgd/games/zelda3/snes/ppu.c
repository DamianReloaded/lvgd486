#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "ppu.h"
#include "../src/types.h"

#define PPU_VRAM_WORDS 0x8000

static uint8 g_tileRowPixels[PPU_VRAM_WORDS][8];
static uint16 g_tileRowSource[PPU_VRAM_WORDS][2];
static uint8 g_tileRowValid[PPU_VRAM_WORDS];

static inline const uint8 *PpuGet4bppRowPixels(Ppu *ppu, const uint16 *addr)
{
    size_t index = (size_t)(addr - ppu->vram);
    uint16 plane01 = addr[0];
    uint16 plane23 = addr[8];

    if (!g_tileRowValid[index] ||
        g_tileRowSource[index][0] != plane01 ||
        g_tileRowSource[index][1] != plane23)
    {
        g_tileRowSource[index][0] = plane01;
        g_tileRowSource[index][1] = plane23;

        for (int x = 0; x < 8; x++)
        {
            g_tileRowPixels[index][x] =
                ((plane01 >> x) & 1) |
                (((plane01 >> (x + 8)) & 1) << 1) |
                (((plane23 >> x) & 1) << 2) |
                (((plane23 >> (x + 8)) & 1) << 3);
        }

        g_tileRowValid[index] = 1;
    }

    return g_tileRowPixels[index];
}
///

static const uint8 kSpriteSizes[8][2] = {
  {8, 16}, {8, 32}, {8, 64}, {16, 32},
  {16, 64}, {32, 64}, {16, 32}, {16, 32}
};

static bool ppu_evaluateSprites(Ppu* ppu, int line);
static void PpuDrawWholeLine(Ppu* ppu, uint y);

#define IS_SCREEN_ENABLED(ppu, sub, layer) (ppu->screenEnabled[sub] & (1 << layer))
#define IS_SCREEN_WINDOWED(ppu, sub, layer) (ppu->screenWindowed[sub] & (1 << layer))
#define IS_MOSAIC_ENABLED(ppu, layer) ((ppu->mosaicEnabled & (1 << layer)))
#define GET_WINDOW_FLAGS(ppu, layer) (ppu->windowsel >> (layer * 4))
enum {
  kWindow1Inversed = 1,
  kWindow1Enabled = 2,
  kWindow2Inversed = 4,
  kWindow2Enabled = 8,
};

Ppu* ppu_init()
{
    Ppu* ppu = (Ppu*)malloc(sizeof(Ppu));

    if (ppu == NULL)
        return NULL;

    ppu->extraLeftRight = kPpuExtraLeftRight;

    return ppu;
}

void ppu_free(Ppu* ppu) {
  free(ppu);
}

void ppu_reset(Ppu* ppu)
{
    memset(ppu->vram, 0, sizeof(ppu->vram));

    ppu->lastBrightnessMult = 0xff;
    ppu->lastMosaicModulo = 0xff;
    ppu->extraLeftCur = 0;
    ppu->extraRightCur = 0;
    ppu->extraBottomCur = 0;
    ppu->vramPointer = 0;
    ppu->vramIncrementOnHigh = false;
    ppu->vramIncrement = 1;

    memset(ppu->cgram, 0, sizeof(ppu->cgram));
    ppu->cgramPointer = 0;
    ppu->cgramSecondWrite = false;
    ppu->cgramBuffer = 0;

    memset(ppu->oam, 0, sizeof(ppu->oam));
    ppu->oamAdr = 0;
    ppu->oamSecondWrite = false;
    ppu->oamBuffer = 0;
    ppu->objTileAdr1 = 0x4000;
    ppu->objTileAdr2 = 0x5000;
    ppu->objSize = 0;
    memset(&ppu->objBuffer, 0, sizeof(ppu->objBuffer));

    for (int i = 0; i < 4; i++)
    {
        ppu->bgLayer[i].hScroll = 0;
        ppu->bgLayer[i].vScroll = 0;
        ppu->bgLayer[i].tilemapWider = false;
        ppu->bgLayer[i].tilemapHigher = false;
        ppu->bgLayer[i].tilemapAdr = 0;
        ppu->bgLayer[i].tileAdr = 0;
    }

    ppu->scrollPrev = 0;
    ppu->scrollPrev2 = 0;
    ppu->mosaicSize = 1;
    ppu->screenEnabled[0] = ppu->screenEnabled[1] = 0;
    ppu->screenWindowed[0] = ppu->screenWindowed[1] = 0;

    memset(ppu->m7matrix, 0, sizeof(ppu->m7matrix));
    ppu->m7prev = 0;
    ppu->m7largeField = true;
    ppu->m7charFill = false;
    ppu->m7xFlip = false;
    ppu->m7yFlip = false;
    ppu->m7extBg_always_zero = false;
    ppu->m7startX = 0;
    ppu->m7startY = 0;

    ppu->windowsel = 0;
    ppu->window1left = 0;
    ppu->window1right = 0;
    ppu->window2left = 0;
    ppu->window2right = 0;
    ppu->clipMode = 0;
    ppu->preventMathMode = 0;
    ppu->addSubscreen = false;
    ppu->subtractColor = false;
    ppu->halfColor = false;
    ppu->mathEnabled = 0;
    ppu->fixedColorR = 0;
    ppu->fixedColorG = 0;
    ppu->fixedColorB = 0;

    ppu->forcedBlank = true;
    ppu->brightness = 0;
    ppu->mode = 0;
}

void ppu_saveload(Ppu *ppu, SaveLoadFunc *func, void *ctx) {
  uint8 tmp[556] = { 0 };

  func(ctx, &ppu->vram, 0x8000 * 2);
  func(ctx, tmp, 10);
  func(ctx, &ppu->cgram, 512);
  func(ctx, tmp, 556);
  func(ctx, tmp, 520);
  for (int i = 0; i < 4; i++) {
    func(ctx, tmp, 4);
    func(ctx, &ppu->bgLayer[i].tilemapWider, 4);
    func(ctx, tmp, 4);
  }
  func(ctx, tmp, 123);
}

// ------------------------------------------------------------
// VGA palette
// ------------------------------------------------------------
static bool g_mathEnabled = false;
static uint8_t g_vgaPalette[256 * 3];
static uint16_t g_lastCgram[256];
static uint8_t g_lastBrightness;
static bool g_paletteInitialized = false;
static uint8_t g_blackIndex;

static uint8_t g_color255Index;
static uint8_t g_color255R;
static uint8_t g_color255G;
static uint8_t g_color255B;

static uint8_t PpuFindPaletteIndex(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t bestDistance = 0xffffffffu;
    uint8_t bestIndex = 0;

    for (int i = 0; i < 255; i++)
    {
        int dr = r - g_vgaPalette[i * 3 + 0];
        int dg = g - g_vgaPalette[i * 3 + 1];
        int db = b - g_vgaPalette[i * 3 + 2];

        uint32_t distance = dr * dr + dg * dg + db * db;

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = (uint8_t)i;

            if (distance == 0)
                break;
        }
    }

    return bestIndex;
}

void SetMathEnabled(bool val)
{
  g_mathEnabled = val;
}

static uint8_t ExpandColor(uint8_t value, uint8_t brightness)
{
    return (uint8_t)((((value << 3) | (value >> 2)) * brightness) / 15);
}

bool PpuUpdatePalette(Ppu* ppu)
{
    if (ppu == NULL)
        return false;

    if (g_paletteInitialized &&
        ppu->brightness == g_lastBrightness &&
        memcmp(ppu->cgram, g_lastCgram, sizeof(g_lastCgram)) == 0)
        return false;

    for (int i = 0; i < 256; i++)
    {
        uint16_t color = ppu->cgram[i];

        g_vgaPalette[i * 3 + 0] = ExpandColor(color & 0x1f, ppu->brightness);
        g_vgaPalette[i * 3 + 1] = ExpandColor((color >> 5) & 0x1f, ppu->brightness);
        g_vgaPalette[i * 3 + 2] = ExpandColor((color >> 10) & 0x1f, ppu->brightness);
    }

    g_vgaPalette[255 * 3 + 0] = 255;
    g_vgaPalette[255 * 3 + 1] = 255;
    g_vgaPalette[255 * 3 + 2] = 255;

    g_blackIndex = PpuFindPaletteIndex(0, 0, 0);

    uint16_t color = ppu->cgram[255];

    g_color255R = ExpandColor(color & 31, ppu->brightness);
    g_color255G = ExpandColor((color >> 5) & 31, ppu->brightness);
    g_color255B = ExpandColor((color >> 10) & 31, ppu->brightness);

    g_color255Index = PpuFindPaletteIndex(
        g_color255R, g_color255G, g_color255B
    );

    memcpy(g_lastCgram, ppu->cgram, sizeof(g_lastCgram));
    g_lastBrightness = ppu->brightness;
    g_paletteInitialized = true;

    return true;
}

const uint8_t* PpuGetVgaPalette(void)
{
    return g_vgaPalette;
}

int PpuGetCurrentRenderScale(Ppu *ppu, uint32_t render_flags)
{
    return 1;
}

void PpuBeginDrawing(Ppu *ppu, uint8_t *pixels, size_t pitch, uint32_t render_flags)
{
    ppu->renderFlags = render_flags;
    ppu->renderPitch = (uint32)pitch;
    ppu->renderBuffer = pixels;

    // Cache the brightness computation.
    if (ppu->brightness != ppu->lastBrightnessMult)
    {
        uint8_t brightness = ppu->brightness;
        ppu->lastBrightnessMult = brightness;

        for (int i = 0; i < 32; i++)
        {
            uint8_t value = ((i << 3) | (i >> 2)) * brightness / 15;

            ppu->brightnessMultHalf[i * 2] = value;
            ppu->brightnessMultHalf[i * 2 + 1] = value;
            ppu->brightnessMult[i] = value;
        }

        // Store 31 extra entries to avoid clamping.
        memset(&ppu->brightnessMult[32], ppu->brightnessMult[31], 31);
    }
}

static inline void ClearBackdrop(PpuPixelPrioBufs *buf)
{
    uint32 *dst = (uint32 *)buf->data;
    uint32 *end = dst + countof(buf->data) / 2;

    while (dst < end)
        *dst++ = 0x05000500;
}

void ppu_runLine(Ppu *ppu, int line)
{
    // Only render scanlines that produce visible output.
    if (line < 13 || line >= 213)
        return;

    if (ppu->forcedBlank)
    {
        ppu->lineHasSprites = false;
        PpuDrawWholeLine(ppu, line);
        return;
    }

    if (ppu->mosaicSize != ppu->lastMosaicModulo)
    {
        int mod = ppu->mosaicSize;
        ppu->lastMosaicModulo = mod;

        for (int i = 0, j = 0; i < countof(ppu->mosaicModulo); i++)
        {
            ppu->mosaicModulo[i] = i - j;
            j = (j + 1 == mod ? 0 : j + 1);
        }
    }

    // Don't evaluate sprites if the main screen doesn't display them.
    if (ppu->screenEnabled[0] & (1 << 4))
    {
        ClearBackdrop(&ppu->objBuffer);
        ppu->lineHasSprites = ppu_evaluateSprites(ppu, line - 1);
    }
    else
    {
        ppu->lineHasSprites = false;
    }

    PpuDrawWholeLine(ppu, line);
}

typedef struct PpuWindows {
  int16 edges[6];
  uint8 nr;
  uint8 bits;
} PpuWindows;

static void PpuWindows_Clear(PpuWindows *win, Ppu *ppu, uint layer) {
  win->edges[0] = -(layer != 2 ? ppu->extraLeftCur : 0);
  win->edges[1] = 256 + (layer != 2 ? ppu->extraRightCur : 0);
  win->nr = 1;
  win->bits = 0;
}

static void PpuWindows_Calc(PpuWindows *win, Ppu *ppu, uint layer) {
  // Evaluate which spans to render based on the window settings.
  // There are at most 5 windows.
  // Algorithm from Snes9x
  uint32 winflags = GET_WINDOW_FLAGS(ppu, layer);
  uint nr = 1;
  int window_right = 256 + (layer != 2 ? ppu->extraRightCur : 0);
  win->edges[0] = - (layer != 2 ? ppu->extraLeftCur : 0);
  win->edges[1] = window_right;
  uint i, j;
  int t;
  bool w1_ena = (winflags & kWindow1Enabled) && ppu->window1left <= ppu->window1right;
  if (w1_ena) {
    if (ppu->window1left > win->edges[0]) {
      win->edges[nr] = ppu->window1left;
      win->edges[++nr] = window_right;
    }
    if (ppu->window1right + 1 < window_right) {
      win->edges[nr] = ppu->window1right + 1;
      win->edges[++nr] = window_right;
    }
  }
  bool w2_ena = (winflags & kWindow2Enabled) && ppu->window2left <= ppu->window2right;
  if (w2_ena) {
    for (i = 0; i <= nr && (t = ppu->window2left) != win->edges[i]; i++) {
      if (t < win->edges[i]) {
        for (j = nr++; j >= i; j--)
          win->edges[j + 1] = win->edges[j];
        win->edges[i] = t;
        break;
      }
    }
    for (; i <= nr && (t = ppu->window2right + 1) != win->edges[i]; i++) {
      if (t < win->edges[i]) {
        for (j = nr++; j >= i; j--)
          win->edges[j + 1] = win->edges[j];
        win->edges[i] = t;
        break;
      }
    }
  }
  win->nr = nr;
  // get a bitmap of how regions map to windows
  uint8 w1_bits = 0, w2_bits = 0;
  if (w1_ena) {
    for (i = 0; win->edges[i] != ppu->window1left; i++);
    for (j = i; win->edges[j] != ppu->window1right + 1; j++);
    w1_bits = ((1 << (j - i)) - 1) << i;
  }
  if ((winflags & (kWindow1Enabled | kWindow1Inversed)) == (kWindow1Enabled | kWindow1Inversed))
    w1_bits = ~w1_bits;
  if (w2_ena) {
    for (i = 0; win->edges[i] != ppu->window2left; i++);
    for (j = i; win->edges[j] != ppu->window2right + 1; j++);
    w2_bits = ((1 << (j - i)) - 1) << i;
  }
  if ((winflags & (kWindow2Enabled | kWindow2Inversed)) == (kWindow2Enabled | kWindow2Inversed))
    w2_bits = ~w2_bits;
  win->bits = w1_bits | w2_bits;
}

// Draw a whole line of a 4bpp background layer into bgBuffers
static void PpuDrawBackground_4bpp(Ppu *ppu, uint y, bool sub, uint layer, PpuZbufType zhi, PpuZbufType zlo)
{
#define DO_PIXEL(i) do { \
  pixel = (bits >> i) & 1 | (bits >> (7 + i)) & 2 | (bits >> (14 + i)) & 4 | (bits >> (21 + i)) & 8; \
  if (pixel && z > dstz[i]) dstz[i] = z + pixel; } while (0)
#define DO_PIXEL_HFLIP(i) do { \
  pixel = (bits >> (7 - i)) & 1 | (bits >> (14 - i)) & 2 | (bits >> (21 - i)) & 4 | (bits >> (28 - i)) & 8; \
  if (pixel && z > dstz[i]) dstz[i] = z + pixel; } while (0)
#define READ_BITS(ta, tile) (addr = &ppu->vram[((ta) + (tile) * 16) & 0x7fff], addr[0] | addr[8] << 16)

  enum { kPaletteShift = 6 };

  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;

  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);

  BgLayer *bglayer = &ppu->bgLayer[layer];
  y += bglayer->vScroll;

  int sc_offs = bglayer->tilemapAdr + (((y >> 3) & 0x1f) << 5);

  if ((y & 0x100) && bglayer->tilemapHigher)
    sc_offs += bglayer->tilemapWider ? 0x800 : 0x400;

  const uint16 *tps[2] = {
    &ppu->vram[sc_offs & 0x7fff],
    &ppu->vram[sc_offs + (bglayer->tilemapWider ? 0x400 : 0) & 0x7fff]
  };

  int tileadr = ppu->bgLayer[layer].tileAdr, pixel;
  int tileadr1 = tileadr + 7 - (y & 0x7), tileadr0 = tileadr + (y & 0x7);
  const uint16 *addr;

  for (size_t windex = 0; windex < win.nr; windex++)
  {
    if (win.bits & (1 << windex))
      continue;

    uint x = win.edges[windex] + bglayer->hScroll;
    uint w = win.edges[windex + 1] - win.edges[windex];

    PpuZbufType *dstz = ppu->bgBuffers[sub].data + win.edges[windex] + kPpuExtraLeftRight;
    const uint16 *tp = tps[x >> 8 & 1] + ((x >> 3) & 0x1f);
    const uint16 *tp_last = tps[x >> 8 & 1] + 31;
    const uint16 *tp_next = tps[(x >> 8 & 1) ^ 1];

#define NEXT_TP() if (tp != tp_last) tp += 1; else tp = tp_next, tp_next = tp_last - 31, tp_last = tp + 31;

    // Handle clipped pixels on left side.
    if (x & 7)
    {
      int curw = IntMin(8 - (x & 7), w);
      w -= curw;

      uint32 tile = *tp;
      NEXT_TP();

      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);

      if (bits)
      {
        z += ((tile & 0x1c00) >> kPaletteShift);

        if (tile & 0x4000)
        {
          bits >>= (x & 7), x += curw;
          do DO_PIXEL(0); while (bits >>= 1, dstz++, --curw);
        }
        else
        {
          bits <<= (x & 7), x += curw;
          do DO_PIXEL_HFLIP(0); while (bits <<= 1, dstz++, --curw);
        }
      }
      else
      {
        dstz += curw;
      }
    }

    // Handle full tiles in the middle.
    while (w >= 8)
    {
      uint32 tile = *tp;
      NEXT_TP();

      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);

      if (bits)
      {
        z += ((tile & 0x1c00) >> kPaletteShift);

        if (tile & 0x4000)
        {
          DO_PIXEL(0); DO_PIXEL(1); DO_PIXEL(2); DO_PIXEL(3);
          DO_PIXEL(4); DO_PIXEL(5); DO_PIXEL(6); DO_PIXEL(7);
        }
        else
        {
          DO_PIXEL_HFLIP(0); DO_PIXEL_HFLIP(1); DO_PIXEL_HFLIP(2); DO_PIXEL_HFLIP(3);
          DO_PIXEL_HFLIP(4); DO_PIXEL_HFLIP(5); DO_PIXEL_HFLIP(6); DO_PIXEL_HFLIP(7);
        }
      }

      dstz += 8;
      w -= 8;
    }

    // Handle remaining clipped part.
    if (w)
    {
      uint32 tile = *tp;
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);

      if (bits)
      {
        z += ((tile & 0x1c00) >> kPaletteShift);

        if (tile & 0x4000)
        {
          do DO_PIXEL(0); while (bits >>= 1, dstz++, --w);
        }
        else
        {
          do DO_PIXEL_HFLIP(0); while (bits <<= 1, dstz++, --w);
        }
      }
    }
  }

#undef NEXT_TP
#undef READ_BITS
#undef DO_PIXEL
#undef DO_PIXEL_HFLIP
}

// Draw a whole line of a 2bpp background layer into bgBuffers
static void PpuDrawBackground_2bpp(Ppu *ppu, uint y, bool sub, uint layer, PpuZbufType zhi, PpuZbufType zlo) {
#define DO_PIXEL(i) do { \
  pixel = (bits >> i) & 1 | (bits >> (7 + i)) & 2; \
  if (pixel && z > dstz[i]) dstz[i] = z + pixel; } while (0)
#define DO_PIXEL_HFLIP(i) do { \
  pixel = (bits >> (7 - i)) & 1 | (bits >> (14 - i)) & 2; \
  if (pixel && z > dstz[i]) dstz[i] = z + pixel; } while (0)
#define READ_BITS(ta, tile) (addr = &ppu->vram[(ta) + (tile) * 8 & 0x7fff], addr[0])
  enum { kPaletteShift = 8 };
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);
  BgLayer *bglayer = &ppu->bgLayer[layer];
  y += bglayer->vScroll;
  int sc_offs = bglayer->tilemapAdr + (((y >> 3) & 0x1f) << 5);
  if ((y & 0x100) && bglayer->tilemapHigher)
    sc_offs += bglayer->tilemapWider ? 0x800 : 0x400;
  const uint16 *tps[2] = {
    &ppu->vram[sc_offs & 0x7fff],
    &ppu->vram[sc_offs + (bglayer->tilemapWider ? 0x400 : 0) & 0x7fff]
  };
  int tileadr = ppu->bgLayer[layer].tileAdr, pixel;
  int tileadr1 = tileadr + 7 - (y & 0x7), tileadr0 = tileadr + (y & 0x7);

  const uint16 *addr;
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    uint x = win.edges[windex] + bglayer->hScroll;
    uint w = win.edges[windex + 1] - win.edges[windex];
    PpuZbufType *dstz = ppu->bgBuffers[sub].data + win.edges[windex] + kPpuExtraLeftRight;
    const uint16 *tp = tps[x >> 8 & 1] + ((x >> 3) & 0x1f);
    const uint16 *tp_last = tps[x >> 8 & 1] + 31;
    const uint16 *tp_next = tps[(x >> 8 & 1) ^ 1];

#define NEXT_TP() if (tp != tp_last) tp += 1; else tp = tp_next, tp_next = tp_last - 31, tp_last = tp + 31;
    // Handle clipped pixels on left side
    if (x & 7) {
      int curw = IntMin(8 - (x & 7), w);
      w -= curw;
      uint32 tile = *tp;
      NEXT_TP();
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);
      if (bits) {
        z += ((tile & 0x1c00) >> kPaletteShift);
        if (tile & 0x4000) {
          bits >>= (x & 7), x += curw;
          do DO_PIXEL(0); while (bits >>= 1, dstz++, --curw);
        } else {
          bits <<= (x & 7), x += curw;
          do DO_PIXEL_HFLIP(0); while (bits <<= 1, dstz++, --curw);
        }
      } else {
        dstz += curw;
      }
    }
    // Handle full tiles in the middle
    while (w >= 8) {
      uint32 tile = *tp;
      NEXT_TP();
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);
      if (bits) {
        z += ((tile & 0x1c00) >> kPaletteShift);
        if (tile & 0x4000) {
          DO_PIXEL(0); DO_PIXEL(1); DO_PIXEL(2); DO_PIXEL(3);
          DO_PIXEL(4); DO_PIXEL(5); DO_PIXEL(6); DO_PIXEL(7);
        } else {
          DO_PIXEL_HFLIP(0); DO_PIXEL_HFLIP(1); DO_PIXEL_HFLIP(2); DO_PIXEL_HFLIP(3);
          DO_PIXEL_HFLIP(4); DO_PIXEL_HFLIP(5); DO_PIXEL_HFLIP(6); DO_PIXEL_HFLIP(7);
        }
      }
      dstz += 8, w -= 8;
    }
    // Handle remaining clipped part
    if (w) {
      uint32 tile = *tp;
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);
      if (bits) {
        z += ((tile & 0x1c00) >> kPaletteShift);
        if (tile & 0x4000) {
          do DO_PIXEL(0); while (bits >>= 1, dstz++, --w);
        } else {
          do DO_PIXEL_HFLIP(0); while (bits <<= 1, dstz++, --w);
        }
      }
    }
  }
#undef NEXT_TP
#undef READ_BITS
#undef DO_PIXEL
#undef DO_PIXEL_HFLIP
}

// Draw a whole line of a 4bpp background layer into bgBuffers, with mosaic applied
static void PpuDrawBackground_4bpp_mosaic(Ppu *ppu, uint y, bool sub, uint layer, PpuZbufType zhi, PpuZbufType zlo) {
#define GET_PIXEL() pixel = (bits) & 1 | (bits >> 7) & 2 | (bits >> 14) & 4 | (bits >> 21) & 8
#define GET_PIXEL_HFLIP() pixel = (bits >> 7) & 1 | (bits >> 14) & 2 | (bits >> 21) & 4 | (bits >> 28) & 8
#define READ_BITS(ta, tile) (addr = &ppu->vram[((ta) + (tile) * 16) & 0x7fff], addr[0] | addr[8] << 16)
  enum { kPaletteShift = 6 };
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);
  BgLayer *bglayer = &ppu->bgLayer[layer];
  y = ppu->mosaicModulo[y] + bglayer->vScroll;
  int sc_offs = bglayer->tilemapAdr + (((y >> 3) & 0x1f) << 5);
  if ((y & 0x100) && bglayer->tilemapHigher)
    sc_offs += bglayer->tilemapWider ? 0x800 : 0x400;
  const uint16 *tps[2] = {
    &ppu->vram[sc_offs & 0x7fff],
    &ppu->vram[sc_offs + (bglayer->tilemapWider ? 0x400 : 0) & 0x7fff]
  };
  int tileadr = ppu->bgLayer[layer].tileAdr, pixel;
  int tileadr1 = tileadr + 7 - (y & 0x7), tileadr0 = tileadr + (y & 0x7);
  const uint16 *addr;
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    int sx = win.edges[windex];
    PpuZbufType *dstz = ppu->bgBuffers[sub].data + sx + kPpuExtraLeftRight;
    PpuZbufType *dstz_end = ppu->bgBuffers[sub].data + win.edges[windex + 1] + kPpuExtraLeftRight;
    uint x = sx + bglayer->hScroll;
    const uint16 *tp = tps[x >> 8 & 1] + ((x >> 3) & 0x1f);
    const uint16 *tp_last = tps[x >> 8 & 1] + 31, *tp_next = tps[(x >> 8 & 1) ^ 1];
    x &= 7;
    int w = ppu->mosaicSize - (sx - ppu->mosaicModulo[sx]);
    do {
      w = IntMin(w, dstz_end - dstz);
      uint32 tile = *tp;
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);
      if (tile & 0x4000) bits >>= x, GET_PIXEL(); else bits <<= x, GET_PIXEL_HFLIP();
      if (pixel) {
        pixel += (tile & 0x1c00) >> kPaletteShift;
        int i = 0;
        do {
          if (z > dstz[i])
            dstz[i] = pixel + z;
        } while (++i != w);
      }
      dstz += w, x += w;
      for (; x >= 8; x -= 8)
        tp = (tp != tp_last) ? tp + 1 : tp_next;
      w = ppu->mosaicSize;
    } while (dstz_end - dstz != 0);
  }
#undef READ_BITS
#undef GET_PIXEL
#undef GET_PIXEL_HFLIP
}

// Draw a whole line of a 2bpp background layer into bgBuffers, with mosaic applied
static void PpuDrawBackground_2bpp_mosaic(Ppu *ppu, int y, bool sub, uint layer, PpuZbufType zhi, PpuZbufType zlo) {
#define GET_PIXEL() pixel = (bits) & 1 | (bits >> 7) & 2
#define GET_PIXEL_HFLIP() pixel = (bits >> 7) & 1 | (bits >> 14) & 2
#define READ_BITS(ta, tile) (addr = &ppu->vram[((ta) + (tile) * 8) & 0x7fff], addr[0])
  enum { kPaletteShift = 8 };
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);
  BgLayer *bglayer = &ppu->bgLayer[layer];
  y = ppu->mosaicModulo[y] + bglayer->vScroll;
  int sc_offs = bglayer->tilemapAdr + (((y >> 3) & 0x1f) << 5);
  if ((y & 0x100) && bglayer->tilemapHigher)
    sc_offs += bglayer->tilemapWider ? 0x800 : 0x400;
  const uint16 *tps[2] = {
    &ppu->vram[sc_offs & 0x7fff],
    &ppu->vram[sc_offs + (bglayer->tilemapWider ? 0x400 : 0) & 0x7fff]
  };
  int tileadr = ppu->bgLayer[layer].tileAdr, pixel;
  int tileadr1 = tileadr + 7 - (y & 0x7), tileadr0 = tileadr + (y & 0x7);
  const uint16 *addr;
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    int sx = win.edges[windex];
    PpuZbufType *dstz = ppu->bgBuffers[sub].data + sx + kPpuExtraLeftRight;
    PpuZbufType *dstz_end = ppu->bgBuffers[sub].data + win.edges[windex + 1] + kPpuExtraLeftRight;
    uint x = sx + bglayer->hScroll;
    const uint16 *tp = tps[x >> 8 & 1] + ((x >> 3) & 0x1f);
    const uint16 *tp_last = tps[x >> 8 & 1] + 31, *tp_next = tps[(x >> 8 & 1) ^ 1];
    x &= 7;
    int w = ppu->mosaicSize - (sx - ppu->mosaicModulo[sx]);
    do {
      w = IntMin(w, dstz_end - dstz);
      uint32 tile = *tp;
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32 bits = READ_BITS(ta, tile & 0x3ff);
      if (tile & 0x4000) bits >>= x, GET_PIXEL(); else bits <<= x, GET_PIXEL_HFLIP();
      if (pixel) {
        pixel += (tile & 0x1c00) >> kPaletteShift;
        uint i = 0;
        do {
          if (z > dstz[i])
            dstz[i] = pixel + z;
        } while (++i != w);
      }
      dstz += w, x += w;
      for (; x >= 8; x -= 8)
        tp = (tp != tp_last) ? tp + 1 : tp_next;
      w = ppu->mosaicSize;
    } while (dstz_end - dstz != 0);
  }
#undef READ_BITS
#undef GET_PIXEL
#undef GET_PIXEL_HFLIP
}


// level6 should be set if it's from palette 0xc0 which means color math is not applied
#define SPRITE_PRIO_TO_PRIO(prio, level6) (((prio) * 4 + 2) * 16 + 4 + (level6 ? 2 : 0))
#define SPRITE_PRIO_TO_PRIO_HI(prio) ((prio) * 4 + 2)

static void PpuDrawSprites(Ppu *ppu, uint y, uint sub, bool clear_backdrop) {
  int layer = 4;
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    int left = win.edges[windex];
    int width = win.edges[windex + 1] - left;
    PpuZbufType *src = ppu->objBuffer.data + left + kPpuExtraLeftRight;
    PpuZbufType *dst = ppu->bgBuffers[sub].data + left + kPpuExtraLeftRight;
    if (clear_backdrop) {
      memcpy(dst, src, width * sizeof(uint16));
    } else {
      do {
        if (src[0] > dst[0])
          dst[0] = src[0];
      } while (src++, dst++, --width);
    }
  }
}

// Assumes it's drawn on an empty backdrop
static void PpuDrawBackground_mode7(Ppu *ppu, uint y, bool sub, PpuZbufType z)
{
    int layer = 0;
    if (!IS_SCREEN_ENABLED(ppu, sub, layer))
        return;

    PpuWindows win;
    if (IS_SCREEN_WINDOWED(ppu, sub, layer))
        PpuWindows_Calc(&win, ppu, layer);
    else
        PpuWindows_Clear(&win, ppu, layer);

    int hScroll = ((int16_t)(ppu->m7matrix[6] << 3)) >> 3;
    int vScroll = ((int16_t)(ppu->m7matrix[7] << 3)) >> 3;
    int xCenter = ((int16_t)(ppu->m7matrix[4] << 3)) >> 3;
    int yCenter = ((int16_t)(ppu->m7matrix[5] << 3)) >> 3;

    int clippedH = hScroll - xCenter;
    int clippedV = vScroll - yCenter;

    clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
    clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);

    bool mosaicEnabled = IS_MOSAIC_ENABLED(ppu, 0);
    if (mosaicEnabled)
        y = ppu->mosaicModulo[y];

    int ry = ppu->m7yFlip ? 255 - (int)y : (int)y;

    int32_t m7startX =
        (ppu->m7matrix[0] * clippedH & ~63) +
        (ppu->m7matrix[1] * ry & ~63) +
        (ppu->m7matrix[1] * clippedV & ~63) +
        (xCenter << 8);

    int32_t m7startY =
        (ppu->m7matrix[2] * clippedH & ~63) +
        (ppu->m7matrix[3] * ry & ~63) +
        (ppu->m7matrix[3] * clippedV & ~63) +
        (yCenter << 8);

    int dx = ppu->m7xFlip ? -ppu->m7matrix[0] : ppu->m7matrix[0];
    int dy = ppu->m7xFlip ? -ppu->m7matrix[2] : ppu->m7matrix[2];

    uint32 outsideValue = ppu->m7largeField ? 0x3ffff : 0xffffffff;
    bool charFill = ppu->m7charFill;

    for (size_t windex = 0; windex < win.nr; windex++)
    {
        if (win.bits & (1 << windex))
            continue;

        int x = win.edges[windex];
        int x2 = win.edges[windex + 1];

        PpuZbufType *dstz = ppu->bgBuffers[sub].data + x + kPpuExtraLeftRight;
        PpuZbufType *dstzEnd = ppu->bgBuffers[sub].data + x2 + kPpuExtraLeftRight;

        int rx = ppu->m7xFlip ? 255 - x : x;
        int32_t xpos = m7startX + ppu->m7matrix[0] * rx;
        int32_t ypos = m7startY + ppu->m7matrix[2] * rx;

        if (mosaicEnabled)
        {
            int w = ppu->mosaicSize - (x - ppu->mosaicModulo[x]);

            while (dstz < dstzEnd)
            {
                if (w > dstzEnd - dstz)
                    w = (int)(dstzEnd - dstz);

                int tile;
                if ((uint32_t)(xpos | ypos) > outsideValue)
                {
                    if (!charFill)
                    {
                        xpos += dx * w;
                        ypos += dy * w;
                        dstz += w;
                        w = ppu->mosaicSize;
                        continue;
                    }

                    tile = 0;
                }
                else
                {
                    tile = ppu->vram[((ypos >> 11) & 0x7f) * 128 +
                                     ((xpos >> 11) & 0x7f)] & 0xff;
                }

                uint8 pixel = ppu->vram[tile * 64 +
                                         ((ypos >> 8) & 7) * 8 +
                                         ((xpos >> 8) & 7)] >> 8;

                if (pixel)
                {
                    int i = 0;
                    do
                    {
                        dstz[i] = pixel + z;
                    } while (++i < w);
                }

                xpos += dx * w;
                ypos += dy * w;
                dstz += w;
                w = ppu->mosaicSize;
            }
        }
        else
        {
            while (dstz < dstzEnd)
            {
                int tile;
                if ((uint32_t)(xpos | ypos) > outsideValue)
                {
                    if (!charFill)
                    {
                        xpos += dx;
                        ypos += dy;
                        ++dstz;
                        continue;
                    }

                    tile = 0;
                }
                else
                {
                    tile = ppu->vram[((ypos >> 11) & 0x7f) * 128 +
                                     ((xpos >> 11) & 0x7f)] & 0xff;
                }

                uint8 pixel = ppu->vram[tile * 64 +
                                         ((ypos >> 8) & 7) * 8 +
                                         ((xpos >> 8) & 7)] >> 8;

                if (pixel)
                    dstz[0] = pixel + z;

                xpos += dx;
                ypos += dy;
                ++dstz;
            }
        }
    }
}

void PpuSetMode7PerspectiveCorrection(Ppu *ppu, int low, int high) {
  ppu->mode7PerspectiveLow = low ? 1.0f / low : 0.0f;
  ppu->mode7PerspectiveHigh = 1.0f / high;
}

void PpuSetExtraSideSpace(Ppu *ppu, int left, int right, int bottom) {
  ppu->extraLeftCur = UintMin(left, ppu->extraLeftRight);
  ppu->extraRightCur = UintMin(right, ppu->extraLeftRight);
  ppu->extraBottomCur = UintMin(bottom, 16);
}

static FORCEINLINE float FloatInterpolate(float x, float xmin, float xmax, float ymin, float ymax) {
  return ymin + (ymax - ymin) * (x - xmin) * (1.0f / (xmax - xmin));
}

static inline uint8 PpuRgbToPaletteIndex(uint8 r, uint8 g, uint8 b)
{
    if (r == g_color255R && g == g_color255G && b == g_color255B)
        return g_color255Index;

    return PpuFindPaletteIndex(r, g, b);
}

static void PpuDrawBackgrounds(Ppu *ppu, int y, bool sub) {
// Top 4 bits contain the prio level, and bottom 4 bits the layer type.
// SPRITE_PRIO_TO_PRIO can be used to convert from obj prio to this prio.
//  15: BG3 tiles with priority 1 if bit 3 of $2105 is set
//  14: Sprites with priority 3 (4 * sprite_prio + 2)
//  12: BG1 tiles with priority 1
//  11: BG2 tiles with priority 1
//  10: Sprites with priority 2 (4 * sprite_prio + 2)
//  8: BG1 tiles with priority 0
//  7: BG2 tiles with priority 0
//  6: Sprites with priority 1 (4 * sprite_prio + 2)
//  3: BG3 tiles with priority 1 if bit 3 of $2105 is clear
//  2: Sprites with priority 0 (4 * sprite_prio + 2)
//  1: BG3 tiles with priority 0
//  0: backdrop

  if (ppu->mode == 1) {
    if (ppu->lineHasSprites)
      PpuDrawSprites(ppu, y, sub, true);

    if (IS_MOSAIC_ENABLED(ppu, 0))
      PpuDrawBackground_4bpp_mosaic(ppu, y, sub, 0, 0xc000, 0x8000);
    else
      PpuDrawBackground_4bpp(ppu, y, sub, 0, 0xc000, 0x8000);

    if (IS_MOSAIC_ENABLED(ppu, 1))
      PpuDrawBackground_4bpp_mosaic(ppu, y, sub, 1, 0xb100, 0x7100);
    else
      PpuDrawBackground_4bpp(ppu, y, sub, 1, 0xb100, 0x7100);

    if (IS_MOSAIC_ENABLED(ppu, 2))
      PpuDrawBackground_2bpp_mosaic(ppu, y, sub, 2, 0xf200, 0x1200);
    else
      PpuDrawBackground_2bpp(ppu, y, sub, 2, 0xf200, 0x1200);
  } else {
    // mode 7
    PpuDrawBackground_mode7(ppu, y, sub, 0xc000);
    if (ppu->lineHasSprites)
      PpuDrawSprites(ppu, y, sub, false);
  }
}

static NOINLINE void PpuDrawWholeLine(Ppu *ppu, uint y)
{
    int destY = (int)y - 13;

    uint8 blackIndex = g_blackIndex;
    uint8 *dst = ppu->renderBuffer + destY * ppu->renderPitch;
    memset(dst, blackIndex, 320);

    if (ppu->forcedBlank)
        return;

    ClearBackdrop(&ppu->bgBuffers[0]);
    PpuDrawBackgrounds(ppu, y, false);

    uint32 mathEnabled = g_mathEnabled ? ppu->mathEnabled : 0;
    bool renderedSubscreen = false;

    if (ppu->preventMathMode != 3 && ppu->addSubscreen && mathEnabled)
    {
        ClearBackdrop(&ppu->bgBuffers[1]);

        if (ppu->screenEnabled[1])
        {
            PpuDrawBackgrounds(ppu, y, true);
            renderedSubscreen = true;
        }
    }

    PpuWindows cwin;
    PpuWindows_Calc(&cwin, ppu, 5);

    static const uint8 kCwBitsMod[8] =
    {
        0x00, 0xff, 0xff, 0x00,
        0xff, 0x00, 0xff, 0x00
    };

    uint32 cwClipMath =
        ((cwin.bits & kCwBitsMod[ppu->clipMode]) ^
         kCwBitsMod[ppu->clipMode + 4]) |
        (((cwin.bits & kCwBitsMod[ppu->preventMathMode]) ^
          kCwBitsMod[ppu->preventMathMode + 4]) << 8);

    uint32 fixedColor =
        ppu->fixedColorR |
        (ppu->fixedColorG << 5) |
        (ppu->fixedColorB << 10);

    uint32 mathFlags =
        ((uint32)ppu->addSubscreen << 8) |
        ((uint32)ppu->subtractColor << 9);

    uint8 *halfColorMap = ppu->halfColor ?
        ppu->brightnessMultHalf : ppu->brightnessMult;

    const int bufferOffset = kPpuExtraLeftRight;
    const int lastPixel = bufferOffset + 256;

    for (uint32 windex = 0; windex < cwin.nr; windex++, cwClipMath >>= 1)
    {
        int left = cwin.edges[windex] + bufferOffset;
        int right = cwin.edges[windex + 1] + bufferOffset;

        if (left < bufferOffset)
            left = bufferOffset;
        if (right > lastPixel)
            right = lastPixel;
        if (left >= right)
            continue;

        uint8 *out = dst + left - bufferOffset + 32;
        PpuZbufType *mainBuffer = ppu->bgBuffers[0].data + left;
        PpuZbufType *subBuffer = ppu->bgBuffers[1].data + left;

        uint32 clipColorMask = (cwClipMath & 1) ? 0x1f : 0;
        uint32 mathEnabledCur = (cwClipMath & 0x100) ? mathEnabled : 0;

        if (!mathEnabledCur ||
            (fixedColor == 0 && !ppu->halfColor && !renderedSubscreen))
        {
            if (clipColorMask == 0)
            {
                memset(out, blackIndex, right - left);
            }
            else
            {
                do
                {
                    uint32 index = *mainBuffer++ & 0xff;

                    if (index != 255)
                    {
                        *out++ = (uint8)index;
                    }
                    else
                    {
                        uint32 color = ppu->cgram[255];
                        uint8 r = ppu->brightnessMult[color & 31];
                        uint8 g = ppu->brightnessMult[(color >> 5) & 31];
                        uint8 b = ppu->brightnessMult[(color >> 10) & 31];
                        *out++ = PpuRgbToPaletteIndex(r, g, b);
                    }
                } while (++left < right);
            }

            continue;
        }

        mathEnabledCur |= mathFlags;

        do
        {
            uint32 pixel = *mainBuffer++;
            uint32 layer = (pixel >> 8) & 0xf;

            if (!(mathEnabledCur & (1 << layer)))
            {
                uint32 index = pixel & 0xff;

                if (clipColorMask == 0)
                    *out++ = blackIndex;
                else if (index != 255)
                    *out++ = (uint8)index;
                else
                {
                    uint32 color = ppu->cgram[255];
                    uint8 r = ppu->brightnessMult[color & 31];
                    uint8 g = ppu->brightnessMult[(color >> 5) & 31];
                    uint8 b = ppu->brightnessMult[(color >> 10) & 31];
                    *out++ = PpuRgbToPaletteIndex(r, g, b);
                }

                ++subBuffer;
                continue;
            }

            uint32 color = ppu->cgram[pixel & 0xff];

            uint32 r = color & clipColorMask;
            uint32 g = (color >> 5) & clipColorMask;
            uint32 b = (color >> 10) & clipColorMask;

            uint8 *colorMap = ppu->brightnessMult;
            uint32 color2;

            if (mathEnabledCur & 0x100)
            {
                uint32 subPixel = *subBuffer & 0xff;

                if (subPixel)
                {
                    color2 = ppu->cgram[subPixel];
                    colorMap = halfColorMap;
                }
                else
                {
                    color2 = fixedColor;
                }
            }
            else
            {
                color2 = fixedColor;
                colorMap = halfColorMap;
            }

            uint32 r2 = color2 & 31;
            uint32 g2 = (color2 >> 5) & 31;
            uint32 b2 = (color2 >> 10) & 31;

            if (mathEnabledCur & 0x200)
            {
                r = (r > r2) ? r - r2 : 0;
                g = (g > g2) ? g - g2 : 0;
                b = (b > b2) ? b - b2 : 0;
            }
            else
            {
                r += r2;
                g += g2;
                b += b2;
            }

            uint8 finalR = colorMap[r];
            uint8 finalG = colorMap[g];
            uint8 finalB = colorMap[b];

            *out++ = PpuRgbToPaletteIndex(finalR, finalG, finalB);
            ++subBuffer;
        } while (++left < right);
    }
}

static bool ppu_evaluateSprites(Ppu* ppu, int line)
{
    int index = 0, index_end = index;
    int spritesLeft = 32 + 1, tilesLeft = 34 + 1;
    uint8 spriteSizes[2] = {
        kSpriteSizes[ppu->objSize][0],
        kSpriteSizes[ppu->objSize][1]
    };
    int extra_left_right = ppu->extraLeftRight;

    if (ppu->renderFlags & kPpuRenderFlags_NoSpriteLimits)
        spritesLeft = tilesLeft = 1024;

    int tilesLeftOrg = tilesLeft;

    do
    {
        int yy = ppu->oam[index] >> 8;

        if (yy == 0xf0)
            continue;

        int row = (line - yy) & 0xff;
        int highOam = ppu->oam[0x100 + (index >> 4)] >> (index & 15);
        int spriteSize = spriteSizes[(highOam >> 1) & 1];

        if (row >= spriteSize)
            continue;

        int x = (ppu->oam[index] & 0xff) + (highOam & 1) * 256;
        x -= (x >= 256 + extra_left_right) * 512;

        if (x <= -(spriteSize + extra_left_right))
            continue;

        if (--spritesLeft == 0)
            break;

        int oam1 = ppu->oam[index + 1];
        int objAdr = (oam1 & 0x100) ? ppu->objTileAdr2 : ppu->objTileAdr1;

        if (oam1 & 0x8000)
            row = spriteSize - 1 - row;

        int paletteBase = 0x80 + 16 * ((oam1 & 0xe00) >> 9);
        int prio = SPRITE_PRIO_TO_PRIO(
            (oam1 & 0x3000) >> 12,
            (oam1 & 0x800) == 0
        );
        PpuZbufType z = paletteBase + (prio << 8);
        bool hflip = (oam1 & 0x4000) != 0;

        for (int col = 0; col < spriteSize; col += 8)
        {
            if (col + x > -8 - extra_left_right &&
                col + x < 256 + extra_left_right)
            {
                if (--tilesLeft == 0)
                    return true;

                int usedCol = hflip ? spriteSize - 1 - col : col;
                int usedTile =
                    ((((oam1 & 0xff) >> 4) + (row >> 3)) << 4) |
                    (((oam1 & 0xf) + (usedCol >> 3)) & 0xf);

                uint16 *addr = &ppu->vram[
                    (objAdr + usedTile * 16 + (row & 0x7)) & 0x7fff
                ];

                if ((addr[0] | addr[8]) == 0)
                    continue;

                const uint8 *pixels = PpuGet4bppRowPixels(ppu, addr);

                int px_left = IntMax(
                    -(col + x + kPpuExtraLeftRight), 0
                );
                int px_right = IntMin(
                    256 + kPpuExtraLeftRight - (col + x), 8
                );

                PpuZbufType *dst = ppu->objBuffer.data +
                    col + x + px_left + kPpuExtraLeftRight;

                const uint8 *src = pixels + (hflip ? px_left : 7 - px_left);
                int srcStep = hflip ? 1 : -1;

                for (int px = px_left; px < px_right; px++, dst++, src += srcStep)
                {
                    int pixel = *src;

                    if (pixel != 0 && (dst[0] & 0xff) == 0)
                        dst[0] = z + pixel;
                }
            }
        }
    }
    while ((index = (index + 2) & 0xff) != index_end);

    return (tilesLeft != tilesLeftOrg);
}

uint8_t ppu_read(Ppu* ppu, uint8_t adr) {
  switch (adr) {
  case 0x34:
  case 0x35:
  case 0x36: {
    int result = ppu->m7matrix[0] * (ppu->m7matrix[1] >> 8);
    return (result >> (8 * (adr - 0x34))) & 0xff;
  }
  }
  return 0xff;
}

void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val)
{
    switch (adr)
    {
        case 0x00:
        {
            ppu->brightness = val & 0xf;
            ppu->forcedBlank = val & 0x80;
            break;
        }
        case 0x01:
        {
            assert(val == 2);
            break;
        }
        case 0x02:
        {
            ppu->oamAdr = (ppu->oamAdr & ~0xff) | val;
            ppu->oamSecondWrite = false;
            break;
        }
        case 0x03:
        {
            assert((val & 0x80) == 0);
            ppu->oamAdr = (ppu->oamAdr & ~0xff00) | ((val & 1) << 8);
            ppu->oamSecondWrite = false;
            break;
        }
        case 0x04:
        {
            if (!ppu->oamSecondWrite)
            {
                ppu->oamBuffer = val;
            }
            else
            {
                if (ppu->oamAdr < 0x110)
                    ppu->oam[ppu->oamAdr++] = (val << 8) | ppu->oamBuffer;
            }

            ppu->oamSecondWrite = !ppu->oamSecondWrite;
            break;
        }
        case 0x05:
        {
            ppu->mode = val & 0x7;
            assert(val == 7 || val == 9);
            assert(ppu->mode == 1 || ppu->mode == 7);
            assert((val & 0xf0) == 0);
            break;
        }
        case 0x06:
        {
            ppu->mosaicSize = (val >> 4) + 1;
            ppu->mosaicEnabled = (ppu->mosaicSize > 1) ? val : 0;
            break;
        }
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0a:
        {
            ppu->bgLayer[adr - 7].tilemapWider = val & 0x1;
            ppu->bgLayer[adr - 7].tilemapHigher = val & 0x2;
            ppu->bgLayer[adr - 7].tilemapAdr = (val & 0xfc) << 8;
            break;
        }
        case 0x0b:
        {
            ppu->bgLayer[0].tileAdr = (val & 0xf) << 12;
            ppu->bgLayer[1].tileAdr = (val & 0xf0) << 8;
            break;
        }
        case 0x0c:
        {
            ppu->bgLayer[2].tileAdr = (val & 0xf) << 12;
            ppu->bgLayer[3].tileAdr = (val & 0xf0) << 8;
            break;
        }
        case 0x0d:
        {
            ppu->m7matrix[6] = ((val << 8) | ppu->m7prev) & 0x1fff;
            ppu->m7prev = val;
        }
        /* Fall through */
        case 0x0f:
        case 0x11:
        case 0x13:
        {
            ppu->bgLayer[(adr - 0xd) / 2].hScroll = ((val << 8) | (ppu->scrollPrev & 0xf8) | (ppu->scrollPrev2 & 0x7)) & 0x3ff;
            ppu->scrollPrev = val;
            ppu->scrollPrev2 = val;
            break;
        }
        case 0x0e:
        {
            ppu->m7matrix[7] = ((val << 8) | ppu->m7prev) & 0x1fff;
            ppu->m7prev = val;
        }
        /* Fall through */
        case 0x10:
        case 0x12:
        case 0x14:
        {
            ppu->bgLayer[(adr - 0xe) / 2].vScroll = ((val << 8) | ppu->scrollPrev) & 0x3ff;
            ppu->scrollPrev = val;
            break;
        }
        case 0x15:
        {
            if ((val & 3) == 0)
                ppu->vramIncrement = 1;
            else if ((val & 3) == 1)
                ppu->vramIncrement = 32;
            else
                ppu->vramIncrement = 128;

            assert(((val & 0xc) >> 2) == 0);
            ppu->vramIncrementOnHigh = val & 0x80;
            break;
        }
        case 0x16:
        {
            ppu->vramPointer = (ppu->vramPointer & 0xff00) | val;
            break;
        }
        case 0x17:
        {
            ppu->vramPointer = (ppu->vramPointer & 0x00ff) | (val << 8);
            break;
        }
        case 0x18:
        {
            uint16_t vramAdr = ppu->vramPointer;

            ppu->vram[vramAdr & 0x7fff] =
                (ppu->vram[vramAdr & 0x7fff] & 0xff00) | val;

            if (!ppu->vramIncrementOnHigh)
                ppu->vramPointer += ppu->vramIncrement;

            break;
        }
        case 0x19:
        {
            uint16_t vramAdr = ppu->vramPointer;

            ppu->vram[vramAdr & 0x7fff] =
                (ppu->vram[vramAdr & 0x7fff] & 0x00ff) | (val << 8);

            if (ppu->vramIncrementOnHigh)
                ppu->vramPointer += ppu->vramIncrement;

            break;
        }
        case 0x1a:
        {
            assert(val == 0x80);
            ppu->m7largeField = val & 0x80;
            ppu->m7charFill = val & 0x40;
            ppu->m7yFlip = val & 0x2;
            ppu->m7xFlip = val & 0x1;
            break;
        }
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e:
        {
            ppu->m7matrix[adr - 0x1b] = (val << 8) | ppu->m7prev;
            ppu->m7prev = val;
            break;
        }
        case 0x1f:
        case 0x20:
        {
            ppu->m7matrix[adr - 0x1b] = ((val << 8) | ppu->m7prev) & 0x1fff;
            ppu->m7prev = val;
            break;
        }
        case 0x21:
        {
            ppu->cgramPointer = val;
            ppu->cgramSecondWrite = false;
            break;
        }
        case 0x22:
        {
            if (!ppu->cgramSecondWrite)
            {
                ppu->cgramBuffer = val;
            }
            else
            {
                ppu->cgram[ppu->cgramPointer++] = (val << 8) | ppu->cgramBuffer;
            }

            ppu->cgramSecondWrite = !ppu->cgramSecondWrite;
            break;
        }
        case 0x23:
        {
            ppu->windowsel = (ppu->windowsel & ~0xff) | val;
            break;
        }
        case 0x24:
        {
            ppu->windowsel = (ppu->windowsel & ~0xff00) | (val << 8);
            break;
        }
        case 0x25:
        {
            ppu->windowsel = (ppu->windowsel & ~0xff0000) | (val << 16);
            break;
        }
        case 0x26:
        {
            ppu->window1left = val;
            break;
        }
        case 0x27:
        {
            ppu->window1right = val;
            break;
        }
        case 0x28:
        {
            ppu->window2left = val;
            break;
        }
        case 0x29:
        {
            ppu->window2right = val;
            break;
        }
        case 0x2a:
        {
            assert(val == 0);
            break;
        }
        case 0x2b:
        {
            assert(val == 0);
            break;
        }
        case 0x2c:
        {
            ppu->screenEnabled[0] = val;
            break;
        }
        case 0x2d:
        {
            ppu->screenEnabled[1] = val;
            break;
        }
        case 0x2e:
        {
            ppu->screenWindowed[0] = val;
            break;
        }
        case 0x2f:
        {
            ppu->screenWindowed[1] = val;
            break;
        }
        case 0x30:
        {
            assert((val & 1) == 0);
            ppu->addSubscreen = val & 0x2;
            ppu->preventMathMode = (val & 0x30) >> 4;
            ppu->clipMode = (val & 0xc0) >> 6;
            break;
        }
        case 0x31:
        {
            ppu->subtractColor = val & 0x80;
            ppu->halfColor = val & 0x40;
            ppu->mathEnabled = val & 0x3f;
            break;
        }
        case 0x32:
        {
            if (val & 0x80)
                ppu->fixedColorB = val & 0x1f;
            if (val & 0x40)
                ppu->fixedColorG = val & 0x1f;
            if (val & 0x20)
                ppu->fixedColorR = val & 0x1f;
            break;
        }
        case 0x33:
        {
            assert(val == 0);
            ppu->m7extBg_always_zero = val & 0x40;
            break;
        }
        default:
        {
            break;
        }
    }
}