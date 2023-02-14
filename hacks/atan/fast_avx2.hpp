static float inv[] = {
  1.0f / 01.0f,
  1.0f / 03.0f,
  1.0f / 05.0f,
  1.0f / 07.0f,
  1.0f / 09.0f,
  1.0f / 11.0f,
  1.0f / 13.0f,
  1.0f / 15.0f,
};

/* AVX2 8-wide float with LUT */
float student_atan(float x, float max_error) {
    float xsq = x * x;
    float fl_xpow4 = xsq * xsq;
    float fl_xpow8 = fl_xpow4 * fl_xpow4;
    float fl_xpow16 = fl_xpow8 * fl_xpow8;
    __m128 xpow_lo = _mm_setr_ps( x, -x * xsq, x * fl_xpow4, -x * fl_xpow4 * xsq);
    __m128 xpow_hi = _mm_mul_ps(xpow_lo, _mm_set1_ps(fl_xpow8));
    __m256 xpow = _mm256_set_m128(xpow_hi, xpow_lo);
    __m256 l = _mm256_setzero_ps();
    __m256 r = _mm256_setzero_ps();
    __m256 xpow16 = _mm256_set1_ps(fl_xpow16);

    int i = 0;
    {
        __m256 factor = _mm256_loadu_ps(&inv[i]);
        l = _mm256_mul_ps(xpow, factor);
        xpow = _mm256_mul_ps(xpow, xpow16);
        r = _mm256_add_ps(r, l);
        i += 8;
    }

    /* ( x3+x7, x2+x6, x1+x5, x0+x4 ) */
    const __m128 x128 = _mm_add_ps(_mm256_extractf128_ps(r, 1), _mm256_castps256_ps128(r));
    /* ( -, -, x1+x3+x5+x7, x0+x2+x4+x6 ) */
    const __m128 x64 = _mm_add_ps(x128, _mm_movehl_ps(x128, x128));
    /* ( -, -, -, x0+x1+x2+x3+x4+x5+x6+x7 ) */
    const __m128 x32 = _mm_add_ss(x64, _mm_shuffle_ps(x64, x64, 0x55));
    /* Conversion to float is a no-op on x86-64 */
    return _mm_cvtss_f32(x32);
}
