#ifndef _SCENE_BEARHOME_H_
#define _SCENE_BEARHOME_H_
#include "GameScene.h"
#include "Avatar.h"
#include "Bitmap_Background.h"
#include "cat.h" //Temp
#include "pork.h"
#include "dragon.h"
#include "shrimp.h"
class Scene_BearHome : public GameScene{
  public: 
  Scene_BearHome(TFT_eSPI *tft){
    GameScene::_tft = tft;
   };
   
  void update(boolean isTouching, uint16_t touchX, uint16_t touchY, boolean* needChangeScene, int* nextSceneIndex) {
    if (isTouching){
      if(!wasTouching){
         //tap down
      }
      wasTouching = true;
    }else{
      if(wasTouching){ 
         //tap up
         *needChangeScene = true;
         *nextSceneIndex = 1;
         return;
      }
      wasTouching = false;
    }
     for (int i = 0; i < numAvatar; ++i) {
        Avatar* avatar = avatars[i];
        if (avatar != NULL) {   
          avatar->updatePos();
        }
      }
    
  }
  void render(){
    //renderCharacter(0, 0, 0, 0, CAT_WIDTH, CAT_HEIGHT, CatBitmap, CatMask, SCREENWIDTH); //Not recommanded way to render

      renderScene();
      
//      for (int i = 0; i < numAvatar; ++i) {
//        Avatar* avatar = avatars[i];
//        if (avatar != NULL) {
//          renderCharacter(avatar->previousRenderedX, avatar->previousRenderedY, avatar->x, avatar->y, avatar->width, avatar->height, avatar->bitmap, avatar->mask, SCREENWIDTH); //temp
//          avatar->savePreviousRenderPos();
//        }
//      }
    
  }

  void initScene(){
    wasTouching = false;
    Avatar* avatar = NULL;
    
    //avatar = new Avatar(-200, -200, CAT_WIDTH, CAT_HEIGHT, CatBitmap, CatMask);
    //appendAvatar(avatar);
//
    //avatar = new Avatar(-200,100, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask);
    //avatar->setVelocity(1, 0);
    //appendAvatar(avatar);
   
    avatar = new Avatar(60, 60, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask);
    appendAvatar(avatar);
//    
    avatar = new Avatar(232, 100, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask);
    avatar->setVelocity(-1, 0);
    appendAvatar(avatar);
    drawBackground(whiteBearHome);
  }

  void destroyScene(){
    wasTouching = false;
    GameScene::destroyScene();
  }
  private :
    boolean wasTouching = false;    
};
#endif
