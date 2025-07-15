#include "state.h"

#include <cstdio>
#include <cctype>

namespace {

void PrintState(const State &state) {
    printf("     God                 Player 1    Player 2\n");
    printf("-----------------------  ----------  ----------\n");
    for (int i = 0; i < GOD_COUNT; ++i) {
        printf("%2d %-20s  %c %2d HP %2s  %c %2d HP %2s  %s \n",
            i + 1,
            pantheon[i].name,
            toupper(pantheon[i].ascii_id),
            state.gods[0][i].hp,
            FieldName(state.gods[0][i].fi),
            tolower(pantheon[i].ascii_id),
            state.gods[1][i].hp,
            FieldName(state.gods[1][i].fi),
            pantheon[i].emoji_utf8);
    }
    printf("\n");
    for (int r = BOARD_SIZE - 1; r >= 0; --r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            int i = FieldIndex(r, c);
            if (i == -1) {
                printf("   ");
            } else if (!state.fields[i].occupied) {
                printf(" . ");
            } else {
                printf(" %c ", state.fields[i].player == 0 ?
                    toupper(pantheon[state.fields[i].god].ascii_id) :
                    tolower(pantheon[state.fields[i].god].ascii_id));
            }
        }
        printf("\n");
    }
}

}  // namespace

int main() {
    State state = State::Initial();
    state.fields[0].occupied = true;
    state.fields[0].player = 0;
    state.fields[0].god = 0;

    state.fields[40].occupied = true;
    state.fields[40].player = 1;
    state.fields[40].god = 11;

    PrintState(state);
}
