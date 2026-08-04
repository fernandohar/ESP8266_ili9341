#include "PendingMeal.h"

static bool s_pending = false;
static const char *s_name = "";
static int s_hunger = 0;
static int s_happiness = 0;
static int s_regions[3] = {0, 0, 0};

void PendingMeal::set(const char *name, int hunger, int happiness,
                      int regionNew, int regionHalf, int regionEaten) {
  s_pending = true;
  s_name = (name != nullptr) ? name : "";
  s_hunger = hunger;
  s_happiness = happiness;
  s_regions[0] = regionNew;
  s_regions[1] = regionHalf;
  s_regions[2] = regionEaten;
}

bool PendingMeal::pending() { return s_pending; }
const char *PendingMeal::name() { return s_name; }
int PendingMeal::hunger() { return s_hunger; }
int PendingMeal::happiness() { return s_happiness; }

int PendingMeal::region(int frame) {
  if (frame < 0) frame = 0;
  if (frame > 2) frame = 2;
  return s_regions[frame];
}

void PendingMeal::clear() { s_pending = false; }
