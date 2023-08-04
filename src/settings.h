#pragma once

#include <stdbool.h>

extern int OFF_SHIFT;
extern int OFF_DWELL0;
extern int OFF_DWELL1;
extern int OFF_DWELL2;
extern int NORMAL_SHIFT;
extern bool FLIP_X;
extern bool FLIP_Y;
extern bool SWAP_XY;
extern int PINCUSHION_FACTOR;

void update_setting(int setting, int value);