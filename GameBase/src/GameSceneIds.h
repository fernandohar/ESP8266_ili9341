#ifndef _GAMESCENEIDS_H_
#define _GAMESCENEIDS_H_

// Central list of scene indices (the order scenes are appended in main.cpp).
// Kept in one place so scenes can navigate to each other without pulling in
// each other's full headers. The pet's forest home is scene 0 and the game's
// central hub; the old location-menu hub scene has been removed.
#define SCENE_PET_TOTORO 0
#define SCENE_ACORN_CATCH 1
#define SCENE_SETTINGS 2
#define SCENE_TIC_TAC_TOE 3
#define SCENE_WHACK_A_MOLE 4
#define SCENE_STATUS 5
#define SCENE_GROCERY 6
#define SCENE_CAT_BUS_CROSS 7
#define SCENE_COIN_REWARD 8

#endif
