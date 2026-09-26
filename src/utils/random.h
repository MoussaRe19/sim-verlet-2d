#ifndef RAND_H
#define RAND_H

#include "common.h"

#define MAX_COLORS 20

typedef struct {
	int len;
	int arr[MAX_COLORS][3];
} Palette;

Color RandomColor(const Palette* palette);
float RandomFloat(float min, float max);

#endif
