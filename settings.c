#include "settings.h"
#include <stdbool.h>

int OFF_SHIFT = 50; // Smaller numbers == slower transits (the higher the number, the less flicker and faster draw but more wavy lines)
int NORMAL_SHIFT = 3; // The higher the number, the less flicker and faster draw but more wavy lines
int OFF_DWELL0 = 5; // Time to wait after changing the beam intensity (settling time for intensity DACs and monitor)
int OFF_DWELL1 = 0; // Time to sit before starting a transit
int OFF_DWELL2 = 0; // Time to sit after finishing a transit
bool FLIP_X = true; // Sometimes the X and Y need to be flipped and/or swapped
bool FLIP_Y = true;
bool SWAP_XY = false;
int PINCUSHION_FACTOR = 24; 

void update_setting(int setting, int value) {
    switch (setting) {
        case 0:
            OFF_SHIFT = value;
            break;
        case 1:
            NORMAL_SHIFT = value;
            break;
        case 2:
            OFF_DWELL0 = value;
            break;
        case 3:
            OFF_DWELL1 = value;
            break;
        case 4:
            OFF_DWELL2 = value;
            break;
        case 5:
            FLIP_X = value;
            break;
        case 6:
            FLIP_Y = value;
            break;
        case 7:
            SWAP_XY = value;
            break;
        case 8:
            PINCUSHION_FACTOR = value;
            break;
    }
}