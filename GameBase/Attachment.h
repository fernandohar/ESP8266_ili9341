#ifndef _ATTACHMENT_H_
#define _ATTACHMENT_H_

#include "Avatar.h"
class Avatar; //Avatar and Attachment references each other

class Attachment : public Avatar{
  public:
  

   Attachment(){};
   Attachment(uint16_t parentX, uint16_t parentY, Avatar *parent, uint16_t _width, uint16_t _height, const uint16_t *_bitmap, const uint8_t *_mask) : 
      Avatar(0, 0, _width, _height, _bitmap, _mask),  _parentX(parentX), _parentY(parentY), _parent(parent){
       x = parent->x + parentX; //Set its X & Y relative to screen
       y = parent->y + parentY; 
    };

   void updatePos(unsigned long currentTime){
    this->x = _parent->x + _parentX;
    this->y = _parent->y + _parentY;
//    Serial.println("updatePos of Attachment");
    
//    this->x += this->velocity.x;
//    this->y += this->velocity.y;
//    if(this->enableBreathing){
//      breathCounter++;
//    }
//    if(breathCounter >= breathDuration){
//      isBreathingDown = !isBreathingDown;
//      breathCounter = 0;
//    }
  }

  private: 
  Avatar *_parent;
  uint16_t _parentX; //position of parent it attach to
  uint16_t _parentY;
};
#endif
