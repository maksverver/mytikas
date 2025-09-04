// Evaluates the relative strengths of gods by playing multiple matches.
//
// This is done by running games where each player has 1 god missing from
// their roster. In the summary, values represent number of games won with the
// respective god missing, so a positive value means a higher chance of winning
// when the god is NOT present, so lower (negative) values indicate that gods
// are more valuable.

#include "players.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>

namespace {

constexpr int max_turns = 2000;

// Runs a single game and returns either 0 or 1 depending on which player wins,
// or -1 if the game ends in a tie.
int RunGame(const PlayerDesc &player_desc, std::array<god_mask_t, 2> summonable) {
    State state = State::InitialWithSummonable(summonable);
    std::unique_ptr<GamePlayer> players[2] = {
        std::unique_ptr<GamePlayer>{CreatePlayerFromDesc(player_desc)},
        std::unique_ptr<GamePlayer>{CreatePlayerFromDesc(player_desc)},
    };
    for (int turns = 0; turns < max_turns && !state.IsAlmostOver(); ++turns) {
        std::optional<Turn> turn = players[state.NextPlayer()]->SelectTurn(state);
        assert(turn);
        ExecuteTurn(state, *turn);
    }
    return state.AlmostWinner();
}

struct Stats {
    int total = 0;
    int wins[2] = {0, 0};
};

}  // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <player-desc>" << std::endl;
        return 1;
    }
    auto player_desc = ParsePlayerDesc(argv[1]);
    if (!player_desc) {
        std::cerr << "Couldn't parse player description!" << std::endl;
        return 1;
    }

    for (int n = 0; n < 1000; ++n) {
        Stats stats[GOD_COUNT][GOD_COUNT] = {};
        for (int i = 0; i < GOD_COUNT; ++i) {
            for (int j = 0; j < GOD_COUNT; ++j) {
                std::array<god_mask_t, 2> summonable = {ALL_GODS, ALL_GODS};
                summonable[0] &= ~GodMask(AsGod(i));
                summonable[1] &= ~GodMask(AsGod(j));
                int res = RunGame(*player_desc, summonable);
                std::cerr << '\r' << std::setw(4) << (GOD_COUNT * i + j) << " / " << (GOD_COUNT * GOD_COUNT) << " res=" << res << std::flush;
                auto &s = stats[i][j];
                s.total++;
                if (res == Player::LIGHT || res == Player::DARK) s.wins[res]++;
            }
        }

        std::cout << "\r                                                    ";
        std::cout << "After " << n + 1 << " rounds:\n";
        for (int i = 0; i < GOD_COUNT; ++i) {
            std::cout << std::noshowpos << std::setw(2) << i + 1 << std::setw(12) << pantheon[i].name << ": ";
            int row_sum = 0;
            for (int j = 0; j < GOD_COUNT; ++j) {
                auto const &s = stats[i][j];
                int value = s.wins[0] - s.wins[1];
                row_sum += value;
                std::cout << std::setw(4) << std::showpos << value;
            }
            std::cout << " = " << std::setw(4) << row_sum;
            std::cout << '\n';
        }
        std::cout << std::noshowpos << std::flush;
    }
}
