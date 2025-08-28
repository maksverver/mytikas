#include "cli.h"
#include "state.h"
#include "moves.h"
#include "players.h"

#include <cassert>
#include <memory>
#include <optional>
#include <string_view>

namespace {

void PrintUsage() {
    std::cout <<
        "Usage: play <light> <dark> [<state>]\n"
        "\n"
        "Where <light> and <dark> is a player descriptor, which must be one of:\n"
        "\n"
        "   cli:      play manually via the command line interface\n"
        "   random:   play randomly\n"
        "   minimax:  Minimax algorithm\n"
        "   mcts:     Monte Carlo Tree Search algorithm (incomplete)\n"
        "\n"
        "Algorithm specific options:\n"
        "\n"
        "   minimax,max_depth=<n>   Maximum search depth (default: 4)\n"
        "   minimax,experiment      Enable experimental behavior (do not use)\n"
        "\n";
}

}  // namespace

int main(int argc, char *argv[]) {
    // Parse command line arugments
    if (argc < 3 || argc > 4) {
        PrintUsage();
        return 1;
    }
    std::unique_ptr<GamePlayer> game_players[2] = {};
    State state;
    {
        int argi = 1;
        for (int p = 0; p < 2; ++p) {
            if (auto pt = ParsePlayerDesc(argv[argi]); !pt) {
                std::cerr << "Failed to parse player type: " << argv[argi] << '\n';
                return 1;
            } else {
                game_players[p].reset(CreatePlayerFromDesc(*pt));
            }
            ++argi;
        }
        if (argi < argc) {
            if (auto s = State::Decode(argv[argi]); !s) {
                std::cerr << "Failed to decode state: " << argv[argi] << '\n';
                return 1;
            } else {
                state = *s;
            }
            ++argi;
        } else {
            state = State::InitialAllSummonable();
        }
        assert(argi == argc);
    }

    // Play game
    while (!state.IsOver()) {
        PrintState(state);
        std::cout << '\n';

        std::optional<Turn> turn = game_players[state.NextPlayer()]->SelectTurn(state);
        if (!turn) {
            std::cerr << "No move selected! Quitting.\n";
            break;
        }
        State old_state = state;
        ExecuteTurn(state, *turn);
        PrintDiff(old_state, state);
        std::cout << '\n';
    }
    if (state.IsOver()) {
        PrintState(state);
        std::cout
            << "Winner: "
            << (state.NextPlayer() ? "light" : "dark")
            << '\n';
    }
}
