// clang-format off
#include <immintrin.h>
#if _WIN32
#include <intrin.h>
#else
# include <x86intrin.h>
#endif

#include "submitted_code.hpp"

#include <chrono>
#include <random>
#include <limits>
#include <iostream>
#include <iomanip>
// clang-format on

static void correctness_test(float *values, int num_inputs, float max_error) {
  for (int i = 0; i < num_inputs; ++i) {
    float x = values[i % num_inputs];
    float actual = student_atan(x, max_error);
    float expected = std::atan(x);
    if (std::abs(actual - expected) > max_error) {
      std::cerr << "Incorrect atan implementation." << std::endl;
      std::cerr << std::setprecision(20);
      std::cerr << "student_atan(" << x << ") = " << actual << std::endl;
      std::cerr << "std::atan(" << x << ") = " << expected << std::endl;
      std::cerr << "Error: " << (actual - expected) << std::endl;
      std::cerr << "Allowed error: " << max_error << std::endl;
      std::exit(1);
    }
  }
}

int main(int argc, char **argv) {
  using namespace std::chrono;

  std::uniform_real_distribution<float> udist(-0.5f, 0.5f);
  std::mt19937 mt;
  mt.seed(42);

  constexpr size_t num_inputs = 2048;
  float values[num_inputs];
  for (int i = 0; i < num_inputs; ++i) {
    values[i] = udist(mt);
  }

  // Correctness
  constexpr float max_error = 1e-7;
  correctness_test(values, num_inputs, max_error);

  float r = 0.0f;

  // Benchmark
  double best_time = std::numeric_limits<double>::max();
  int64_t best_cycle_count = std::numeric_limits<int64_t>::max();
  int best_run = -1;
  constexpr int test_count = 1024 * 1024;
  for (int run = 0; run < 40 || best_run > run - 10; ++run) {
    high_resolution_clock::time_point start = high_resolution_clock::now();
    int64_t start_cycle = __rdtsc();
#if 1
    for (int i = 0; i < test_count; i += 8) {
      r += student_atan(values[(i + 0) % num_inputs], max_error);
      r += student_atan(values[(i + 1) % num_inputs], max_error);
      r += student_atan(values[(i + 2) % num_inputs], max_error);
      r += student_atan(values[(i + 3) % num_inputs], max_error);
      r += student_atan(values[(i + 4) % num_inputs], max_error);
      r += student_atan(values[(i + 5) % num_inputs], max_error);
      r += student_atan(values[(i + 6) % num_inputs], max_error);
      r += student_atan(values[(i + 7) % num_inputs], max_error);
    }
#else
    for (int i = 0; i < test_count; ++i) {
      r += student_atan(values[i % num_inputs], max_error);
    }
#endif
    int64_t stop_cycle = __rdtsc();
    int64_t cycles_taken = stop_cycle - start_cycle;
    high_resolution_clock::time_point stop = high_resolution_clock::now();
    double elapsed_seconds =
        std::chrono::duration_cast<std::chrono::duration<double> >(stop - start)
            .count();
    if (elapsed_seconds < best_time) {
      best_time = elapsed_seconds;
      best_run = run;
      best_cycle_count = cycles_taken;
      std::cerr << "Time: " << elapsed_seconds << "  (new best!)" << std::endl;
    } else {
      std::cerr << "Time: " << elapsed_seconds << std::endl;
    }
  }
  std::cerr << r << std::endl;
  std::cerr << std::setprecision(9);
  std::cerr << "Best time: " << best_time << std::endl;
  std::cerr << "total cycles: " << best_cycle_count << std::endl;
  std::cerr << "cycles / student_atan: " << (best_cycle_count / double(test_count)) << std::endl;

  // Correctness (no cheaters!)
  correctness_test(values, num_inputs, max_error);

  std::printf("%.9f\n", best_time);
  std::printf("%.9f\n", (best_cycle_count / double(test_count)));

  return 0;
}
