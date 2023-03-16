#include "drawing.h"
#include "build/spi_half_duplex.pio.h"
#include "max5716.h"

#define MAX_PTS 4096

typedef struct {
    int16_t x; // x destination
    int16_t y; // y destination
    bool blank; // if this is true, the program shouldn't bother looking at rgb
    uint8_t r;
    uint8_t g;
    uint8_t b;

    //TODO: add drawing variables
} point_t;

point_t frame[2][MAX_PTS]; // two framebuffers - one for writing to, and one for reading from

uint frame_count[2]; // number of points in each framebuffer

bool which_frame = 0; // which framebuffer is read from
#define POINT_READ which_frame
#define POINT_WRITE !which_frame

int x = 0, y = 0; // current position

void draw_frame() {
    for (int i = 0; i < frame_count[POINT_READ]; i++) {
        point_t p = frame[POINT_READ][i];
        
    }
}