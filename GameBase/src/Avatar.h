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
    int id = 0;
    float x = 0;
    float y = 0;

    uint16_t width  = 0;
    uint16_t height = 0;


    uint16_t breathAmount = 1; //default
    Vec2 velocity;
    int updateInterval = 0; //in milli seconds
    const uint16_t *bitmap;
    const uint8_t *mask;
    const uint16_t* bitmaps [50]; //suppose at most 50 frames per avatar;
    const uint8_t* masks [50]; 
    volatile byte numOfFrames = 0;
    volatile byte currentFrame = 0;
    unsigned long frameUpdatetime = 0;
    unsigned long lastFrameUpdatetime = 0;
    
    Avatar() {};
    Avatar(float initX, float initY, uint16_t _width, uint16_t _height, const uint16_t *_bitmap, const uint8_t *_mask) :
      x(initX), y(initY), width(_width), height(_height), bitmap(_bitmap), mask(_mask), previousRenderedX(initX), previousRenderedY(initY) {
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
      uint16_t bytesPerRow = (this->width + 7) / 8;
      uint8_t maskByte = pgm_read_byte(mask + (uint32_t)localY * bytesPerRow + (localX >> 3));
      return (maskByte & (0x80 >> (localX & 7))) != 0;
    }
  private:
    unsigned long nextPosUpdateTime = 0;
    unsigned long breathUpdateTime = 0;
    float previousRenderedX = 0;
    float previousRenderedY = 0;
    bool renderTainted = false;
    uint16_t _breathPosition = 0;
    boolean _enableBreathing = false;
    uint16_t _breathInterval = 0;
};
#endif
