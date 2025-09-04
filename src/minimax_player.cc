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

constexpr int default_max_search_depth = 4;
constexpr int inf = 999999999;
constexpr int win = 100000000;

int Evaluate(const State &state, bool experiment) {
    // Very simplistic:
    int score[2] = {0, 0};
    for (int p = 0; p < 2; ++p) {
        for (int g = 0; g < GOD_COUNT; ++g) {
            Player player = AsPlayer(p);
            God god = AsGod(g);

            // Score HP remaining
            //
            // TODO: instead of absolute diff, we should probably use relative
            // diff, to avoid cases where one player is clearly leading but
            // unwilling to lose e.g. 4 HP to kill a 3 HP enemy because it will
            // make the absolute score go down.
            score[p] += state.hp(player, god) * 1000;

            field_t field = state.fi(player, god);
            if (field != -1) {
                // Score distance to enemy's gate. Note: this isn't a great
                // metric for Hera (who may be swapped off the main diagonals)
                // and Dionysus; I should fix this later.
                //
                // Note that this intrinsically values having gods in play,
                // too, since only if field != -1 is the bonus applied.
                auto [r1, c1] = FieldCoords(field);
                auto [r2, c2] = FieldCoords(gate_index[1 - p]);
                int dist = abs(r1 - r2) + abs(c1 - c2);
                score[p] += 100*(10 - dist);

                if (experiment) {
                    // Score auras. This doesn't seem to have too noticable of
                    // an impact on playing strength.
                    StatusFx fx = state.fx(player, god);
                    if (fx & CHAINED) score[p] -= 10;
                    // if (fx & DAMAGE_BOOST)  score[p] +=  1;
                    // if (fx & SPEED_BOOST)   score[p] +=  3;
                    // if (fx & SHIELDED)      score[p] +=  5;
                }
            }
        }
    }
    Player player = state.NextPlayer();
    Player opponent = Other(player);
    return score[player] - score[opponent];
}

int Search(const State &state, int depth_left, int alpha, int beta, bool experiment);

void ReorderMoves(const State &state, std::vector<Turn> &turns, int depth, bool experiment) {
    assert(depth > 0);
    std::vector<std::pair<int, Turn>> tmp;
    tmp.reserve(turns.size());
    for (const Turn &turn : turns) {
        // TODO: this is doing duplicate work, maybe it makes sense to combine
        // this into Search() which also evaluates the new states for all turns?
        State new_state = state;
        ExecuteTurn(new_state, turn);
        int value = -Search(new_state, depth - 1, -inf, inf, experiment);
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
int Search(const State &state, int depth_left, int alpha, int beta, bool experiment) {
    if (state.IsOver()) {
        assert(state.Winner() == Other(state.NextPlayer()));
        // Next player loses. Value is discounted by how deep down the search
        // tree it is, to force winning sooner rather than alter.
        return -(win + depth_left - default_max_search_depth);
    }

    if (depth_left == 0) {
        return Evaluate(state, experiment);
    }

    std::vector<Turn> turns = GenerateTurns(state);
    if (depth_left > 2) ReorderMoves(state, turns, depth_left - 2, experiment);

    int best_value = -inf;
    for (const Turn &turn : turns) {
        State new_state = state;
        ExecuteTurn(new_state, turn);
        int value = -Search(new_state, depth_left - 1, -beta, -alpha, experiment);
        if (value > best_value) {
            best_value = value;
            if (value >= beta) break;  // beta cut-off
            if (value > alpha) alpha = value;
        }
    }
    return best_value;
}

int FindBestTurns(const State &state, int search_depth, std::vector<Turn> &best_turns, bool experiment) {
    assert(search_depth > 0 && !state.IsOver());
    best_turns.clear();

    std::vector<Turn> turns = GenerateTurns(state);
    if (search_depth > 2) ReorderMoves(state, turns, search_depth - 2, experiment);

    int best_value = -inf;
    for (const Turn &turn : turns) {
        State new_state = state;
        ExecuteTurn(new_state, turn);
        // +1 here allows collecting all the best moves, instead of just the first:
        int value = -Search(new_state, search_depth - 1, -inf, -best_value + 1, experiment);
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
    MinimaxPlayer(int max_search_depth, bool experiment, bool verbose) :
            rng(InitializeRng()),
            max_search_depth(max_search_depth),
            experiment(experiment),
            verbose(verbose) {}

    std::optional<Turn> SelectTurn(const State &state) override;

private:
    rng_t rng;
    int max_search_depth;
    bool experiment;
    bool verbose;
};

std::optional<Turn> MinimaxPlayer::SelectTurn(const State &state) {
    std::vector<Turn> turns;
    int value = FindBestTurns(state, max_search_depth, turns, experiment);
    assert(!turns.empty());
    int start_value = Evaluate(state, experiment);
    if (verbose) {
        std::cerr << "Minimax value: " << value << " (" << (value > start_value ? "+" : "") << (value - start_value) << ")\n";
        std::cerr << "Optimal turns:";
        for (const Turn &turn : turns) std::cerr << ' ' << turn;
        std::cerr << '\n';
    }
    Turn turn;
    if (turns.size() == 1) {
        // Only one choice.
        turn = turns[0];
    } else {
        turn = Choose(rng, turns);
        if (verbose) std::cerr << "Randomly selected: " << turn << '\n';
    }
    return turn;
}

GamePlayer *CreateMinimaxPlayer(const MinimaxPlayerOpts &opts) {
    int max_depth = opts.max_depth > 0 ? opts.max_depth : default_max_search_depth;
    return new MinimaxPlayer(max_depth, opts.experiment, opts.verbose);
}
