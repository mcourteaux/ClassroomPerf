// clang-format off
#include <immintrin.h>
#if _WIN32
#include <intrin.h>
#else
# include <x86intrin.h>
#endif

#include <cmath>

#define MAX_ERROR 1e-6f

#include "submitted_code.hpp"

#include <chrono>
#include <random>
#include <limits>
#include <iostream>
#include <iomanip>
// clang-format on

static float correct_haversine(float radius, float lat1, float lon1, float lat2, float lon2) {
  float s1 = std::sin((lat2 - lat1) * 0.5f);
  float s2 = std::sin((lon2 - lon1) * 0.5f);
  return 2.0f * radius * std::asin(std::sqrt(s1 * s1 + std::cos(lat1) * std::cos(lat2) * s2 * s2));
}

static void correctness_test(float *values, int num_inputs, float max_error) {
  for (int i = 0; i < num_inputs; ++i) {
    float rad = values[(i + 1) % num_inputs];
    float lat1 = values[(i + 2) % num_inputs];
    float lon1 = values[(i + 0) % num_inputs];
    float lat2 = values[(i + 4) % num_inputs];
    float lon2 = values[(i + 5) % num_inputs];
    float actual = student_haversine(rad, lat1, lon1, lat2, lon2);
    float expected = correct_haversine(rad, lat1, lon1, lat2, lon2);
    if (std::abs(actual - expected) > max_error) {
      std::cerr << "Incorrect haversine implementation." << std::endl;
      std::cerr << std::setprecision(20);
      std::cerr << "Error: " << (actual - expected) << std::endl;
      std::cerr << "Allowed error: " << max_error << std::endl;
      std::exit(1);
    }
  }
}

int main(int argc, char **argv) {
  using namespace std::chrono;

  std::uniform_real_distribution<float> udist(-M_PI*0.5f, M_PI*0.5f);
  std::mt19937 mt;
  mt.seed(42);

  constexpr size_t num_inputs = 2048;
  float values[num_inputs];
  for (int i = 0; i < num_inputs; ++i) {
    values[i] = udist(mt);
  }

  // Correctness
  constexpr float max_error = MAX_ERROR;
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

    for (int i = 0; i < test_count; ++i) {
      float rad = values[(i + 0) % num_inputs];
      float lat1 = values[(i + 4) % num_inputs];
      float lon1 = values[(i + 3) % num_inputs];
      float lat2 = values[(i + 2) % num_inputs];
      float lon2 = values[(i + 1) % num_inputs];
      r += student_haversine(rad, lat1, lon1, lat2, lon2);
    }

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
