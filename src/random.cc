#include "random.h"

#include <algorithm>
#include <array>
#include <random>

rng_t InitializeRng() {
  std::array<unsigned int, 128> seed_data;  // 4096 bits
  std::random_device dev;
  std::generate_n(seed_data.data(), seed_data.size(), std::ref(dev));
  std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
  return rng_t(seq);
}
