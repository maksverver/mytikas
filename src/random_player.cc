#include "moves.h"
#include "players.h"
#include "random.h"
#include "state.h"

#include <cassert>
#include <iostream>

class RandomPlayer : public GamePlayer {
public:
    RandomPlayer() : rng(InitializeRng()) {}
    std::optional<Turn> SelectTurn(const State &state) override;
private:
    rng_t rng;
};

std::optional<Turn> RandomPlayer::SelectTurn(const State &state) {
    std::vector<Turn> turns = GenerateTurns(state);
    assert(!turns.empty());
    Turn turn = Choose(rng, turns);
    std::cerr << "Randomly selected: " << turn << "\n\n";
    return turn;
};

GamePlayer *CreateRandomPlayer() {
    return new RandomPlayer();
}
