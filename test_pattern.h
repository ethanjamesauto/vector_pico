inline void moveto(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    draw_to_xyrgb(x - 2048, y - 2048, r, g, b);
}

void draw_test_pattern() {
  // Prepare buffer of test pattern data as a speed optimisation

  int offset, i, j;

  int intensity = 150;

  moveto(4095, 4095, 0, 0, 0);
  moveto(4095, 0, intensity, intensity, intensity);
  moveto(0, 0, intensity, intensity, intensity);
  moveto(0, 4095, intensity, intensity, intensity);
  moveto(4095, 4095, intensity, intensity, intensity);

  moveto(0, 0, 0, 0, 0);
  moveto(3071, 4095, intensity, intensity, intensity);
  moveto(4095, 2731, intensity, intensity, intensity);
  moveto(2048, 0, intensity, intensity, intensity);
  moveto(0, 2731, intensity, intensity, intensity);
  moveto(1024, 4095, intensity, intensity, intensity);
  moveto(4095, 0, intensity, intensity, intensity);

  moveto(0, 4095, 0, 0, 0);
  moveto(3071, 0, intensity, intensity, intensity);
  moveto(4095, 1365, intensity, intensity, intensity);
  moveto(2048, 4095, intensity, intensity, intensity);
  moveto(0, 1365, intensity, intensity, intensity);
  moveto(1024, 0, intensity, intensity, intensity);
  moveto(4095, 4095, intensity, intensity, intensity);
  moveto(4095, 4095, 0, 0, 0);

  // cross
  moveto(4095, 4095, 0, 0, 0);
  moveto(4095 - 512, 4095, 128, 128, 128);
  moveto(4095 - 512, 4095 - 512, 128, 128, 128);
  moveto(4095, 4095 - 512, 128, 128, 128);
  moveto(4095, 4095, 128, 128, 128);

  moveto(0, 4095, 0, 0, 0);
  moveto(512, 4095, 128, 128, 128);
  moveto(0, 4095 - 512, 128, 128, 128);
  moveto(512, 4095 - 512, 128, 128, 128);
  moveto(0, 4095, 128, 128, 128);

  // Square
  moveto(0, 0, 0, 0, 0);
  moveto(512, 0, 128, 128, 128);
  moveto(512, 512, 128, 128, 128);
  moveto(0, 512, 128, 128, 128);
  moveto(0, 0, 128, 128, 128);

  // triangle
  moveto(4095, 0, 0, 0, 0);
  moveto(4095 - 512, 0, 128, 128, 128);
  moveto(4095 - 0, 512, 128, 128, 128);
  moveto(4095, 0, 128, 128, 128);

  // RGB gradiant scale

  const uint16_t height = 3072;
  const int mult = 5;

  for (i = 0, j = 31; j <= 255; i += 8, j += 32)  //Start at 31 to end up at full intensity?
  {
    moveto(1100, height + i * mult, 0, 0, 0);
    moveto(1500, height + i * mult, j, 0, 0);  // Red
    moveto(1600, height + i * mult, 0, 0, 0);
    moveto(2000, height + i * mult, 0, j, 0);  // Green
    moveto(2100, height + i * mult, 0, 0, 0);
    moveto(2500, height + i * mult, 0, 0, j);  // Blue
    moveto(2600, height + i * mult, 0, 0, 0);
    moveto(3000, height + i * mult, j, j, j);  // all 3 colours combined
  }
}
