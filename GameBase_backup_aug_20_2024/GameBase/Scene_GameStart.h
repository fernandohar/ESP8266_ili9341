#ifndef _SCENE_GAMESTART_H
#define _SCENE_GAMESTART_H
#include "GameScene.h"
#include "Avatar.h"
#include "cinnamoroll_hands_up.h"
#include "chiffon_hands_up.h"
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
    

    
    void update(boolean isTouching, boolean* needChangeScene, int* nextSceneIndex) {
      if (isTouching) {

        if (!wasTouching) {
          //tap down

          //get status of Start button
          uint16_t touchX = 0;
          uint16_t touchY = 0;
          _tft->getTouch(&touchX, &touchY);
          if (startBtn.contains(touchX, touchY)) {
            startButtonPressed = true;
            startBtn.drawButton(true, "");
            addSound(NOTE_C5, 8);
          }
        }
        wasTouching = true;
      } else {
        if (wasTouching) {
          //Just released

          if (startButtonPressed) {
            addSound(NOTE_G6, 8);
            //Simulate disappear of start button
            uint16_t color = rgb565(230, 157, 132);
            _tft->fillScreen(color);
            changeSceneTimer = millis() + 100;
            startButtonPressed = false;

          }
        }
        wasTouching = false;
      }
      unsigned long current = millis();

      boolean tempAllGrounded = true;
      for (int i = 0; i < numAvatar; ++i) {
        Avatar* avatar = avatars[i];
        if (avatar != NULL) {
          avatar->updatePos(current);
          //If the avatar touches the floor, do not move it
          if ( avatar->y >= (SCREENHEIGHT - avatar->height)) {
            avatar->setVelocity(0, 0);
          } else {
            tempAllGrounded = false;
          }
        }
      }

      // the first time all avatars grounded, show start button
      if (tempAllGrounded && !allAvatarsGounded) {
        allAvatarsGounded = true;
        drawStartBtn();
        addSound(NOTE_C4, 100); //Middle C
        addSound(NOTE_C5, 100);
        addSound(REST, 1000);

        addMelody();
      }

      if (changeSceneTimer > 0 && millis() > changeSceneTimer) {
        //tap up
        *needChangeScene = true;
        *nextSceneIndex = 1;
      }
    }

    void render() {
      renderScene();
    }

    void initScene() {
      setBackgroundColor(GameScene::rgb565(230, 157, 132));
      wasTouching = false;
      Avatar* avatar = NULL;

      //Lowest layer ... top layer


      avatar = new Avatar(20, -1 * CINNAMOROLL_HANDS_UP_HEIGHT, CINNAMOROLL_HANDS_UP_WIDTH, CINNAMOROLL_HANDS_UP_HEIGHT, cinnamoroll_hands_up, cinnamoroll_hands_up_mask); //120x58px
      avatar->enableBreathing();
      avatar->setBreathInterval(500);
      avatar->setBreathPosition(30);
      avatar->breathAmount = 2;
      avatar->setVelocity(0.0, 9.8);
      appendAvatar(avatar);


      avatar = new Avatar (120, -51, CHIFFON_HANDS_UP_WIDTH, CHIFFON_HANDS_UP_HEIGHT, chiffon_hands_up, chiffon_hands_up_mask); //120 X 58
      avatar->enableBreathing();
      avatar->setBreathInterval(100);
      avatar->setBreathPosition(30);
      avatar->breathAmount = 2;
      avatar->setVelocity(0.0, 9.8);
      appendAvatar(avatar);
//
//      avatar = new Avatar (167, -60, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask);//44x60
//      avatar->enableBreathing();
//      avatar->setBreathInterval(150);
//      avatar->setBreathPosition(30);
//      avatar->breathAmount = 2;
//      avatar->setVelocity(0.0, 9.8);
//      appendAvatar(avatar);

      uint16_t color = rgb565(230, 157, 132);
      _tft->fillScreen(color);
    }

    void destroyScene() {
      wasTouching = false;
      GameScene::destroyScene();
    }

    void addMelody() {
      int notes = sizeof(melody) / sizeof(melody[0]) / 2;
      int tempo = 160;
      int wholenote = (60000 * 4) / tempo;
      for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
        addSound(melody[thisNote], wholenote / melody[thisNote + 1]);
      }
    }
  private :
    boolean wasTouching = false;
    boolean startButtonPressed = false;
    unsigned long changeSceneTimer = 0;
    TFT_eSPI_Button startBtn;
    boolean allAvatarsGounded = false;


    void drawStartBtn() {
      char* startBtnText = "START";
      startBtn.initButton(_tft, 120, 160, 150, 50, TFT_BLACK, TFT_CYAN, // Fill
                          TFT_BLACK, startBtnText, 3);
      startBtn.drawButton(false, "");
    }

    
};
#endif
