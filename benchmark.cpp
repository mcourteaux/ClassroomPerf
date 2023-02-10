
// clang-format off
#include "submitted_code.hpp"

#include <chrono>
#include <random>
#include <limits>
#include <iostream>
// clang-format on

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
  constexpr float max_error = 1e-6;
  for (int i = 0; i < num_inputs; ++i) {
    float actual = student_atan(values[i % num_inputs], max_error);
    float expected = std::atan(values[i % num_inputs]);
    if (std::abs(actual - expected) > max_error) {
      std::cerr << "Incorrect atan implementation." << std::endl;
      return 1;
    }
  }

  // Warmup
  float r = 0;
  for (int i = 0; i < 10000; ++i) {
    r += student_atan(values[i % num_inputs], max_error);
  }

  // Benchmark
  double best_time = std::numeric_limits<double>::max();
  for (int run = 0; run < 20; ++run) {
    high_resolution_clock::time_point start = high_resolution_clock::now();
    for (int i = 0; i < 200'0000; ++i) {
      r += student_atan(values[i % num_inputs], max_error);
    }
    high_resolution_clock::time_point stop = high_resolution_clock::now();
    double elapsed_seconds =
        std::chrono::duration_cast<std::chrono::duration<double> >(stop - start)
            .count();
    std::cerr << "Time: " << elapsed_seconds << std::endl;
    best_time = std::min(best_time, elapsed_seconds);
  }
  std::cerr << "Best time: " << best_time << std::endl;
  std::cerr << r << std::endl;

  std::printf("%f\n", best_time);

  return 0;
}
