#include "state.h"
#include "moves.h"
#include "random.h"

#include <cassert>
#include <cstdio>
#include <cctype>
#include <optional>
#include <random>
#include <sstream>
#include <string_view>

namespace {

constexpr bool debug_encoding = true;

void PrintTurns(const std::vector<Turn> &turns) {
    for (size_t i = 0; i < turns.size(); ++i) {
        printf("%4d. ", (int)i + 1);
        std::cout << turns[i] << '\n';

        if (debug_encoding) {
            assert(Turn::FromString(turns[i].ToString()) == turns[i]);
        }
    }
}

void PrintState(const State &state) {
    printf("    God           Player 1    Player 2\n");
    printf("--  ------------  ----------  ----------\n");
    for (int i = 0; i < GOD_COUNT; ++i) {
        printf("%2d  %-12s",
            i + 1,
            pantheon[i].name);
        for (int p = 0; p < 2; ++p) {
            printf("  %c ", (p == 0 ? toupper : tolower)(pantheon[i].ascii_id));
            int hp = state.hp((Player)p, (God)i);
            if (hp == 0) {
                printf(" -------");
            } else {
                printf("%2d HP %2s", hp, FieldName(state.fi((Player)p, (God)i)));
            }
        }
        printf(" %s\n",pantheon[i].emoji_utf8);
    }
    std::string encoded = state.Encode();
    printf("%s\n", encoded.c_str());
    if (debug_encoding) {
        std::optional<State> decoded = State::Decode(encoded);
        if (decoded != state) {
            fprintf(stderr, "encoded: %s\n", encoded.c_str());
            fprintf(stderr, "decoded: %s\n", decoded ? decoded->Encode().c_str() : "FAILED");
            abort();
        }
    }
    printf("\n   ");
    for (int c = 0; c < BOARD_SIZE; ++c) {
        printf(" %c ", 'a' + c);
    }
    printf("\n\n");
    for (int r = BOARD_SIZE - 1; r >= 0; --r) {
        printf(" %c ", '1' + r);
        for (int c = 0; c < BOARD_SIZE; ++c) {
            int i = FieldIndex(r, c);
            if (i == -1) {
                printf("   ");
            } else if (state.IsEmpty(i)) {
                printf(" . ");
            } else {
                printf(" %c ", state.PlayerAt(i) == 0 ?
                    toupper(pantheon[state.GodAt(i)].ascii_id) :
                    tolower(pantheon[state.GodAt(i)].ascii_id));
            }
        }
        printf(" %c\n", '1' + r);
    }
    printf("\n    ");
    for (int c = 0; c < BOARD_SIZE; ++c) {
        printf(" %c ", 'a' + c);
    }
    printf("\n\n%s to move\n", state.NextPlayer() == LIGHT ? "Light" : "Dark");
}

void PrintUsage() {
    printf(
        "Usage: play [<state>] <light> <dark>\n"
        "Where light/dark is either 'user' or 'rand'\n"
    );
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

std::mt19937_64 rng = InitializeRng();

}  // namespace

int main(int argc, char *argv[]) {
    // Parse command line arugments
    if (argc < 3 || argc > 4) {
        PrintUsage();
        return 1;
    }
    PlayerType player_type[2];
    State state;
    {
        int i = 1;
        if (argc < 4) {
            state = State::Initial();
        } else if (auto s = State::Decode(argv[i])) {
            state = *s;
            ++i;
        } else {
            printf("Failed to decode state: %s\n", argv[i]);
        }
        for (int p = 0; p < 2; ++p) {
            if (auto pt = ParsePlayerType(argv[i])) {
                player_type[p] = *pt;
                ++i;
            } else {
                printf("Failed to parse player type: %s\n", argv[i]);
            }
        }
        assert(i == argc);
    }

    // Play game
    while (!state.IsOver()) {
        PrintState(state);

        std::vector<Turn> turns = GenerateTurns(state);
        printf("\n");
        PrintTurns(turns);
        printf("\n");

        assert(!turns.empty());

        auto random_turn = [&]() -> const Turn& {
            std::uniform_int_distribution<> dist(0, turns.size() - 1);
            size_t i = dist(rng);
            std::cout << "Randomly selected: " << (i + 1) << ". " << turns[i] << '\n';
            return turns[i];
        };

        std::optional<Turn> turn;
        if (player_type[state.NextPlayer()] == PLAY_RAND) {
            turn = random_turn();
        } else {
            std::string line;
            while (!turn) {
                std::cout << "Move: " << std::flush;
                if (!std::getline(std::cin, line)) {
                    std::cerr << "End of input!\n";
                    break;
                }
                if (line == "?") {
                    turn = random_turn();
                    break;
                }
                for (size_t i = 0; i < turns.size(); ++i) {
                    std::ostringstream is;
                    is << i + 1;
                    std::ostringstream ts;
                    ts << turns[i];
                    if (is.str() == line || ts.str() == line) {
                        turn = turns[i];
                        break;
                    }
                }
                if (!turn) {
                    std::cerr << "Move not recognized!\n";
                }
            }
        }
        if (!turn) {
            std::cerr << "No move selected! Quitting.\n";
            break;
        }
        ExecuteTurn(state, *turn);
    }
    if (state.IsOver()) {
        PrintState(state);
        printf("Winner: %s\n", state.NextPlayer() ? "light" : "dark");
    }
}
