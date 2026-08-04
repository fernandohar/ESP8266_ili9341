#ifndef _GAMESCENEMANAGER_H_
#define _GAMESCENEMANAGER_H_

#define UPDATES_PER_SECOND 20
#define UPDATES_DT 1000 / UPDATES_PER_SECOND
#define MAX_FRAMESKIP 5
#define MAXSCENES 10

//#define DEBUG_SCENEMANAGER
#include <Arduino.h>
#include "GameScene.h"
#include "Input.h"
#include "SoundPlayer.h"
#include "PetSave.h"
#include <TFT_eSPI.h>

class GameSceneManager {
  public:
    GameSceneManager(TFT_eSPI *tft, uint8_t touchIrq, bool (*touchReader)() = NULL) : _touchIrq(touchIrq), _tft(tft), _touchReader(touchReader) { };
    
    void appendScene(GameScene *gameScene) {
      _scenes[_totalScenes++] = gameScene;
    }

    // Explicitly select the first scene to show. Call once after all scenes are
    // registered (the boot scene, e.g. the pet's home).
    void startScene(int sceneIndex) {
      changeScene(sceneIndex);
    }

    
    int loop = 0;
    void update() {

      //Uses deWiTTERS game loop's Constant Game Speed with Maximum FPS
      loop = 0;

      //when we have slow hardware and the update + logic is taking too much time, we will skip rendering and update again. 
      //After skipping MAX_FRAMESKIP number of frames, we force rendering, and the game will slow down
      while ( millis() > nextUpdate && loop < MAX_FRAMESKIP) { 
        //Scene Control
        if (needChangeScene && _nextSceneIndex != -1) {
          changeScene(_nextSceneIndex);
          needChangeScene = false;
          _nextSceneIndex = -1;
          return;
        }

        // Poll inputs once per fixed tick (not per loop()). Button edges like
        // homePressed are only true for a single Input::update(); polling faster
        // than the tick would clear the edge before a scene ever sees it.
        Input::update();

        boolean isTouching = (_touchReader != NULL) ? _touchReader() : (digitalRead(_touchIrq) == LOW);

#if !defined(ARDUINO_ARCH_ESP32)
        SoundPlayer::update();
#endif
        _currentScenePtr->update(isTouching, &needChangeScene, &_nextSceneIndex);
        if (needChangeScene && _nextSceneIndex != -1) {
          changeScene(_nextSceneIndex);
          needChangeScene = false;
          _nextSceneIndex = -1;
          return;
        }
        nextUpdate += UPDATES_DT;
        loop++;

//#ifdef DEBUG_SCENEMANAGER        
//        updateCount++;
//#endif
      }

      current = millis();
      bool urgentRender = _currentScenePtr->consumeRenderRequest();
      if (urgentRender || current > nextRender) {
        _currentScenePtr->render();
        nextRender = current + (urgentRender ? 33 : 50);
      }

      
//#ifdef DEBUG_SCENEMANAGER
//      frameCount++; //Count number of renders (frames), for calculation of FPS
//
//      if ((millis() - frameStart) >= 1000) {
//        String stats = "FPS: " +  String(frameCount) + " UPDATE: " + String(updateCount);
//        _tft->setTextSize(1);
//        _tft->drawString(stats, 0, 0, 2);
//        frameCount = 0;
//        updateCount = 0;
//        frameStart = millis();
//      }
//#endif
    }

    void changeScene(unsigned sceneIndex) {
      Serial.println("changeScene");
      if (_currentSceneIndex >= 0) {
        // Persist coins + pet stats when leaving a scene so progress survives
        // reboots (scene changes are user-paced, so this won't thrash NVS).
        PetSave::save();
        _scenes[_currentSceneIndex]->destroyScene();
      }
      _currentSceneIndex = sceneIndex;
      _currentScenePtr = _scenes[_currentSceneIndex];
      _currentScenePtr->initScene();
      Input::syncEdges();

//#ifdef DEBUG_SCENEMANAGER       
//      frameStart = millis();
//#endif
    }
  private:
    unsigned long current  = 0;
    unsigned long nextUpdate = 0;
    unsigned long nextRender = 0;
//    unsigned long previousUpdate = 0 ;
//    unsigned long previousRender = 0;

    GameScene *_scenes[MAXSCENES];
    GameScene *_currentScenePtr; //use a pointer for performance
    int _currentSceneIndex = -1;
    int _totalScenes = 0;
    uint8_t _touchIrq;
    TFT_eSPI *_tft;    
    bool (*_touchReader)() = NULL;
    uint16_t touchX = 0;
    uint16_t touchY = 0;
    boolean needChangeScene = false;
    int _nextSceneIndex = -1;

//    //Stats
//#ifdef DEBUG_SCENEMANAGER            
//    unsigned long frameCount = 0;
//    unsigned long frameStart = 0;
//    unsigned long updateCount = 0;
//#endif

};

#endif
