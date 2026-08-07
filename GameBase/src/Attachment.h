#ifndef _ATTACHMENT_H_
#define _ATTACHMENT_H_
#include <Arduino.h>
#include "Avatar.h"

class Attachment : public Avatar {
  public:
    Attachment() {};
    Attachment(int16_t parentX, int16_t parentY, Avatar *parent, uint16_t _width, uint16_t _height, const uint16_t *_bitmap, const uint8_t *_mask) :
      Avatar(0, 0, _width, _height, _bitmap, _mask), _parent(parent), _attachToParentX(parentX), _attachToParentY(parentY) {
      x = parent->x + parentX; //Set its X & Y relative to screen
      y = parent->y + parentY;
    };

    // Re-anchor on the parent, e.g. when a pose swap moves the face.
    void setAttachOffset(int16_t parentX, int16_t parentY) {
      _attachToParentX = parentX;
      _attachToParentY = parentY;
    }

    void updatePos(unsigned long currentTime) {
      this->x = _parent->x + _attachToParentX;
      if (_parent->_enableBreathing) {
        if (_attachToParentY > _parent->getBreathPosition()) {
          this->y = _parent->y + _attachToParentY;
        } else {
          //calculate breathing up and down position
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
};
#endif
