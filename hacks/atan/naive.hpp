/* Works GREAT with -O3 -mavx -ffast-math */
float student_atan(float x, float max_error) {
  float r = 0.0f;
  float xpow = x;
  for (int i = 0; i < 8; ++i) {
    if (i & 1) {
      r -= xpow / (2 * i + 1);
    } else {
      r += xpow / (2 * i + 1);
    }
    xpow *= x * x;
  }
  return r;
}
