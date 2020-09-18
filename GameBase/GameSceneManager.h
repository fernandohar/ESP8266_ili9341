#ifndef _GAMESCENEMANAGER_H_
#define _GAMESCENEMANAGER_H_

#define MAXSCENES 10
#include "GameScene.h"
#include <TFT_eSPI.h>

class GameSceneManager {
  public:
    GameSceneManager(TFT_eSPI *tft, uint8_t touchIrq) : _tft(tft), _touchIrq(touchIrq) { };
    int appendScene(GameScene *gameScene) {
      _scenes[_totalScenes++] = gameScene;
      if ( _currentSceneIndex == -1) {
        changeScene(0);
      }
    }
    unsigned long frameCount = 0;
    unsigned long frameStart = 0;
    unsigned long updateCount = 0;

    unsigned long nextUpdate = 0;
    int loop = 0;
    void update() {

      //Uses deWiTTERS game loop's Constant Game Speed with Maximum FPS
      loop = 0;
      while ( millis() > nextUpdate && loop < 10) { //5 determines the lowest frames per second
        boolean isTouching = (digitalRead(_touchIrq) == LOW);
        if (isTouching) {
          _tft->getTouch(&touchX, &touchY);
        }

        _currentScenePtr->update(isTouching, touchX, touchY, &needChangeScene, &_nextSceneIndex); //Suppose we use Constant Update speed of 50 times per seconds (50 / 1000 = 20) = diration

        //Scene Control
        if (needChangeScene && _nextSceneIndex != -1) {
          changeScene(_nextSceneIndex);
          needChangeScene = false;
          _nextSceneIndex = -1;
          return;
        }

        nextUpdate += 20; //want Update speed = 50 times per second
        loop++;

        updateCount++;
      }
      _currentScenePtr->render();
      frameCount++;


      if ((millis() - frameStart) >= 1000) {
        Serial.print(" FPS: ");
        Serial.print(frameCount );
        Serial.print(" UPS: ");
        Serial.println(updateCount);
        frameCount = 0;
        updateCount = 0;
        frameStart = millis();
      }
    }

    void changeScene(unsigned sceneIndex) {
      if (_currentSceneIndex >= 0) {
        _scenes[_currentSceneIndex]->destroyScene();
      }
      _currentSceneIndex = sceneIndex;
      _currentScenePtr = _scenes[_currentSceneIndex];
      _currentScenePtr->initScene();
      frameStart = millis();
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
    uint16_t touchX = 0;
    uint16_t touchY = 0;
    boolean needChangeScene = false;
    int _nextSceneIndex = -1;
};

#endif
