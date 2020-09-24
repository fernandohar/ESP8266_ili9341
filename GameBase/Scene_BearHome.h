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
         *nextSceneIndex = 2;
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
    setBackground(whiteBearHome);

    wasTouching = false;
    Avatar* avatar = NULL;
    //Lowest layer ... top layer
    avatar = new Avatar(100, 0, CAT_WIDTH, CAT_HEIGHT, CatBitmap, CatMask); //100X67
    appendAvatar(avatar);
    avatar->setVelocity(0, 1);
    
    avatar = new Avatar(-30, 0, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    appendAvatar(avatar);
    
    avatar = new Avatar(-25, 51, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    appendAvatar(avatar);

    avatar = new Avatar(-20, 102, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    appendAvatar(avatar);

        avatar = new Avatar(-10, 153, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    appendAvatar(avatar);

    avatar = new Avatar(5, 204, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    appendAvatar(avatar);

    avatar = new Avatar(10, 255, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    avatar->setVelocity(0, -1);
    appendAvatar(avatar);

   //Top Layer 

   
    avatar = new Avatar(193, 224, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    appendAvatar(avatar);

   avatar = new Avatar(0,200, PORK_WIDTH, PORK_HEIGHT, PorkBitmap, PorkMask); //40X47
   appendAvatar(avatar);
   avatar->setVelocity(1, 0);
   
    avatar = new Avatar(194, 160, DRAGON_WIDTH, DRAGON_HEIGHT, DragonBitmap, DragonMask); //47 X 51
    appendAvatar(avatar);    
    
//    avatar = new Avatar(100, 271, SHRIMP_WIDTH, SHRIMP_HEIGHT, ShrimpTailBitmap, ShrimpTailmask); //44 X 60
//    appendAvatar(avatar);

    
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
