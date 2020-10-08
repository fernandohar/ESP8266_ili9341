#ifndef _ATTACHMENT_H_
#define _ATTACHMENT_H_

#include "Avatar.h"
class Avatar; //Avatar and Attachment references each other

class Attachment : public Avatar {
  public:
    Attachment() {};
    Attachment(int16_t parentX, int16_t parentY, Avatar *parent, uint16_t _width, uint16_t _height, const uint16_t *_bitmap, const uint8_t *_mask) :
      Avatar(0, 0, _width, _height, _bitmap, _mask),  _attachToParentX(parentX), _attachToParentY(parentY), _parent(parent) {
      x = parent->x + parentX; //Set its X & Y relative to screen
      y = parent->y + parentY;
      Serial.println(y);
    };

    void updatePos(unsigned long currentTime) {
      this->x = _parent->x + _attachToParentX;

      if (_parent->_enableBreathing && _attachToParentY <= _parent->_breathPosition) {
        if (_parent->isBreathingDown != isParentBreathingDown) {
          isParentBreathingDown = _parent->isBreathingDown;
          if (_parent->isBreathingDown) {
            this->y = _parent->y + _attachToParentY + _parent->breathAmount;
          } else {
            this->y = _parent->y + _attachToParentY;
          }
        }
      } else {
        this->y = _parent->y + _attachToParentY;
      }
    }

  private:
    Avatar *_parent;
    int16_t _attachToParentX; //position of parent it attach to
    int16_t _attachToParentY;
    boolean isParentBreathingDown = false;
};
#endif
