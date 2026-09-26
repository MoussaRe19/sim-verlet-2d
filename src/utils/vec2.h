#ifndef VEC2_H
#define VEC2_H

#include "common.h"

#define TWO_PI PI * 2.0f

Vector2 Vector2Random2D(void);

Vector2 Vector2SetMag(Vector2 v, float length);

Vector2 Vector2DivideScalar(Vector2 v, float scalar);

#endif
