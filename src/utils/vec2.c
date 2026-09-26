#include "vec2.h"
#include "random.h"

Vector2 Vector2Random2D(void) {
	float angle = RandomFloat(0.0f, 2.0f * PI);
	return (Vector2){cosf(angle), sinf(angle)};
}

Vector2 Vector2SetMag(Vector2 v, float length) {
	return Vector2Scale(Vector2Normalize(v), length);
}

Vector2 Vector2DivideScalar(Vector2 v, float scalar) {
	if (scalar == 0.0f) return (Vector2){0.0f, 0.0f};
	return Vector2Scale(v, 1.0f / scalar);
}
