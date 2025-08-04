// Implements an AI player based on Monte Carlo Tree Search.
//
// NOTE: current implementation applies Monte Carlo simulations only, it does
// not build a search tree at all.

#include "moves.h"
#include "players.h"
#include "random.h"
#include "state.h"

#include <cassert>

#include <iostream>

namespace {

constexpr int playouts_per_node = 100;

void PlayOutRandomly(State &state, rng_t &rng) {
    while (!state.IsAlmostOver()) {
        std::vector<Turn> turns = GenerateTurns(state);
        assert(!turns.empty());
        ExecuteTurn(state, Choose(rng, turns));
    }
}

int FinalScore(Player player, const State &state) {
    int w = state.AlmostWinner();
    return w == player ? 2 : w == -1 ? 1 : 0;
}

}  // namespace

class MctsPlayer : public GamePlayer {
public:
    MctsPlayer(): rng(InitializeRng()) {}
    std::optional<Turn> SelectTurn(const State &state) override;

private:
    std::mt19937_64 rng;
};

std::optional<Turn> MctsPlayer::SelectTurn(const State &state) {
    std::optional<Turn> best_turn;
    const Player player = state.NextPlayer();
    const std::vector<Turn> turns = GenerateTurns(state);
    const int samples = 100;
    int min_wins = 2*samples + 1;
    int max_wins = -1;
    for (const Turn &turn : turns) {
        State next_state = state;
        ExecuteTurn(next_state, turn);
        int wins = 0;
        for (int n = 0; n < samples; ++n) {
            State final_state = next_state;
            PlayOutRandomly(final_state, rng);
            wins += FinalScore(player, final_state);
        }
        if (wins < min_wins) {
            min_wins = wins;
        }
        if (wins > max_wins) {
            max_wins = wins;
            best_turn = turn;
        }
    }
    std::cerr << "min_wins=" << min_wins << " max_wins=" << max_wins;
    if (best_turn) std::cerr << " best_turn=" << *best_turn;
    std::cerr << '\n';
    return best_turn;
}

GamePlayer *CreateMctsPlayer() {
    return new MctsPlayer();
}
