#include "moves.h"
#include "players.h"
#include "random.h"
#include "state.h"

#include <cassert>
#include <iostream>

class RandomPlayer : public GamePlayer {
public:
    RandomPlayer(bool verbose) : verbose(verbose), rng(InitializeRng()) {}
    std::optional<Turn> SelectTurn(const State &state) override;
private:
    bool verbose;
    rng_t rng;
};

std::optional<Turn> RandomPlayer::SelectTurn(const State &state) {
    std::vector<Turn> turns = GenerateTurns(state);
    assert(!turns.empty());
    Turn turn = Choose(rng, turns);
    if (verbose) std::cerr << "Randomly selected: " << turn << "\n";
    return turn;
};

GamePlayer *CreateRandomPlayer(const RandomPlayerOpts &opts) {
    return new RandomPlayer(opts.verbose);
}
