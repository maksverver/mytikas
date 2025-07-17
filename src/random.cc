#include "random.h"

#include <algorithm>
#include <array>
#include <random>

std::mt19937_64 InitializeRng() {
  std::array<unsigned int, 128> seed_data;  // 4096 bits
  std::random_device dev;
  std::generate_n(seed_data.data(), seed_data.size(), std::ref(dev));
  std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
  return std::mt19937_64(seq);
}
