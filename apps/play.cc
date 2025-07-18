#include "state.h"
#include "moves.h"
#include "random.h"

#include <cassert>
#include <cstdio>
#include <cctype>
#include <random>

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

}  // namespace

int main() {
    std::mt19937_64 rng = InitializeRng();
    State state = State::Initial();
    while (!state.IsOver()) {
        PrintState(state);

        std::vector<Turn> turns = GenerateTurns(state);
        printf("\n");
        PrintTurns(turns);
        printf("\n");

        assert(!turns.empty());

        std::uniform_int_distribution<> dist(0, turns.size() - 1);
        size_t i = dist(rng);

        printf("Randomly selected: %d. ", (int)i + 1);
        std::cout << turns[i] << '\n';
        ExecuteTurn(state, turns[i]);
        printf("\n");
    }
    PrintState(state);
    printf("Winner: %s\n", state.NextPlayer() ? "light" : "dark");
}
