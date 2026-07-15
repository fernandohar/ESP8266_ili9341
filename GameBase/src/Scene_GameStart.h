#ifndef _SCENE_GAMESTART_H
#define _SCENE_GAMESTART_H
#include <Arduino.h>
#include "GameScene.h"
#include "Avatar.h"
#include "cinnamoroll_hands_up.h"
#include "Image_cinnamoroll_sit.h"
#include "chiffon_hands_up.h"
#include "image_background1.h"
#include "image_background2.h"
#include "cake.h"
#include "cookie.h"

#if defined(WOKWI_CAP_TOUCH)
extern bool readTouchPoint(uint16_t *x, uint16_t *y);
#endif

int melody[] = {
  // Keyboard cat
  // Score available at https://musescore.com/user/142788/scores/147371
    REST,1,
    REST,1,
    NOTE_C4,4, NOTE_E4,4, NOTE_G4,4, NOTE_E4,4, 
};

class Scene_GameStart : public GameScene {
  public:
    Scene_GameStart(TFT_eSPI *tft) {
      GameScene::_tft = tft;
    };

    void changeScene() {
      changeSceneTimer = millis() + 100;
    }

    void handleChangeScene(boolean* needChangeScene, int* nextSceneIndex) {
      if (changeSceneTimer > 0 && millis() > changeSceneTimer) {
        //tap up
        *needChangeScene = true;
        *nextSceneIndex = 1;
      }
    }

    //Helper function for moveSceneLeft and moveScreenRight
    void moveAllCharacters(int16_t deltaX) {
      Avatar* avatar;
      for (int i = 0; i < numAvatar; ++i) {
        avatar = avatars[i];
        if (avatar != NULL && (avatar == cinnamoroll || avatar == chiffon)) {
          avatar->setPos( avatar->x - deltaX, avatar->y);
        }
      }
    }
    
    void moveSceneLeft(uint16_t amount) {
      if (currentBgOffset > 0) {
        uint16_t originalX = currentBgOffset;
        currentBgOffset = (currentBgOffset > amount) ? (currentBgOffset - amount) : 0;
        setBackgroundOffset(currentBgOffset);
        refreshBackground = true;
        //drawBackground(game_bg2, currentBgOffset);
        //how much we moved the screen
        int16_t delta = currentBgOffset - originalX;
        moveAllCharacters(delta);
        requestRender();
      }
    }
    void moveSceneRight(uint16_t amount ) {
      if (currentBgOffset < 272 ) {
        uint16_t originalX = currentBgOffset;
        currentBgOffset = (currentBgOffset + amount);
        if (currentBgOffset > 272) {
          currentBgOffset = 272;
        }
        setBackgroundOffset(currentBgOffset);
        refreshBackground = true;
        //drawBackground(game_bg2, currentBgOffset);
        //how much we moved the screen
        int16_t delta = currentBgOffset - originalX;
        moveAllCharacters(delta);
        requestRender();
      }
    }
    
