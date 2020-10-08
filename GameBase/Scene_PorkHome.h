#ifndef _SCENE_PORKHOME_H_
#define _SCENE_PORKHOME_H_
#include "GameScene.h"
#include "Avatar.h"
#include "Attachment.h"
#include "Physics.h"
#include "BackgroundPorkHome.h"
#include "cat.h"
#include "pork.h"
#include "dragon.h"
#include "shrimp.h"
#include "pork2.h"
#include "pitches.h"
#include "cookie.h"
#include "macaron.h"
#include "cake.h"
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
        addSound(melody[thisNote], noteDuration);
        //tone(16, melody[thisNote], noteDuration);

        // to distinguish the notes, set a minimum time between them.
        // the note's duration + 30% seems to work well:
        //int pauseBetweenNotes = noteDuration * 1.30;
//        delay(pauseBetweenNotes);
//        int pauseBetweenNotes = noteDuration * 0.3;
//        addSound(0, pauseBetweenNotes);
        // stop the tone playing:
        //noTone(16);
      }
    }

    void playMusicDelay(){
      for (int thisNote = 0; thisNote < 8; thisNote++) {

        // to calculate the note duration, take one second divided by the note type.
        //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
        int noteDuration = 1000 / noteDurations[thisNote];
          Serial.print("Play Tone ");
          Serial.print(melody[thisNote]);
          Serial.print(" Duration  ");
          Serial.println(noteDuration);
        tone(16, melody[thisNote], noteDuration);

        // to distinguish the notes, set a minimum time between them.
        // the note's duration + 30% seems to work well:
        int pauseBetweenNotes = noteDuration * 1.30;
        delay(pauseBetweenNotes);
        
        Serial.println("no Tone");        
        // stop the tone playing:
        noTone(16);
      }
    }

    void update(boolean isTouching, uint16_t touchX, uint16_t touchY, boolean* needChangeScene, int* nextSceneIndex) {

 
      if (isTouching) {
        //Check if touching on Avatar id = 2 (Left )/ 3 (Right)
        if(!wasTouching){
          if(touchX > avatar3->x && touchX  < avatar3->x + avatar3->width && touchY > avatar3->y && touchY < avatar3->y  + avatar3->height){
            avatar2->setVelocity(-1, 0);
            isTouchingButton = true;
            playMusic();
          }else if(touchX > avatar4->x && touchX  < avatar4->x + avatar3->width && touchY > avatar4->y && touchY < avatar4->y  + avatar4->height){
            avatar2->setVelocity(1, 0);
            isTouchingButton = true;
            playMusicDelay();
          }
        }

        //macaronAvatar->setPos(touchX, touchY);
        
        wasTouching = true;
      } else {
        if (wasTouching) { //tap up
          if(!isTouchingButton){
//            *needChangeScene = true;
//            *nextSceneIndex = 0;
//            return;
          }else{
            avatar2->setVelocity(0, 0);
            isTouchingButton =  false;  
          }
        }
        wasTouching = false;
      }


      unsigned long currentElapse = millis();
      //Suppose we use Constant Game update speed
      for (int i = 0; i < numAvatar; ++i) {
        Avatar* avatar = avatars[i];
        
        if(avatar == macaronAvatar){  
          macaronAvatar->updatePos(currentElapse);
        }else if (avatar == cookieAttachment){
          cookieAttachment->updatePos(currentElapse);
        }else{
          avatar->updatePos(currentElapse);
          boundToScreen(avatar);
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
    }

    void initScene() {
      setBackground(BackgroundPorkHome);
      wasTouching = false;
      stage = 0;
      
      avatar1 = new Avatar(97, 190, 87, 99, Pork2, Pork2Mask); //87x99 pixels
      avatar1->id = 0;
      appendAvatar(avatar1);
      //avatar1->enableBreathing = true;
      avatar1->enableBreathing();
      avatar1->setBreathInterval(400);
//      avatar1->breathInterval = 400;
      //avatar1->breathPosition = 20;
      avatar1->setBreathPosition(20);
      avatar1->breathAmount = 4;
      avatar1->setVelocity(-8, 0);
      avatar1->updateInterval = 600
      ;
      
      macaronAvatar = new Attachment(18, -18, avatar1, 25, 21, macaron, macaronMask);
      appendAvatar(macaronAvatar); //append to Avatar list to that the renderScene function will be able to see this 

      cookieAttachment = new Attachment( 10, 80, avatar1, 25, 23, cookie, cookieMask);
      appendAvatar(cookieAttachment);
      
      avatar2 = new Avatar(20, 229, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask);
      avatar2->id = 1;
      //avatar2->breathInterval = 500;
      //avatar2->enableBreathing = true;
      avatar2->setBreathInterval(500);
      avatar2->enableBreathing();
      //avatar2->breathPosition = 15;
      avatar2->setBreathPosition(15);
      avatar2->breathAmount = 2;
      appendAvatar(avatar2);
      

      avatar3 = new Avatar(20, 150, 25, 22, cake, cakeMask);
      avatar3->id = 2;
      avatar3->setVelocity(1, 1);
      appendAvatar(avatar3);
      

      avatar4 = new Avatar(200, 150,25, 23, cookie, cookieMask);
      avatar4->id = 3;
      avatar4->setVelocity(1, 1);
      appendAvatar(avatar4); 

      Avatar *avatarX = new Avatar(30, 200, 25, 22, cake, cakeMask);
      avatarX->setVelocity(1, 1);
      appendAvatar(avatarX);

      avatarX = new Avatar(50, 300, 25, 22, cake, cakeMask);
      avatarX->setVelocity(1, 1);
      appendAvatar(avatarX);

      avatarX = new Avatar(90, 100, 25, 22, cake, cakeMask);
      avatarX->setVelocity(1, 1);
      appendAvatar(avatarX);
      
      drawBackground(BackgroundPorkHome);
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

  private :
    boolean wasTouching = false;
    boolean isTouchingButton = false;
    int stage = 0;
    Avatar *avatar1 = NULL;
    Avatar *avatar2 = NULL;

    Avatar *avatar3 = NULL; //cake
    Avatar *avatar4 = NULL; //cookie

    Attachment *macaronAvatar = NULL;//macaron
    Attachment *cookieAttachment = NULL;
};

#endif
