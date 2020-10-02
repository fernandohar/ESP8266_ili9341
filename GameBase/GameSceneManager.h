#ifndef _GAMESCENEMANAGER_H_
#define _GAMESCENEMANAGER_H_

#define UPDATES_PER_SECOND 20
#define UPDATES_DT 1000 / UPDATES_PER_SECOND
#define MAXSCENES 10
//#define SHOW_STATS_ON_TFT
#define SHOW_STATS
#include "GameScene.h"
#include <TFT_eSPI.h>
//#include <XPT2046.h>

class GameSceneManager {
  public:
//    GameSceneManager(TFT_eSPI *tft, XPT2046 *touch, uint8_t touchIrq) : _tft(tft), _touchIrq(touchIrq), _touch(touch) { };
    GameSceneManager(TFT_eSPI *tft, uint8_t touchIrq) : _tft(tft), _touchIrq(touchIrq) { };
    int appendScene(GameScene *gameScene) {
      _scenes[_totalScenes++] = gameScene;
      if ( _currentSceneIndex == -1) {
        changeScene(0);
      }
    }

    unsigned long nextUpdate = 0;
    int loop = 0;
    void update() {

      //Uses deWiTTERS game loop's Constant Game Speed with Maximum FPS
      loop = 0;
      while ( millis() > nextUpdate && loop < 10) { //5 determines the lowest frames per second
        boolean isTouching = (digitalRead(_touchIrq) == LOW);
        if (isTouching) {
          _tft->getTouch(&touchX, &touchY);
         //_touch->getPosition(touchX, touchY);
//         Serial.print("touched");
//         Serial.print(touchX);
//         Serial.print(",");
//         Serial.println(touchY);
        }

        _currentScenePtr->update(isTouching, touchX, touchY, &needChangeScene, &_nextSceneIndex); //Suppose we use Constant Update speed of 50 times per seconds (50 / 1000 = 20) = diration

        //Scene Control
        if (needChangeScene && _nextSceneIndex != -1) {
          changeScene(_nextSceneIndex);
          needChangeScene = false;
          _nextSceneIndex = -1;
          return;
        }
        nextUpdate += UPDATES_DT;
        loop++;
#ifdef SHOW_STATS
        updateCount++;
#endif
      }
      _currentScenePtr->render();
#ifdef SHOW_STATS
      frameCount++;


      if ((millis() - frameStart) >= 1000) {
#ifdef SHOW_STATS_ON_TFT        
        _tft->drawString("FPS", 0, 0, 2);
        _tft->drawNumber(frameCount, 50, 0, 2);
#endif
        Serial.print(" FPS: ");
        Serial.print(frameCount );
        Serial.print(" UPS: ");
        Serial.println(updateCount);
        frameCount = 0;
        updateCount = 0;
        frameStart = millis();
      }
#endif
    }

    void changeScene(unsigned sceneIndex) {
      if (_currentSceneIndex >= 0) {
        _scenes[_currentSceneIndex]->destroyScene();
      }
      _currentSceneIndex = sceneIndex;
      _currentScenePtr = _scenes[_currentSceneIndex];
      _currentScenePtr->initScene();
#ifdef SHOW_STATS      
      frameStart = millis();
#endif      
    }
  private:
    unsigned long previousUpdate = 0 ;
    unsigned long previousRender = 0;

    GameScene *_scenes[MAXSCENES];
    GameScene *_currentScenePtr; //use a pointer for performance
    int _currentSceneIndex = -1;
    int _totalScenes = 0;
    uint8_t _touchIrq;
    TFT_eSPI *_tft;
    //XPT2046 *_touch;
    uint16_t touchX = 0;
    uint16_t touchY = 0;
    boolean needChangeScene = false;
    int _nextSceneIndex = -1;

#ifdef SHOW_STATS
    unsigned long frameCount = 0;
    unsigned long frameStart = 0;
    unsigned long updateCount = 0;
#endif
};

#endif
