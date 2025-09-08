// Evaluates the relative strengths of gods by playing multiple matches.
//
// This is done by running games where each player has 1 god missing from
// their roster. In the summary, values represent number of games won with the
// respective god missing, so a positive value means a higher chance of winning
// when the god is NOT present, so lower (negative) values indicate that gods
// are more valuable.

#include "players.h"

#include <array>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>

namespace {

constexpr int max_turns_without_progress = 100;

// To prevent infinite loops, we rule the game a draw if there are a 100 turns
// (counting both players) in which:
//
//  - no god was summoned, and
//  - no damage was dealt.
//
struct GameProgress {
    std::array<god_mask_t, 2> gods_in_play;
    std::array<int, 2> health_in_play;

    static GameProgress FromState(const State &state) {
        GameProgress progress = {
            .gods_in_play   = {0, 0},
            .health_in_play = {0, 0},
        };
        for (int p = 0; p < 2; ++p) {
            for (int g = 0; g < GOD_COUNT; ++g) {
                Player player = (Player) p;
                God    god    = (God) g;
                if (state.IsInPlay(player, god)) {
                    progress.gods_in_play[p]   |= GodMask(god);
                    progress.health_in_play[p] += state.hp(player, god);
                }
            }
        }
        return progress;
    }

    auto operator<=>(const GameProgress &) const = default;
};

// Runs a single game and returns either 0 or 1 depending on which player wins,
// or -1 if the game ends in a tie.
int RunGame(const PlayerDesc &player_desc, std::array<god_mask_t, 2> summonable) {
    State state = State::InitialWithSummonable(summonable);
    std::unique_ptr<GamePlayer> players[2] = {
        std::unique_ptr<GamePlayer>{CreatePlayerFromDesc(player_desc)},
        std::unique_ptr<GamePlayer>{CreatePlayerFromDesc(player_desc)},
    };
    GameProgress last_progress = GameProgress::FromState(state);
    int turns_without_progress = 0;
    while (turns_without_progress < max_turns_without_progress) {
        if (state.IsAlmostOver()) {
            return state.AlmostWinner();
        }
        std::optional<Turn> turn = players[state.NextPlayer()]->SelectTurn(state);
        assert(turn);
        ExecuteTurn(state, *turn);
        GameProgress next_progress = GameProgress::FromState(state);
        turns_without_progress = last_progress == next_progress ? turns_without_progress + 1 : 0;
        last_progress = next_progress;
    }
    return -1;  // too many turns without progress
}

std::string GodMaskToString(god_mask_t mask) {
    std::string s;
    for (int g = 0; g < GOD_COUNT; ++g) {
        if ((mask & GodMask(AsGod(g))) != 0) {
            s += pantheon[g].ascii_id;
        }
    }
    return s;
}

struct TeamStats {
    // Summonable gods on each side.
    std::array<god_mask_t, 2> summonable = { 0, 0 };

    std::string Desc() const {
        return GodMaskToString(summonable[0]) + "/" + GodMaskToString(summonable[1]);
    }

    // Total times this matchup was played
    int total = 0;

    // Number of times dark/light won.
    int wins[2] = {0, 0};
};

void GenerateTeams(
        std::vector<TeamStats> &res,
        god_mask_t have,
        int need,
        int next_god) {
    if (need == 0) {
        TeamStats stats = {};
        stats.summonable[0] = have;
        stats.summonable[1] = ALL_GODS & ~have;
        res.push_back(stats);
        return;
    }
    if (GOD_COUNT - next_god < need) return;

    GenerateTeams(res, have | GodMask(AsGod(next_god)), need - 1, next_god + 1);
    GenerateTeams(res, have, need, next_god + 1);
}

void PrintSummary(const std::vector<TeamStats> &teams) {
    struct GodStats {
        int idx = 0;
        int win = 0;
        int loss = 0;
        int tie = 0;

        int Total() const { return win + loss + tie; };
        double WinRate() const { return (win + 0.5*tie) / Total(); }

    } god_stats[GOD_COUNT];

    for (int g = 0; g < GOD_COUNT; ++g) {
        god_stats[g].idx = g;
        for (const auto &team : teams) {
            if (team.summonable[0] & GodMask(AsGod(g))) {
                god_stats[g].win  += team.wins[0];
                god_stats[g].loss += team.wins[1];
                god_stats[g].tie  += team.total - team.wins[0] - team.wins[1];
            }
            if (team.summonable[1] & GodMask(AsGod(g))) {
                god_stats[g].win  += team.wins[1];
                god_stats[g].loss += team.wins[0];
                god_stats[g].tie  += team.total - team.wins[0] - team.wins[1];
            }
        }
    }

    std::ranges::sort(god_stats, {}, &GodStats::WinRate);
    std::ranges::reverse(god_stats);

    for (const auto &s : god_stats) {
        std::cerr << std::setw(2) << std::right << s.idx + 1 << ' '
            << std::setw(20) << std::left << pantheon[s.idx].name
            << std::fixed << std::setprecision(3) << s.WinRate() << '\n';
    }
}

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

    // This plays random matches where each player has 11 out of 12 gods,
    // with one missing, and counting the outcomes.
    //
    // It reports the difference in number of games won by a player when
    // that god is missing, which means negative values imply a god is
    // stronger.
    //
    // Unfortunately, it didn't produce very meaningful results. The outcomes
    // even after a 1000 rounds (12×12×1000 = 144000 games) seemed more or less
    // random with max_depth=2, and playing so many games with max_depth=4
    // takes too long.
    if (false) {
        for (int n = 0; n < 1000; ++n) {
            TeamStats stats[GOD_COUNT][GOD_COUNT] = {};
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

    // Second attempt. Instead of omitting one god, I randomly assign 6 gods to
    // one player and 6 gods to the other. The benefit is that games go much
    // faster even at max_depth=4 due to the lower branching factors.
    //
    // The main downside is that this is not how the game is played realistically
    // (e.g., neither player will ever have more than 6 gods in play at the same
    // time, even though that's perfectly possible in the real game) but I'm
    // hoping it will still give me some information about relative strengths.
    //
    // There are 12!/6!/6! = 12*11*10*9*8*7/6/5/4/3/2/1 = 924 ways to choose a
    // team of 6 gods. We'll just do all of them in a round.
    std::vector<TeamStats> teams;
    GenerateTeams(teams, 0, 6, 0);
    std::cerr << teams.size() << '\n';
    assert(teams.size() == 924);

    long games_played = 0;
    for (int rounds = 0; rounds < 1000; ++rounds) {
        for (auto &team : teams) {
            int res = RunGame(*player_desc, team.summonable);
            team.total++;
            if (res != -1) {
                team.wins[res]++;
            }
            std::cout << team.Desc() << " " << res << '\n';

            games_played++;
            if (games_played % 100 == 0) {
                std::cerr << "\nSummary after " << games_played << " games played:\n";
                PrintSummary(teams);
            }
        }
    }
    std::cerr << "\nFinal summary:\n";
    PrintSummary(teams);
}
