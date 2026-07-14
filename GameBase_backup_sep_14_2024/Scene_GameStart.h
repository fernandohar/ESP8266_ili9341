#ifndef _SCENE_GAMESTART_H
#define _SCENE_GAMESTART_H
#include "GameScene.h"
#include "Avatar.h"
#include "cinnamoroll_hands_up.h"
#include "Image_cinnamoroll_sit.h"
#include "chiffon_hands_up.h"
#include "Background1.h"
#include "Image_gameTitle.h"
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

      boolean titleAppeared= false;
      boolean avatarAppeared = false;
      for (int i = 0; i < numAvatar; ++i) {
        Avatar* avatar = avatars[i];
        if (avatar != NULL) {
          avatar->updatePos(current);

          if ( avatar == gameTitle ) {
            if ( avatar->y >= 54) {
              avatar->setVelocity(0, 0);
              titleAppeared = true;
            }
          } else {
            if ( avatar->x <= 75) {
              avatar->setVelocity(0, 0);
              avatarAppeared = true;
            }
          }
        }
      }

      // the first time all avatars grounded, show start button
      if (titleAppeared && avatarAppeared && !allAvatarsPositionFixed) {
        allAvatarsPositionFixed = true;
        //drawStartBtn();
        addSound(NOTE_C4, 20); //Middle C
        addSound(NOTE_C5, 20);
        addSound(REST, 1000);

        //addMelody();
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
      setBackground(game_bg1);
      wasTouching = false;
//      Avatar* avatar = NULL;

      //Lowest layer ... top layer
      cinnamoroll = new Avatar(240, 242, CINNAMOROLL_SIT_WIDTH, CINNAMOROLL_SIT_HEIGHT, cinnamoroll_sit_frame_3, cinnamoroll_sit_frame_3_mask); //119 X 73 px
      cinnamoroll->setFrameUpdatetime(100);
      cinnamoroll->addFrame(cinnamoroll_sit_frame_4, cinnamoroll_sit_frame_4_mask);
      cinnamoroll->addFrame(cinnamoroll_sit_frame_0, cinnamoroll_sit_frame_0_mask);
      cinnamoroll->addFrame(cinnamoroll_sit_frame_1, cinnamoroll_sit_frame_1_mask);
      cinnamoroll->addFrame(cinnamoroll_sit_frame_2, cinnamoroll_sit_frame_2_mask);
      
      cinnamoroll->setVelocity(-5, 0);
      appendAvatar(cinnamoroll);


//      avatar = new Avatar (120, -51, CHIFFON_HANDS_UP_WIDTH, CHIFFON_HANDS_UP_HEIGHT, chiffon_hands_up, chiffon_hands_up_mask); //120 X 58
//      avatar->enableBreathing();
//      avatar->setBreathInterval(100);
//      avatar->setBreathPosition(30);
//      avatar->breathAmount = 2;
//      avatar->setVelocity(0.0, 9.8);
//      appendAvatar(avatar);

//      gameTitle = new Avatar (0, -IMAGE_GAME_TITLE_HEIGHT, IMAGE_GAME_TITLE_WIDTH, IMAGE_GAME_TITLE_HEIGHT, game_title, game_title_mask);
//      gameTitle->setVelocity(0.0, 5);
//      appendAvatar(gameTitle);

//      uint16_t color = rgb565(230, 157, 132);
//      _tft->fillScreen(color);

        drawBackground(game_bg1);
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
    boolean allAvatarsPositionFixed = false;
    //Avatar* gameTitle = NULL;
    Avatar* cinnamoroll = NULL;
//    void drawStartBtn() {
//      char* startBtnText = "START";
//      startBtn.initButton(_tft, 120, 200, 150, 30, TFT_BLACK, TFT_CYAN, // Fill
//                          TFT_BLACK, startBtnText, 3);
//      startBtn.drawButton(false, "");
//    }

    
};
#endif
