#ifndef _PHYSICS_H_
#define _PHYSICS_H_
#include "Avatar.h"

#define COLLIDE_RESITUTION 1.0
class physics{
  public:

  //collision test with aabb
  static bool aabbTest(Avatar a, Avatar b){
    //No intersection if found separated along an axis
    if(a.x + a.width < b.x || a.x > b.x + b.width){
      return false;
    }
    if(a.y + a.height < b.y || a.y > b.y + b.height){
      return false;
    }
    return true;
  }
  static float distance(Vec2 a, Vec2 b){
    return sqrt( (a.x - b.x) *  (a.x - b.x) + (a.y - b.y)  *  (a.y - b.y) );
  }

  static bool circleTest(Circle a, Circle b){
    float r = a.radius + b.radius;
    r *= r;
    return r < ((a.x - b.x) *  (a.x - b.x) + (a.y - b.y) * (a.y - b.y) );
  }
  float dotProduct(Vec2 a, Vec2 b){
    return (a.x * b.x) + (a.y * b.y);
  }
  
  static void resolveCollision(Avatar *aPtr, Avatar *bPtr){
    //Physics from https://github.com/komrad36/ArduinoPhysics/blob/master/Sharp.ino
    Avatar a = *aPtr;
    Avatar b = *bPtr;

    float oldx = a.x - a.velocity.x ;
    float oldy = a.y - a.velocity.y ;
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    
    float d2 = dx * dx + dy * dy; //the square of the diagonal between a and b; because  sqrt(x^2 + y^2) = d

    //Calculate Relative velocity
    float dvx = (b.velocity.x  - a.velocity.x ) ;
    float dvy = (b.velocity.y  - a.velocity.y ) ;

    //dvx * dx + dvy * dy is Dot product of relative velocity and normal direction
    if( dvx * dx + dvy * dy < 0.0f){ //make sure we only calculate when objects are moving towards each other
      
      const float c = (dvx * dx + dvy * dy) / d2;
      const float cx = c * dx;
      const float cy = c * dy;

      //modify the velocity of a and b
      aPtr->velocity.x += cx;
      aPtr->velocity.y += cy;
      bPtr->velocity.x -= cx;
      bPtr->velocity.y -= cy;

      aPtr->x = oldx;
      aPtr->y = oldy;
    }
  }
};


#endif
