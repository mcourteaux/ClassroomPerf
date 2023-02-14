#include <immintrin.h>
inline float hsum_ps_sse3(__m128 v) {
    __m128 shuf = _mm_movehdup_ps(v);        // broadcast elements 3,1 to 2,0
    __m128 sums = _mm_add_ps(v, shuf);
    shuf        = _mm_movehl_ps(shuf, sums); // high half -> low half
    sums        = _mm_add_ss(sums, shuf);
    return        _mm_cvtss_f32(sums);
}

/* SSE3 4-wide float */
float student_atan(float x, float max_error) {
    float xsq = x * x;
    float fl_xpow4 = xsq * xsq;
    float fl_xpow8 = fl_xpow4 * fl_xpow4;
    bool flip = x < 0.0;
    if (flip) {
        x = -x;
    }
    __m128 xpow = _mm_set_ps(x, -x * xsq, x * fl_xpow4, -x * fl_xpow4 * xsq);
    __m128 denom = _mm_set_ps(1.0, 3.0, 5.0, 7.0);
    __m128 l = _mm_setzero_ps();
    __m128 r = _mm_setzero_ps();
    __m128 xpow8 = _mm_set1_ps(fl_xpow8);
    __m128 const8 = _mm_set1_ps(8.0);

    int i = 0;
    do {
        l = _mm_div_ps(xpow, denom);
        xpow = _mm_mul_ps(xpow, xpow8);
        r = _mm_add_ps(r, l);
        denom = _mm_add_ps(denom, const8);
    } while (-_mm_cvtss_f32(l) > max_error);

    float rr = hsum_ps_sse3(r);
    return flip ? -rr : rr;
}

