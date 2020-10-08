#ifndef _AVATAR_H_
#define _AVATAR_H_

struct Vec2{
  float x;
  float y;
  
  Vec2() : x(0), y(0){  };
  Vec2(float _x, float _y) : x (_x), y(_y){};

  Vec2 operator+(const Vec2& b){
    Vec2 vec;
    vec.x = this->x + b.x;
    vec.y = this->y + b.y;
    return vec;
  }

  Vec2 operator-(const Vec2& b){
    Vec2 vec;
    vec.x = this->x - b.x;
    vec.y = this->y - b.y;
    return vec;
  }
};



class Avatar{
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
    
    Avatar(){};
    Avatar(float initX, float initY, uint16_t _width, uint16_t _height, const uint16_t *_bitmap, const uint8_t *_mask) : 
      x(initX), y(initY), width(_width), height(_height), bitmap(_bitmap), mask(_mask), previousRenderedX(initX), previousRenderedY(initY){
        this->velocity = Vec2();
        
    };
    
    void savePreviousRenderPos(){
      previousRenderedX = x;
      previousRenderedY = y;
    }
    void setVelocity(float dx, float dy){
      this->velocity.x = dx;
      this->velocity.y = dy;
    }
    void setPos(float x, float y){
      this->x = x;
      this->y = y;
    }

    
    void updatePos(unsigned long currentTime){
      if(currentTime >= this->nextPosUpdateTime){
        this->x += this->velocity.x;
        this->y += this->velocity.y;
        this->nextPosUpdateTime = currentTime + this->updateInterval;
      }
      if(this->_enableBreathing){
        if(currentTime >= this->breathUpdateTime){
          isBreathingDown = !isBreathingDown;
          this->breathUpdateTime = currentTime + this->_breathInterval;
        }
        
      }
    }

    //this is used by renderScene function to simulate up and down motion
    
    boolean isBreathingDown = false; 

    void setBreathInterval(uint16_t breathInterval){
      this->_breathInterval = breathInterval;
    }
    void setBreathPosition(uint16_t pos){
      this->_breathPosition = pos;
    }
    void enableBreathing(){
      this->_enableBreathing = true;
    }
    void disableBreathing(){
      this->_enableBreathing = false;
    }
    bool isBreathingEnabled(){
      return this->_enableBreathing;
    }
   private: 
    unsigned long nextPosUpdateTime = 0;
    unsigned long breathUpdateTime = 0;
    float previousRenderedX = 0;
    float previousRenderedY = 0;
     uint16_t _breathPosition = 0;
     boolean _enableBreathing = false;
     uint16_t _breathInterval = 0;
};
#endif
