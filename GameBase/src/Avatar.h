#ifndef _AVATAR_H_
#define _AVATAR_H_
#include <Arduino.h>
struct Vec2 {
  float x;
  float y;

  Vec2() : x(0), y(0) {  };
  Vec2(float _x, float _y) : x (_x), y(_y) {};

  Vec2 operator+(const Vec2& b) {
    Vec2 vec;
    vec.x = this->x + b.x;
    vec.y = this->y + b.y;
    return vec;
  }

  Vec2 operator-(const Vec2& b) {
    Vec2 vec;
    vec.x = this->x - b.x;
    vec.y = this->y - b.y;
    return vec;
  }
};



class Avatar {
    friend class GameScene;
    friend class Attachment;
  public:
    float x = 0;
    float y = 0;

    uint16_t width  = 0;
    uint16_t height = 0;


    uint16_t breathAmount = 1; //default
    Vec2 velocity;
    int updateInterval = 0; //in milli seconds
    const uint16_t* bitmaps [50]; //suppose at most 50 frames per avatar;
    const uint8_t* masks [50]; 
    volatile byte numOfFrames = 0;
    volatile byte currentFrame = 0;
    unsigned long frameUpdatetime = 0;
    unsigned long lastFrameUpdatetime = 0;
    
    Avatar() {};
    Avatar(float initX, float initY, uint16_t _width, uint16_t _height, const uint16_t *_bitmap, const uint8_t *_mask) :
      x(initX), y(initY), width(_width), height(_height), previousRenderedX(initX), previousRenderedY(initY) {
      this->velocity = Vec2();
      bitmaps[0] = _bitmap;
      masks[0] = _mask;
      numOfFrames++;
    };
    
    void setFrameUpdatetime(int updateTime) {
      frameUpdatetime = updateTime;
    }
    
    void addFrame(const uint16_t *_bitmap, const uint8_t *_mask){
      if (numOfFrames < 50) {
        bitmaps[numOfFrames] = _bitmap;
        masks[numOfFrames] = _mask;      
        numOfFrames++;
      }
      // do not allow adding additional frame, it will cause array out of bounds
    }

    void updateFrameIndex(unsigned long currentMillis) {
      if (numOfFrames > 1) {
        if ( (currentMillis - lastFrameUpdatetime) >= frameUpdatetime ) {
          if (currentFrame >= numOfFrames - 1) {
            currentFrame = 0;
            return;
          }
          currentFrame = currentFrame + 1;
          lastFrameUpdatetime = currentMillis;  
        }
      }
    }

    
    const uint16_t* getBitmap() {
      return bitmaps[currentFrame];
    }

    const uint8_t* getMask() {
      return masks[currentFrame];
    }

    void savePreviousRenderPos() {
      previousRenderedX = x;
      previousRenderedY = y;
    }
    void setVelocity(float dx, float dy) {
      this->velocity.x = dx;
      this->velocity.y = dy;
    }
    void setPos(float x, float y) {
      this->x = x;
      this->y = y;
    }

    // Horizontal mirroring. Works for both plain bitmaps and sprite-sheet
    // regions, and for the mask-based hit test below, so a single sprite can
    // face both directions without a second set of frames.
    bool flipX = false;
    void setFlipX(bool f) {
      if (this->flipX != f) {
        this->flipX = f;
        requestRedraw();
      }
    }

    void setSheetSource(const uint16_t *sheetBitmap, const uint8_t *sheetMask, uint16_t sheetW,
                        uint16_t srcX, uint16_t srcY, uint16_t regionWidth, uint16_t regionHeight) {
      useSheetSource = true;
      this->sheetBitmap = sheetBitmap;
      this->sheetMask = sheetMask;
      this->sheetWidth = sheetW;
      this->sheetSrcX = srcX;
      this->sheetSrcY = srcY;
      this->width = regionWidth;
      this->height = regionHeight;
    }

    void clearSheetSource() {
      useSheetSource = false;
      sheetBitmap = NULL;
      sheetMask = NULL;
      sheetWidth = 0;
      sheetSrcX = 0;
      sheetSrcY = 0;
    }

    // Force a repaint of this avatar on the next renderScene() without
    // disturbing previousRenderedX/Y. Clobbering the previous position (as this
    // used to do) breaks the dirty-rect union for a moving avatar and leaves a
    // trail of stale pixels, so use a flag the renderer honours instead.
    void requestRedraw() {
      forceRedraw = true;
    }

    bool usesSheetSource() const {
      return useSheetSource;
    }


    void updatePos(unsigned long currentTime) {
      if (currentTime >= this->nextPosUpdateTime) {
        this->x += this->velocity.x;
        this->y += this->velocity.y;
        this->nextPosUpdateTime = currentTime + this->updateInterval;
      }
      if (this->_enableBreathing) {
        if (currentTime >= this->breathUpdateTime) {
          isBreathingDown = !isBreathingDown;
          this->breathUpdateTime = currentTime + this->_breathInterval;
        }

      }
    }

    //this is used by renderScene function to simulate up and down motion

    boolean isBreathingDown = false;

    void setBreathInterval(uint16_t breathInterval) {
      this->_breathInterval = breathInterval;
    }
    void setBreathPosition(uint16_t pos) {
      this->_breathPosition = this->height - pos;
    }
    uint16_t getBreathPosition() {
      return this->height - this->_breathPosition;
    }
    void enableBreathing() {
      this->_enableBreathing = true;
    }
    void disableBreathing() {
      this->_enableBreathing = false;
    }
    bool isBreathingEnabled() {
      return this->_enableBreathing;
    }

    // Return true if (x,y) is on a non-transparent pixel of the current frame.
    bool contains(uint16_t targetX, uint16_t targetY) {
      if (targetX <= this->x || targetX >= (this->x + this->width) ||
          targetY <= this->y || targetY >= (this->y + this->height)) {
        return false;
      }

      const uint8_t *mask = getMask();
      if (mask == NULL) {
        return true;
      }

      uint16_t localX = targetX - (uint16_t)this->x;
      uint16_t localY = targetY - (uint16_t)this->y;
      uint16_t srcCol = flipX ? (this->width - 1 - localX) : localX;
      if (useSheetSource) {
        uint16_t sheetX = sheetSrcX + srcCol;
        uint16_t sheetY = sheetSrcY + localY;
        uint16_t bytesPerSheetRow = (sheetWidth + 7) / 8;
        uint8_t maskByte = pgm_read_byte(sheetMask + (uint32_t)sheetY * bytesPerSheetRow + (sheetX >> 3));
        return (maskByte & (0x80 >> (sheetX & 7))) != 0;
      }

      uint16_t bytesPerRow = (this->width + 7) / 8;
      uint8_t maskByte = pgm_read_byte(mask + (uint32_t)localY * bytesPerRow + (srcCol >> 3));
      return (maskByte & (0x80 >> (srcCol & 7))) != 0;
    }
  private:
    unsigned long nextPosUpdateTime = 0;
    unsigned long breathUpdateTime = 0;
    float previousRenderedX = 0;
    float previousRenderedY = 0;
    bool renderTainted = false;
    bool forceRedraw = false;
    bool useSheetSource = false;
    const uint16_t *sheetBitmap = NULL;
    const uint8_t *sheetMask = NULL;
    uint16_t sheetWidth = 0;
    uint16_t sheetSrcX = 0;
    uint16_t sheetSrcY = 0;
    uint16_t _breathPosition = 0;
    boolean _enableBreathing = false;
    uint16_t _breathInterval = 0;
};
#endif
