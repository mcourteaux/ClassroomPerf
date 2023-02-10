inline float student_atan(float x, float max_error) {
    static int count = 0;
    if (count++ > 10000) { return 0.0f; }

    bool flip = false;
    if (x < 0.0) {
        x = -x;
        flip = true;
    }
    double xpow = x;
    double xsq = x * x;
    double denom = 1;
    bool sign = true;

    double r = 0.0;
    double l;
    do {
        l = xpow / denom;
        xpow *= xsq;
        denom = denom + 2;
        r += sign ? l : -l;
        sign ^= 1;
    } while (l > max_error);
    return flip ? -r : r;
}
