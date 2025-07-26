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
    std::mt19937_64 rng;
};

std::optional<Turn> RandomPlayer::SelectTurn(const State &state) {
    std::vector<Turn> turns = GenerateTurns(state);
    assert(!turns.empty());
    std::uniform_int_distribution<> dist(0, turns.size() - 1);
    size_t i = dist(rng);
    std::cerr << "Randomly selected: " << (i + 1) << ". " << turns[i] << "\n\n";
    return std::move(turns[i]);
};

GamePlayer *CreateRandomPlayer() {
    return new RandomPlayer();
}
