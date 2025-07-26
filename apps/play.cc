#include "cli.h"
#include "state.h"
#include "moves.h"
#include "players.h"

#include <cassert>
#include <optional>
#include <string_view>

namespace {

void PrintUsage() {
    std::cout <<
        "Usage: play <light> <dark> [<state>]\n"
        "Where light/dark is either 'user' or 'rand'\n";
}

enum PlayerType {
    PLAY_RAND,
    PLAY_USER
};

std::optional<PlayerType> ParsePlayerType(std::string_view sv) {
    if (sv == "r" || sv == "rand") return PLAY_RAND;
    if (sv == "u" || sv == "user") return PLAY_USER;
    return {};
}

}  // namespace

int main(int argc, char *argv[]) {
    // Parse command line arugments
    if (argc < 3 || argc > 4) {
        PrintUsage();
        return 1;
    }
    GamePlayer *game_players[2] = {};
    State state;
    {
        int argi = 1;
        for (int p = 0; p < 2; ++p) {
            if (auto pt = ParsePlayerType(argv[argi]); !pt) {
                std::cerr << "Failed to parse player type: " << argv[argi] << '\n';
                return 1;
            } else {
                switch (*pt) {
                case PLAY_USER:
                    game_players[p] = CreateCliPlayer();
                    break;

                case PLAY_RAND:
                    game_players[p] = CreateRandomPlayer();
                    break;

                default:
                    std::cerr << "Unsupported player type!\n";
                    return 1;
                }
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
            state = State::Initial();
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
