#ifndef _ATTACHMENT_H_
#define _ATTACHMENT_H_

#include "Avatar.h"
class Avatar; //Avatar and Attachment references each other

class Attachment{
  uint16_t x; //position of parent it attach to
  uint16_t y;
  Avatar *thisAvatar = NULL;
};
#endif
