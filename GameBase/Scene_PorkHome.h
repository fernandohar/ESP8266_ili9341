#ifndef _SCENE_PORKHOME_H_
#define _SCENE_PORKHOME_H_
#include "GameScene.h"
#include "Avatar.h"
#include "Physics.h"
#include "Bitmap_Background.h"
#include "cat.h"
#include "pork.h"
#include "dragon.h"
#include "shrimp.h"
#include "pork2.h"
#include "pitches.h"

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
      for (int thisNote = 0; thisNote < 8; thisNote++) {

        // to calculate the note duration, take one second divided by the note type.
        //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
        int noteDuration = 1000 / noteDurations[thisNote];
        tone(16, melody[thisNote], noteDuration);

        // to distinguish the notes, set a minimum time between them.
        // the note's duration + 30% seems to work well:
        int pauseBetweenNotes = noteDuration * 1.30;
        delay(pauseBetweenNotes);
        // stop the tone playing:
        noTone(16);
      }
    }

    void update(boolean isTouching, uint16_t touchX, uint16_t touchY, boolean* needChangeScene, int* nextSceneIndex) {

 
      if (isTouching) {

        wasTouching = true;
      } else {
        if (wasTouching) { //tap up
          *needChangeScene = true;
          *nextSceneIndex = 0;
          return;
        }
        wasTouching = false;
      }


      //Suppose we use Constant Game update speed
      for (int i = 0; i < numAvatar; ++i) {
        Avatar* avatar = avatars[i];
        avatar->updatePos();
        if(avatar->id == 0){
          if(stage == 0){
            if (avatar->y == 221) {
              avatar->setVelocity(-0.2, 0);
              //playMusic();
              stage++;
              avatar2->x = avatar->x - 25;
              avatar2->y = avatar->y + 39;
            }
          }else if (stage == 1){
            if(boundToScreen(avatar)){
              avatar2->setVelocity( avatar->velocity.x, avatar->velocity.y );
            }
          }
        }
      }

      //      for (int i = 0; i < numAvatar; ++i) {
      //        Avatar* avatar = avatars[i];
      //        if (avatar != NULL) {
      //#ifdef PHYSICS
      //          if (avatar->velocity.y == 0 && (avatar->y + avatar->height) < SCREENHEIGHT) {
      //            avatar->velocity.y = 0.1;
      //          }
      //
      //          avatar->velocity.y +=  0.2;
      //#endif
      //          avatar->updatePos();
      //          boundToScreen(avatar);
      //
      //        }
      //      }
      //      for (int i = 0; i < numAvatar; ++i) {
      //        Avatar* avatar1 = avatars[i];
      //        for (int j = 0; j < numAvatar; ++j) {
      //          if (i == j) {
      //            continue;
      //          }
      //          Avatar* avatar2 = avatars[j];
      //#ifdef PHYSICS
      //          if (physics::aabbTest(*avatar1, *avatar2)) {
      //
      //            physics::resolveCollision(avatar1, avatar2);
      //
      //          }
      //#endif
      //        }
      //      }

    }

    boolean boundToScreen(Avatar* avatar) {
      boolean reversed = false;
      if ( (avatar->x <= 0) || ((avatar->x + avatar->width) >= SCREENWIDTH)) {
        avatar->velocity.x *= -1;//Change the movement direction
        avatar->x = (avatar->x <= 0) ? 0 : SCREENWIDTH - avatar->width ; //Clip the image within the screen
        reversed = true;
      }
      if (avatar->y <= 0) {
        avatar->y = 0;
        //
#ifdef PHYSICS
        avatar->velocity.y *= -0.8; //We hit the top, lost energy
#else
        avatar->velocity.y *= -1;
#endif
        reversed = true;
      } else if ((avatar->y + avatar->height) >= SCREENHEIGHT) {
        avatar->y = SCREENHEIGHT - avatar->height;
        
#ifdef PHYSICS
        avatar->velocity.y *= -0.85; //suppose we have energy lost in both x and y when hit the floor
        avatar->velocity.x *= 0.9;
#else
        avatar->velocity.y *= -1;
#endif
        reversed = true;
      }
      
      return reversed;
    }


    void render() {
      renderScene();
      //      for (int i = 0; i < numAvatar; ++i) {
      //        Avatar* avatar = avatars[i];
      //        if (avatar != NULL) {
      //          renderCharacter(avatar->previousRenderedX, avatar->previousRenderedY, avatar->x, avatar->y, avatar->width, avatar->height, avatar->bitmap, avatar->mask, SCREENWIDTH); //temp
      //          avatar->savePreviousRenderPos();
      //        }
      //      }
    }

    void initScene() {
      setBackground(friedPorkHome);
      wasTouching = false;
      stage = 0;
      
      avatar1 = new Avatar(120, 320, 87, 99, Pork2, Pork2Mask);
      avatar1->id = 0;
      appendAvatar(avatar1);
      avatar1->setVelocity(0, -1);
      avatar1->enableBreathing = true;
      avatar1->breathDuration = 40;
      avatar1->breathPosition = 20;
      avatar1->breathAmount = 3;

      
      avatar2 = new Avatar(241, 321, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask);
      avatar2->id = 1;
      avatar2->breathDuration = 20;
      avatar2->enableBreathing = true;
      avatar2->breathPosition = 15;
      avatar2->breathAmount = 2;
      appendAvatar(avatar2);
      
      drawBackground(friedPorkHome);
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
};

#endif
