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
            state.Hp((Player)0, (Gods)i),
            FieldName(state.Fi((Player)0, (Gods)i)),
            tolower(pantheon[i].ascii_id),
            state.Hp((Player)1, (Gods)i),
            FieldName(state.Fi((Player)1, (Gods)i)),
            pantheon[i].emoji_utf8);
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
    printf("\n");
}

}  // namespace

int main() {
    State state = State::Initial();
    state.Summon(ZEUS);
    state.EndTurn();
    state.Summon(HADES);
    PrintState(state);
}
