// Implements an AI player based on Minimax search with alpha/beta-pruning
// and move ordering heuristic.

#include "moves.h"
#include "players.h"
#include "random.h"
#include "state.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace {

constexpr int max_search_depth = 4;
constexpr int inf = 999999999;
constexpr int win = 100000000;

int Evaluate(const State &state) {
    // Very simplistic:
    int score[2] = {0, 0};
    for (int p = 0; p < 2; ++p) {
        for (int g = 0; g < GOD_COUNT; ++g) {
            score[p] += state.hp(AsPlayer(p), AsGod(g));
        }
    }
    Player player = state.NextPlayer();
    Player opponent = Other(player);
    return score[player] - score[opponent];
}

int Search(const State &state, int depth_left, int alpha, int beta);

void ReorderMoves(const State &state, std::vector<Turn> &turns, int depth) {
    assert(depth > 0);
    std::vector<std::pair<int, Turn>> tmp;
    tmp.reserve(turns.size());
    for (const Turn &turn : turns) {
        // TODO: this is doing duplicate work, maybe it makes sense to combine
        // this into Search() which also evaluates the new states for all turns?
        State new_state = state;
        ExecuteTurn(new_state, turn);
        int value = -Search(new_state, depth - 1, -inf, inf);
        tmp.push_back({value, turn});
    }
    std::sort(tmp.rbegin(), tmp.rend());
    for (size_t i = 0; i < turns.size(); ++i) {
        turns[i] = tmp[i].second;
    }
}

// Uses minimax search with alpha-beta pruning to determine the value of the
// game search tree of depth `depth_left`.
//
// Returns an exact value strictly between alpha and beta, or an upper bound
// less than or equal to alpha, or a lower bound greater than or equal to beta.
int Search(const State &state, int depth_left, int alpha, int beta) {
    if (state.IsOver()) {
        assert(state.Winner() == Other(state.NextPlayer()));
        // Next player loses. Value is discounted by how deep down the search
        // tree it is, to force winning sooner rather than alter.
        return -(win + depth_left - max_search_depth);
    }

    if (depth_left == 0) {
        return Evaluate(state);
    }

    std::vector<Turn> turns = GenerateTurns(state);
    if (depth_left > 2) ReorderMoves(state, turns, depth_left - 2);

    int best_value = -inf;
    for (const Turn &turn : turns) {
        State new_state = state;
        ExecuteTurn(new_state, turn);
        int value = -Search(new_state, depth_left - 1, -beta, -alpha);
        if (value > best_value) {
            best_value = value;
            if (value >= beta) break;  // beta cut-off
            if (value > alpha) alpha = value;
        }
    }
    return best_value;
}

int FindBestTurns(const State &state, int search_depth, std::vector<Turn> &best_turns) {
    assert(search_depth > 0 && !state.IsOver());
    best_turns.clear();

    std::vector<Turn> turns = GenerateTurns(state);
    if (search_depth > 2) ReorderMoves(state, turns, search_depth - 2);

    int best_value = -inf;
    for (const Turn &turn : turns) {
        State new_state = state;
        ExecuteTurn(new_state, turn);
        // +1 here allows collecting all the best moves, instead of just the first:
        int value = -Search(new_state, search_depth - 1, -inf, -best_value + 1);
        if (value == best_value) {
            best_turns.push_back(turn);
        } else if (value > best_value) {
            best_turns.clear();
            best_turns.push_back(turn);
            best_value = value;
        }
    }
    return best_value;
}

}  // namespace

class MinimaxPlayer : public GamePlayer {
public:
    MinimaxPlayer(): rng(InitializeRng()) {}
    std::optional<Turn> SelectTurn(const State &state) override;

private:
    rng_t rng;
};

std::optional<Turn> MinimaxPlayer::SelectTurn(const State &state) {
    std::vector<Turn> turns;
    int value = FindBestTurns(state, max_search_depth, turns);
    assert(!turns.empty());
    int start_value = Evaluate(state);
    std::cerr << "Minimax value: " << value << " (" << (value > start_value ? "+" : "") << (value - start_value) << ")\n";
    std::cerr << "Optimal turns:";
    for (const Turn &turn : turns) std::cerr << ' ' << turn;
    std::cerr << '\n';
    Turn turn;
    if (turns.size() == 1) {
        // Only one choice.
        turn = turns[0];
    } else {
        turn = Choose(rng, turns);
        std::cerr << "Randomly selected: " << turn << '\n';
    }
    return turn;
}

GamePlayer *CreateMinimaxPlayer() {
    return new MinimaxPlayer();
}
