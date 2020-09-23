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

    //    void debug(){
    //      Avatar* avatar = avatars[0];
    //      Serial.print("Avatar0 (x, y): ");
    //      Serial.print(avatar->x);
    //      Serial.print(", ");
    //      Serial.print(avatar->y);
    //      Serial.print(" (velocity x, y): ");
    //      Serial.print(avatar->velocity.x);
    //      Serial.print(", ");
    //       Serial.println(avatar->velocity.y);
    //
    //      avatar = avatars[1];
    //      Serial.print("Avatar1 (x, y): ");
    //      Serial.print(avatar->x);
    //      Serial.print(", ");
    //      Serial.print(avatar->y);
    //      Serial.print(" (velocity x, y): ");
    //      Serial.print(avatar->velocity.x);
    //      Serial.print(", ");
    //       Serial.println(avatar->velocity.y);
    //    }

    //unsigned long debugTimer = 0;
    void update(boolean isTouching, uint16_t touchX, uint16_t touchY, boolean* needChangeScene, int* nextSceneIndex) {

      //      if(millis() - debugTimer > 1000){
      //        debug();
      //        debugTimer = millis();
      //      }
      //

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
      for(int i = 0; i < numAvatar; ++i){
        Avatar* avatar = avatars[i];
        avatar->updatePos();
        if(avatar->id == 0){
          if(avatar->y == 221){
            avatar->setVelocity(0, 0);
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

    void boundToScreen(Avatar* avatar) {
      if ( (avatar->x <= 0) || ((avatar->x + avatar->width) >= SCREENWIDTH)) {
        avatar->velocity.x *= -1;//Change the movement direction
        avatar->x = (avatar->x <= 0) ? 0 : SCREENWIDTH - avatar->width ; //Clip the image within the screen
      }
      if (avatar->y <= 0) {
        avatar->y = 0;
        //
#ifdef PHYSICS
        avatar->velocity.y *= -0.8; //We hit the top, lost energy
#else
        avatar->velocity.y *= -1;
#endif

        //avatar->dy *= -1;
      } else if ((avatar->y + avatar->height) >= SCREENHEIGHT) {
        avatar->y = SCREENHEIGHT - avatar->height;
        //
#ifdef PHYSICS        
        avatar->velocity.y *= -0.85; //suppose we have energy lost in both x and y when hit the floor
        avatar->velocity.x *= 0.9;
#else
        avatar->velocity.y *= -1;
#endif
      }
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
      wasTouching = false;
      //if (avatar1 == NULL){
      Avatar* avatar = NULL;

//      avatar = new Avatar(0, 0, CAT_WIDTH, CAT_HEIGHT, CatBitmap, CatMask);
//      avatar->setVelocity(1, 1);
//      appendAvatar(avatar);
//
//      avatar = new Avatar(50, 0, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask);
//      avatar->setVelocity(-1, 1);
//      appendAvatar(avatar);
//
//      avatar = new Avatar(100, 0, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask);
//      avatar->setVelocity(1, 1);
//      appendAvatar(avatar);
//
//      avatar = new Avatar(150, 0, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask);
//      avatar->setVelocity(-1, -1);
//      appendAvatar(avatar);

      avatar = new Avatar(120, 320, 87, 99, Pork2, Pork2Mask);
      avatar->id = 0;
      appendAvatar(avatar);
      avatar->setVelocity(0, -1);
    
      drawBackground(friedPorkHome);
      Serial.print("NumOfAvatars: ");
      Serial.println(numAvatar);
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private :
    boolean wasTouching = false;

};

#endif
