inline int clamp(int i, int min, int max) {
  const int t = i < min ? min : i;
  return t > max ? max : t;
}