    void update(boolean isTouching, boolean* needChangeScene, int* nextSceneIndex) {
      if (isTouching) {
        //Serial.printf("isTouching: %s\n \n\n", isTouching ? "true" : "false");
        if (!wasTouching) {
          //Serial.printf("wasTouching: %s\n \n\n", wasTouching ? "true" : "false");

          touchTimer = millis();
          //tap down

          //get status of Start button
          uint16_t touchX = 0;
          uint16_t touchY = 0;
          getTouchPoint(&touchX, &touchY);
          if (cakeAvatar->contains(touchX, touchY)) {
            cakePressed = true;
            moveSceneLeft(10);
          } else if (cookieAvatar->contains(touchX, touchY)) {
            cookiePressed = true;
            moveSceneRight(10);
          } else if (cinnamoroll != NULL && cinnamoroll->contains(touchX, touchY)){
            addSound(NOTE_D5, noteDurationMs(16, 800));
            addSound(NOTE_D6, noteDurationMs(16, 800));
            addSound(NOTE_D7, noteDurationMs(16, 800));
            addSound(NOTE_D8, noteDurationMs(16, 800));
            cinnamorollPressed = true;
            cinnamoroll->setVelocity(0, 0);
            cinnamorollGrabOffsetX = (float)touchX - cinnamoroll->x;
            cinnamorollGrabOffsetY = (float)touchY - cinnamoroll->y;
          } else if (chiffon != NULL && chiffon->contains(touchX, touchY)) {
            addSound(NOTE_C5, noteDurationMs(16, 600));
            addSound(NOTE_C6, noteDurationMs(16, 600));
            addSound(NOTE_C7, noteDurationMs(16, 600));
            addSound(NOTE_C8, noteDurationMs(16, 600));
            chiffonPressed = true;
            chiffon->setVelocity(0, 0);
            chiffonGrabOffsetX = (float)touchX - chiffon->x;
            chiffonGrabOffsetY = (float)touchY - chiffon->y;
          }
        } else {
          //touch is holding down
          if (cinnamorollPressed || chiffonPressed) {
            uint16_t touchX = 0;
            uint16_t touchY = 0;
            if (!getTouchPoint(&touchX, &touchY)) { return; }

            // Possible hardware error? some times touch returns 0, 0
            if (touchX == 0 && touchY == 0){ return; }

            if (cinnamorollPressed) {
              float newX = (float)touchX - cinnamorollGrabOffsetX;
              float newY = (float)touchY - cinnamorollGrabOffsetY;
              if (newX != cinnamoroll->x || newY != cinnamoroll->y) {
                cinnamoroll->setPos(newX, newY);
                requestRender();
              }
            }
            if (chiffonPressed) {
              float newX = (float)touchX - chiffonGrabOffsetX;
              float newY = (float)touchY - chiffonGrabOffsetY;
              if (newX != chiffon->x || newY != chiffon->y) {
                chiffon->setPos(newX, newY);
                requestRender();
              }
            }
          }
          if ((millis() - touchTimer) > 100) {
            touchTimer = millis();

            if (cakePressed) {
              moveSceneLeft(5);
            } else if (cookiePressed) {
              moveSceneRight(5);
            }
          }
        }
        wasTouching = true;
      } else {
        if (wasTouching) {
          //Just released
          if (cakePressed) {
            //moveSceneLeft(50);
            //disableDebug();
          } else if (cookiePressed) {
            //moveSceneRight(50);
            //enableDebug();
          } else if (cinnamorollPressed) {
            markAvatarsUnder(cinnamoroll);
            requestRender();
          } else if (chiffonPressed) {
            markAvatarsUnder(chiffon);
            requestRender();
          }
        }
        cakePressed = false;
        cookiePressed = false;
        cinnamorollPressed = false;
        chiffonPressed = false;
        wasTouching = false;
      }
      unsigned long current = millis();

      
      boolean avatarAppeared = false;
      for (int i = 0; i < numAvatar; ++i) {
        Avatar* avatar = avatars[i];
        if (avatar != NULL) {
          avatar->updatePos(current);
          if (avatar == cinnamoroll) {
            if ( avatar->x <= 75) {
              avatar->setVelocity(0, 0);
              avatarAppeared = true;
            }
          } else if (avatar == chiffon) {
            if (avatar->y >= (300 - CHIFFON_HANDS_UP_HEIGHT)) {
              avatar->setVelocity(0, 0);
              
            }
          }
        }
      }

      // the first time all avatars grounded, show start button
      if (avatarAppeared && !allAvatarsPositionFixed) {
        allAvatarsPositionFixed = true;
        addSound(NOTE_C4, noteDurationMs(4));
        //addSound(NOTE_C5, noteDurationMs(4));
        addSound(REST, noteDurationMs(1));
      }
      if (changeSceneTimer > 0 && millis() > changeSceneTimer) {
        //tap up
        *needChangeScene = true;
        *nextSceneIndex = 1;
      }
      
    }

