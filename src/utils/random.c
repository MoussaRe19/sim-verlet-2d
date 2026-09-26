#include "random.h"

Color RandomColor(const Palette* palette) {
	int index = GetRandomValue(0, palette->len - 1);
	return (Color){palette->arr[index][0], palette->arr[index][1],
				   palette->arr[index][2], 255};
}

float RandomFloat(float min, float max) {
	return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}
