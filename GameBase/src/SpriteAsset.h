#ifndef _SPRITEASSET_H_
#define _SPRITEASSET_H_

#include <Arduino.h>
#include <pgmspace.h>

enum SpriteBpp : uint8_t {
  SPRITE_BPP_4 = 4,
  SPRITE_BPP_8 = 8,
  SPRITE_BPP_16 = 16,
};

struct SpriteBitmapRegion {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

struct SpriteAsset {
  uint8_t bpp;
  uint16_t sheetWidth;
  uint16_t sheetHeight;
  uint16_t paletteCount;
  const uint16_t *palette;
  const void *pixels;
  const uint8_t *mask;
};

static inline uint8_t spriteAssetMaskBit(const SpriteAsset *asset, uint16_t sheetX, uint16_t sheetY) {
  uint16_t sheetBw = (asset->sheetWidth + 7) / 8;
  uint8_t maskByte =
      pgm_read_byte(asset->mask + (uint32_t)sheetY * sheetBw + (sheetX >> 3));
  return maskByte & (0x80 >> (sheetX & 7));
}

static inline uint16_t spriteAssetPixelRgb565(const SpriteAsset *asset, uint16_t sheetX,
                                              uint16_t sheetY) {
  uint32_t idx = (uint32_t)sheetY * asset->sheetWidth + sheetX;

  if (asset->bpp == SPRITE_BPP_16) {
    const uint16_t *pixels = (const uint16_t *)asset->pixels;
    return pgm_read_word_far(pixels + idx);
  }

  if (asset->bpp == SPRITE_BPP_8) {
    const uint8_t *pixels = (const uint8_t *)asset->pixels;
    uint8_t palIdx = pgm_read_byte(pixels + idx);
    return pgm_read_word_far(asset->palette + palIdx);
  }

  // 4 bpp: two pixels per byte, high nibble first (even column).
  const uint8_t *pixels = (const uint8_t *)asset->pixels;
  uint32_t byteIdx = idx >> 1;
  uint8_t byte = pgm_read_byte(pixels + byteIdx);
  uint8_t nibble = (idx & 1) ? (byte & 0x0F) : (byte >> 4);
  return pgm_read_word_far(asset->palette + nibble);
}

static inline uint16_t spriteAssetPixelRgb565Flipped(const SpriteAsset *asset, uint16_t sheetX,
                                                     uint16_t sheetY, uint16_t regionWidth,
                                                     uint16_t localX) {
  uint16_t srcCol = regionWidth - 1 - localX;
  return spriteAssetPixelRgb565(asset, sheetX - localX + srcCol, sheetY);
}

#endif
