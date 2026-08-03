#ifndef _PENDINGMEAL_H_
#define _PENDINGMEAL_H_

// One-shot hand-off from the grocery store to the pet's home. When a food is
// bought, the grocery records it here and returns to the pet scene. The pet
// scene plays the eating animation (the food is attached to Totoro and steps
// through its new -> half -> eaten frames) and only *then* applies the
// hunger/happiness effect. The grocery owns the sprite-sheet layout, so it
// passes the three ready-to-use region indices; the pet scene stays agnostic.
// Not persisted (only meaningful for the single grocery -> home transition).
class PendingMeal {
  public:
    static void set(const char *name, int hunger, int happiness,
                    int regionNew, int regionHalf, int regionEaten);
    static bool pending();
    static const char *name();
    static int hunger();
    static int happiness();
    static int region(int frame);  // frame 0=new, 1=half, 2=eaten
    static void clear();
};

#endif