    void render() {
      if (refreshBackground) {
        refreshBackground = false;
        // One full-screen pass (background + avatars). Avoids drawBackground + renderScene.
        renderFullScreen();
        return;
      }
      renderScene();
    }

    void initScene() {
      setBackground(game_bg2, GAME_BG2_WIDTH);
      drawBackground(game_bg2, 136);
      wasTouching = false;
      
     //Lowest layer ... top layer
     chiffon = new Avatar (120, -1000, CHIFFON_HANDS_UP_WIDTH, CHIFFON_HANDS_UP_HEIGHT, chiffon_hands_up, chiffon_hands_up_mask); //120 X 58
     chiffon->setVelocity(0.0, 9.8);
     appendAvatar(chiffon);
     
     cinnamoroll = new Avatar(240, 242, CINNAMOROLL_SIT_WIDTH, CINNAMOROLL_SIT_HEIGHT, cinnamoroll_sit_frame_3, cinnamoroll_sit_frame_3_mask); //119 X 73 px
     cinnamoroll->setFrameUpdatetime(130);
     cinnamoroll->addFrame(cinnamoroll_sit_frame_4, cinnamoroll_sit_frame_4_mask);
     cinnamoroll->addFrame(cinnamoroll_sit_frame_0, cinnamoroll_sit_frame_0_mask);
     cinnamoroll->addFrame(cinnamoroll_sit_frame_1, cinnamoroll_sit_frame_1_mask);
     cinnamoroll->addFrame(cinnamoroll_sit_frame_2, cinnamoroll_sit_frame_2_mask);
     
     cinnamoroll->setVelocity(-5, 0);
     appendAvatar(cinnamoroll);


      cakeAvatar = new Avatar(10, 160, 25, 22, cake, cakeMask);
      appendAvatar(cakeAvatar);
      
      cookieAvatar = new Avatar(205,160, 25, 23, cookie, cookieMask);
      appendAvatar(cookieAvatar);
      //enableDebug();
      
      disableDebug();
      delay(1000);
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

    void addMelody() {
      int notes = sizeof(melody) / sizeof(melody[0]) / 2;
      int tempo = 160;
      for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
        addSound(melody[thisNote], noteDurationMs(melody[thisNote + 1], tempo));
      }
    }
  private :
    bool getTouchPoint(uint16_t *touchX, uint16_t *touchY) {
#if defined(WOKWI_CAP_TOUCH)
      return readTouchPoint(touchX, touchY);
#else
      return _tft->getTouch(touchX, touchY);
#endif
    }

    boolean refreshBackground = true;
    boolean wasTouching = false;
    boolean cakePressed = false;
    boolean cookiePressed = false;
    boolean cinnamorollPressed = false;
    boolean chiffonPressed = false;
    unsigned long changeSceneTimer = 0;
    unsigned long touchTimer = 0;
    //TFT_eSPI_Button startBtn;
    boolean allAvatarsPositionFixed = false;
    //Avatar* backgroundPic = NULL;
    Avatar* chiffon = NULL;
    Avatar* cinnamoroll = NULL;
    Avatar* cakeAvatar = NULL;
    Avatar* cookieAvatar = NULL;
    float cinnamorollGrabOffsetX = 0;
    float cinnamorollGrabOffsetY = 0;
    float chiffonGrabOffsetX = 0;
    float chiffonGrabOffsetY = 0;
    uint16_t currentBgOffset = 136;  //Default Startup position of the background = 136
//    void drawStartBtn() {
//      char* startBtnText = "START";
//      startBtn.initButton(_tft, 120, 200, 150, 30, TFT_BLACK, TFT_CYAN, // Fill
//                          TFT_BLACK, startBtnText, 3);
//      startBtn.drawButton(false, "");
//    }
};
#endif
