#ifndef _GROCERY_FOODS_H_
#define _GROCERY_FOODS_H_

#include <Arduino.h>

// The store's stock. Shared rather than living inside Scene_Grocery so anything
// reasoning about whether a meal is affordable reads the same prices the store
// charges — CareActionRules needs the cheapest one to decide whether suggesting
// "Eat" is even actionable.
//
// Vegetables cost little and fill the pet up but cost happiness; sweets are the
// other way round. Keep that trade-off when editing prices.

#define GROCERY_FOOD_COUNT 12

struct GroceryFood {
  const char *name;
  int16_t cost;
  int8_t hunger;
  int8_t happiness;
};

inline const GroceryFood &groceryFood(int index) {
  static const GroceryFood FOODS[GROCERY_FOOD_COUNT] = {
    {"Broccoli",     5, 15,  -8},
    {"Green onion", 10, 10, -10},
    {"Salad",       10, 14,   1},
    {"Onigiri",     15, 22,   1},
    {"Yam",         10,  5,   1},
    {"Bun",         18, 25,   2},
    {"Hamburger",   22, 35,   2},
    {"Sushi",       25, 20,   3},
    {"Ramen",       41, 34,   3},
    {"Dorayaki",    21, 12,   4},
    {"Cotton candy",23,  3,   5},
    {"Soft serve",  26,  6,   5},
  };
  return FOODS[index];
}

// Least a meal can possibly cost. Derived from the table rather than written as a
// constant so re-pricing the shelves cannot leave the care rules behind.
inline int groceryCheapestCost() {
  int cheapest = groceryFood(0).cost;
  for (int i = 1; i < GROCERY_FOOD_COUNT; i++) {
    if (groceryFood(i).cost < cheapest) {
      cheapest = groceryFood(i).cost;
    }
  }
  return cheapest;
}

#endif
