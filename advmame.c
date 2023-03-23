/*
   VSTCM

   Vector Signal Transceiver Colour Mod using MCP4922 DACs on the Teensy 4.1

   Code to interface with AdvanceMAME

*/

// #include <SD.h>
#include "advmame.h"
#include "drawing.h"
#include "hardware/gpio.h"
#include "string.h"
#include <stdio.h>
// #include "settings.h"

char const* json_opts[] = { "\"productName\"", "\"version\"", "\"flipx\"", "\"flipy\"", "\"swapxy\"", "\"bwDisplay\"", "\"vertical\"", "\"crtSpeed\"", "\"crtJumpSpeed\"", "\"remote\"", "\"crtType\"", "\"defaultGame\"" };
char const* json_vals[] = { "\"VSTCM\"", "\"V3.0\"", "false", "false", "false", "false", "false", "15", "9", "true", "\"CUSTOM\"", "\"none\"" };
static char json_str[MAX_JSON_STR_LEN];

/*
extern params_t v_setting[NB_SETTINGS];
extern unsigned long dwell_time;
extern float line_draw_speed;
extern bool spot_triggered;
*/

// Read data from Raspberry Pi or other external computer
// using AdvanceMAME protocol published here
// https://github.com/amadvance/advancemame/blob/master/advance/osd/dvg.c

// Build up the information string and return the length including two nulls at the end.
// Currently does not check for overflow so make sure the string is long enough!!
uint32_t build_json_info_str(char* str)
{
    int i;
    uint32_t len;
    str[0] = 0;
    strcat(str, "{\n");

    for (i = 0; i < NUM_JSON_OPTS; i++) {
        strcat(str, json_opts[i]);
        strcat(str, ":");
        strcat(str, json_vals[i]);
        if (i < (NUM_JSON_OPTS - 1))
            strcat(str, ",");
        strcat(str, "\n");
    }

    strcat(str, "}\n");
    len = strlen(str);
    str[len + 1] = 0; // Double null terminate
    // 10 has to be added to the length. TODO: investigate why (probably will never get around to this tbh)
    return (len + 2 + 10); // Length includes both nulls
}

int read_data(int init)
{
    static bool start = true;
    static uint32_t cmd = 0;
    static bool resync = true;
    static uint8_t gl_red, gl_green, gl_blue;
    static int frame_offset = 0;

    static uint32_t len;

    if (init) {
        len = build_json_info_str(json_str);
        frame_offset = 0;
        return 0;
    }
    //gpio_put(25, 0);
    int c = getchar(); // read one byte at a time
    //gpio_put(25, 1);
    if (resync && c == 0xc3) {
        resync = false;
        return 0;
    }

    // if (c == -1) // if serial port returns nothing then exit
    //    return -1;

    cmd = cmd << 8 | c;
    frame_offset++;

    if (frame_offset < 4)
        return 0;

    frame_offset = 0;

    uint8_t header = (cmd >> 29) & 0b00000111;

    if (header == FLAG_RESYNC && cmd != 0xc0c1c2c3) {
        resync = true;
    } else if (header == FLAG_XY) {
        if (start) {
            start = false;
            begin_frame();
        }
        // uint32_t y = (cmd >> 0) & 0x3fff;
        // uint32_t x = (cmd >> 14) & 0x3fff;

        int y = ((cmd >> 0) & 0xfff) - 2048;
        int x = ((cmd >> 14) & 0xfff) - 2048;

        //   Serial.println(x);
        //  Serial.println(y);

        // As an optimisation there is a blank flag in the XY coord which
        // allows blanking of the beam without updating the RGB color DACs.

        if ((cmd >> 28) & 0x01)
            draw_moveto(x, y);
        else {
            brightness(gl_red, gl_green, gl_blue); // Set RGB intensity levels
            if (gl_red == 0 && gl_green == 0 && gl_blue == 0)
                draw_moveto(x, y);
            else
                _draw_lineto(x, y);
        }
    } else if (header == FLAG_RGB) {
        // encode brightness for R, G and B
        gl_red = (cmd >> 16) & 0xFF;
        gl_green = (cmd >> 8) & 0xFF;
        gl_blue = (cmd >> 0) & 0xFF;
    }

    else if (header == FLAG_FRAME) {
        //  uint32_t frame_complexity = cmd & 0b0001111111111111111111111111111;
        // TODO: Use frame_complexity to adjust screen writing algorithms
    } else if (header == FLAG_COMPLETE) {
        // Check FLAG_COMPLETE_MONOCHROME like "if(cmd&FLAG_COMPLETE_MONOCHROME) ... "
        // Not sure what to do differently if monochrome frame complete??
        // Add FPS on games as a guide for optimisation
        /*
        if (v_setting[9].pval == true) {
            if (spot_triggered)
                draw_string("*DT:", 3000, 150, 6, v_setting[13].pval);
            else
                draw_string("DT:", 3000, 150, 6, v_setting[13].pval);
            // draw_string("DS:", 3000, 150, 6, v_setting[13].pval);
            // draw_string(itoa(line_draw_speed*NORMAL_SHIFT_SCALING, buf1, 10), 3400, 150, 6, v_setting[13].pval);
            draw_string(itoa(dwell_time, buf1, 10), 3400, 150, 6, v_setting[13].pval);
        }
        */
        end_frame();
        start = true;
        return 1;
    } else if (header == FLAG_EXIT) {
        // Serial.flush(); // not sure if this useful, may help in case of manual quit on MAME
        return -1;
    } else if (header == FLAG_CMD && ((cmd & 0xFF) == FLAG_CMD_GET_DVG_INFO)) { // Some info about the host comes in as follows from the advmame code:
        // sscanf(ADV_VERSION, "%u.%u", &major, &minor);
        // version = ((major & 0xf) << 12) | ((minor & 0xf) << 8) | (DVG_RELEASE << 4) | (DVG_BUILD);
        //  cmd |= version << 8;
        //  Currently DVG_RELEASE and DVG_BUILD are both zero
        // The response needs to be the following:
        // 1. Echo back the command in reverse order (least significant first)  like: 01 00 00 A0
        // 2. Send the length of the JSON string including two nulls at the end and all whitespace etc
        // 3. Send the JSON string followed by two nulls
        /*for (int i = 0; i < 1000; i++) {
            putchar('A');
        }
        return 0;*/
        putchar(cmd & 0xFF);
        putchar((cmd >> 8) & 0xFF);
        putchar((cmd >> 16) & 0xFF);
        putchar((cmd >> 24) & 0xFF);
        putchar(len & 0xFF);
        putchar((len >> 8) & 0xFF);
        putchar(0); // Only send the first 16 bits since we better not have a strong more than 64K long!
        putchar(0);
        puts(json_str);

        return 0;
    } else {
        // Serial.println("Unknown");  //This might be messing things up?
    }

    return 0;
}