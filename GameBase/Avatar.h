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

  void print(){
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
  }
};

struct Circle{
  float x;
  float y;
  float radius;
};

class Avatar{
  public: 
    int id = 0;
    float x = 0;
    float y = 0;
    float previousRenderedX = 0;
    float previousRenderedY = 0;
    uint16_t width  = 0;
    uint16_t height = 0;

    Vec2 velocity;

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
    void updatePos(){
      this->x += this->velocity.x;
      this->y += this->velocity.y;
    }
    
};
#endif
