#ifndef _SPRITESHEET_H_
#define _SPRITESHEET_H_

#include "Avatar.h"

struct SpriteSheetRegion {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

class SpriteSheet {
  public:
    SpriteSheet(const uint16_t *bitmap, const uint8_t *mask, uint16_t sheetWidth, uint16_t sheetHeight) :
      _bitmap(bitmap), _mask(mask), _sheetWidth(sheetWidth), _sheetHeight(sheetHeight), _asset(NULL) {}

    SpriteSheet(const SpriteAsset *asset) :
      _bitmap(NULL), _mask(NULL), _sheetWidth(asset->sheetWidth), _sheetHeight(asset->sheetHeight),
      _asset(asset) {}

    static SpriteSheetRegion readRegion(const SpriteSheetRegion *regions, int index) {
      SpriteSheetRegion region;
      memcpy_P(&region, &regions[index], sizeof(SpriteSheetRegion));
      return region;
    }

    static SpriteBitmapRegion readBitmapRegion(const SpriteBitmapRegion *regions, int index) {
      SpriteBitmapRegion region;
      memcpy_P(&region, &regions[index], sizeof(SpriteBitmapRegion));
      return region;
    }

    void applyRegion(Avatar *avatar, const SpriteSheetRegion &region) const {
      applyRegion(avatar, region.x, region.y, region.width, region.height);
    }

    void applyRegion(Avatar *avatar, const SpriteBitmapRegion &region) const {
      applyRegion(avatar, region.x, region.y, region.width, region.height);
    }

    void applyRegion(Avatar *avatar, uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height) const {
      if (_asset != NULL) {
        avatar->setSheetSource(_asset, srcX, srcY, width, height);
      } else {
        avatar->setSheetSource(_bitmap, _mask, _sheetWidth, srcX, srcY, width, height);
      }
    }

    Avatar *createAvatar(float x, float y, const SpriteSheetRegion &region) const {
      return createAvatar(x, y, region.x, region.y, region.width, region.height);
    }

    Avatar *createAvatar(float x, float y, const SpriteBitmapRegion &region) const {
      return createAvatar(x, y, region.x, region.y, region.width, region.height);
    }

    Avatar *createAvatar(float x, float y, uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height) const {
      Avatar *avatar = (_asset != NULL)
                             ? new Avatar(x, y, width, height, NULL, NULL)
                             : new Avatar(x, y, width, height, _bitmap, _mask);
      applyRegion(avatar, srcX, srcY, width, height);
      avatar->setVelocity(0, 0);
      avatar->updateInterval = 50;
      return avatar;
    }

    const SpriteAsset *asset() const {
      return _asset;
    }

  private:
    const uint16_t *_bitmap;
    const uint8_t *_mask;
    uint16_t _sheetWidth;
    uint16_t _sheetHeight;
    const SpriteAsset *_asset;
};

#endif
