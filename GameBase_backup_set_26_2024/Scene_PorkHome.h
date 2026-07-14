#ifndef _SCENE_PORKHOME_H_
#define _SCENE_PORKHOME_H_
#include "GameScene.h"
#include "Avatar.h"
#include "Attachment.h"
#include "Physics.h"
#include "image_background1.h"



#include "pitches.h"
#include "cookie.h"

#include "cake.h"

//#define DEBUG_PORKHOME
//#define SERIALDEBUG_PORKHOME
class Scene_PorkHome : public GameScene {
  public:
    Scene_PorkHome(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    };

    // notes in the melody:
    int melody[8] = {
      NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
    };

    // note durations: 4 = quarter note, 8 = eighth note, etc.:
    int noteDurations[8] = { 4, 8, 8, 4, 4, 4, 4, 4};

    //unsigned long debugTimer = 0;
    void playMusic() {
#ifdef DEBUG_PORKHOME
      _tft->drawString("Play Music", 0, 30, 2);
#endif
      for (int thisNote = 0; thisNote < 8; thisNote++) {
        // to calculate the note duration, take one second divided by the note type.
        //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
        int noteDuration = 1000 / noteDurations[thisNote];
        addSound(melody[thisNote], noteDuration);
      }
    }

    void onTouchStart(boolean * needChangeScene, int* nextSceneIndex) {
      uint16_t touchX = 0;
      uint16_t touchY = 0;
      _tft->getTouch(&touchX, &touchY);

      //Check if pressing on the floating Cookie.
      //Play sound when it is pressed on
      if (avatar4->contains(touchX, touchY)) {
        playMusic();
      }
//
//      if (avatar1->contains(touchX, touchY)) {
//        *needChangeScene = true;
//        *nextSceneIndex = 1;
//      }
    }

    void onTouchEnd(boolean* needChangeScene, int* nextSceneIndex) {
    }

    void update(boolean isTouching, boolean* needChangeScene, int* nextSceneIndex) {
      if (isTouching) {
        if (!wasTouching) {
          onTouchStart(needChangeScene,  nextSceneIndex);
        }
        wasTouching = true;
      } else {
        if (wasTouching) {
          //When Touch ends
          onTouchEnd(needChangeScene,  nextSceneIndex);
        }
        wasTouching = false;
      }

      unsigned long currentElapse = millis();
      //Suppose we use Constant Game update speed

      //      for (int i = 0; i < numAvatar; ++i) {
      //        Avatar* avatar1 = avatars[i];
      //        for (int j = 0; j < numAvatar; ++j) {
      //          if (i == j) {
      //            continue;
      //          }
      //          Avatar* avatar2 = avatars[j];
      //          if (physics::aabbTest(*avatar1, *avatar2)) {
      //
      //            physics::resolveCollision(avatar1, avatar2);
      //
      //          }
      //        }
      //      }
      //First update the main characters
      for (int i = 0; i < numAvatar; ++i) {
        Avatar* avatar = avatars[i];

        if (avatar == cookieAttachment) {
          cookieAttachment->updatePos(currentElapse);
        } else if (avatar == macaronAvatar) {
          macaronAvatar->updatePos(currentElapse);
        } else {
          avatar -> updatePos(currentElapse);
        }
        //boundToVirtualScreen(avatar);
        boundToScreen(avatar);
      }


    }


    void render() {
      renderScene();
    }

    void initScene() {

      setBackground(game_bg1);
      wasTouching = false;
      stage = 0;

//      avatar1 = new Avatar(97, 190, 87, 99, Pork2, Pork2Mask); //87x99 pixels
//      avatar1->id = 0;
//      appendAvatar(avatar1);
//
//      avatar1->enableBreathing();
//      avatar1->setBreathInterval(400);
//      avatar1->setBreathPosition(79);
//      avatar1->breathAmount = 4;
//      avatar1->setVelocity(-10, 10);
//      //avatar1->updateInterval = 600; //With this setting, the avatar will slow down
//
//      cookieAttachment = new Attachment( 15, 55, avatar1, 25, 23, cookie, cookieMask);
//      appendAvatar(cookieAttachment);
//
//      avatar2 = new Avatar(20, 229, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask);
//      avatar2->id = 1;
//
//      avatar2->setBreathInterval(500);
//      avatar2->enableBreathing();
//      avatar2->setBreathPosition(15);
//      avatar2->breathAmount = 2;
//      avatar2->setVelocity(-5, 5);
//      appendAvatar(avatar2);
//
//      macaronAvatar = new Attachment(0, 0, avatar2, 25, 21, macaron, macaronMask);
//      appendAvatar(macaronAvatar); //append to Avatar list to that the renderScene function will be able to see this

      avatar3 = new Avatar(20, 150, 25, 22, cake, cakeMask);
      avatar3->id = 2;
      avatar3->setVelocity(1, 1);
      appendAvatar(avatar3);


      avatar4 = new Avatar(200, 150, 25, 23, cookie, cookieMask);
      avatar4->id = 3;
      avatar4->setVelocity(5, 5);
      appendAvatar(avatar4);

      Avatar *avatarX = new Avatar(30, 200, 25, 22, cake, cakeMask);
      avatarX->setVelocity(3, 3);
      appendAvatar(avatarX);

      avatarX = new Avatar(50, 300, 25, 22, cake, cakeMask);
      avatarX->setVelocity(4, 4);
      appendAvatar(avatarX);

      avatarX = new Avatar(90, 100, 25, 22, cake, cakeMask);
      avatarX->setVelocity(4, 3);
      appendAvatar(avatarX);

      drawBackground(game_bg1);

    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private :
    boolean wasTouching = false;

    int stage = 0;
    Avatar *avatar1 = NULL;
    Avatar *avatar2 = NULL;

    Avatar *avatar3 = NULL; //cake
    Avatar *avatar4 = NULL; //cookie

    Attachment *macaronAvatar = NULL;//macaron
    Attachment *cookieAttachment = NULL;

    boolean boundToVirtualScreen(Avatar* avatar) {
      int virtualGap = 80;
      boolean reversed = false;
      if (avatar->x <= -80) {
        avatar->velocity.x *= -1.0;//Change the movement direction
        avatar->x = -80;
        reversed = true;
      } else if ( (avatar->x + avatar->width) >= SCREENWIDTH + 80) {
        avatar->x = SCREENWIDTH + 80 - avatar->width;
        avatar->velocity.x *= -1.0;
        reversed = true;
      }


      if (avatar->y <= -80) {
        avatar->y = -80;
        avatar->velocity.y *= -1.0;
        reversed = true;
      } else if ((avatar->y + avatar->height) >= SCREENHEIGHT + 80) {
        avatar->y = SCREENHEIGHT + 80 - avatar->height;
        avatar->velocity.y *= -1.0;
        reversed = true;
      }

      return reversed;
    }
};

#endif
