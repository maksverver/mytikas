#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include <cassert>
#include <random>
#include <ranges>

using rng_t = std::mt19937_64;

// Returns a Mersenne Twister pseudo-random number generator that is properly
// seeded from a random device. This should produce high-quality pseudo-random
// numbers that are different for every instance returned.
rng_t InitializeRng();

// Returns a random element from a range, which must not be empty.
template<class R>
    requires std::ranges::sized_range<R> && std::ranges::random_access_range<R>
 std::ranges::range_reference_t<R> Choose(rng_t &rng, R &range) {
    auto size = std::ranges::size(range);
    assert(size > 0);
    return *(range.begin() + std::uniform_int_distribution<>(0, size - 1)(rng));
}

#endif  // ndef RANDOM_H_INCLUDED
