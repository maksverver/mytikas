#include "state.h"

constexpr Dir knight_dirs[8] = {
    Dir{ -2, -1},
    Dir{ -2, +1},
    Dir{ -1, -2},
    Dir{ -1, +2},
    Dir{ +1, -2},
    Dir{ +1, +2},
    Dir{ +2, -1},
    Dir{ +2, +1},
};

constexpr Dir all_dirs[8] = {
    // Orthogonal dirs
    Dir{ -1,  0},
    Dir{  0, +1},
    Dir{  0, -1},
    Dir{ +1,  0},
    // Diagonal dirs
    Dir{ -1, -1},
    Dir{ -1, +1},
    Dir{ +1, -1},
    Dir{ +1, +1},
};

constexpr std::span<const Dir, 4> ortho_dirs{&all_dirs[0], &all_dirs[4]};
constexpr std::span<const Dir, 4> diag_dirs{&all_dirs[4], &all_dirs[8]};
constexpr std::span<const Dir, 0> no_dirs;

const int8_t field_index_by_coords[BOARD_SIZE][BOARD_SIZE] = {
    { -1, -1, -1, -1,  0, -1, -1, -1, -1 },
    { -1, -1, -1,  1,  2,  3, -1, -1, -1 },
    { -1, -1,  4,  5,  6,  7,  8, -1, -1 },
    { -1,  9, 10, 11, 12, 13, 14, 15, -1 },
    { 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { -1, 25, 26, 27, 28, 29, 30, 31, -1 },
    { -1, -1, 32, 33, 34, 35, 36, -1, -1 },
    { -1, -1, -1, 37, 38, 39, -1, -1, -1 },
    { -1, -1, -1, -1, 40, -1, -1, -1, -1 },
};

const uint8_t coords_by_field_index[FIELD_COUNT] = {
#define _(r, c) ((r << 4) | c)
                                     _(0,4),
                             _(1,3), _(1,4), _(1,5),
                     _(2,2), _(2,3), _(2,4), _(2,5), _(2,6),
             _(3,1), _(3,2), _(3,3), _(3,4), _(3,5), _(3,6), _(3,7),
    _(4, 0), _(4,1), _(4,2), _(4,3), _(4,4), _(4,5), _(4,6), _(4,7), _(4,8),
             _(5,1), _(5,2), _(5,3), _(5,4), _(5,5), _(5,6), _(5,7),
                     _(6,2), _(6,3), _(6,4), _(6,5), _(6,6),
                             _(7,3), _(7,4), _(7,5),
                                     _(8,4),
#undef _
};

const char *field_names[FIELD_COUNT] = {
                            "e1",
                      "d2", "e2", "f2",
                "c3", "d3", "e3", "f3", "g3",
          "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "i5",
          "b6", "c6", "d6", "e6", "f6", "g6", "h6",
                "c7", "d7", "e7", "f7", "g7",
                      "d8", "e8", "f8",
                            "e9",

};

// Must keep this in sync with the Gods enum.
constexpr GodInfo pantheon[GOD_COUNT] = {
    // name        id  emoji  hit mov dmg rng  mov_direct atk_direct mov_dirs     atk_dirs
    {"Zeus",       'Z', "⚡️", 10,   1, 10,  3, false,     true,      all_dirs,    ortho_dirs },
    {"Hephaestus", 'H', "🔨",  9,   2,  7,  2, false,     true,      ortho_dirs,  ortho_dirs },
    {"hEra",       'E', "👸",  8,   2,  5,  2, false,     false,     diag_dirs,   diag_dirs  },
    {"Poseidon",   'P', "🔱",  7,   3,  4,  0, false,     false,     ortho_dirs,  no_dirs    },
    {"apOllo",     'O', "🏹",  6,   2,  2,  3, false,     false,     all_dirs,    all_dirs   },
    {"Aphrodite",  'A', "🌹",  6,   3,  6,  1, false,     false,     all_dirs,    all_dirs   },
    {"aRes",       'R', "⚔️",  5,   3,  5,  3, true,      true,      all_dirs,    all_dirs   },
    {"herMes",     'M', "🪽",  5,   3,  3,  2, false,     true,      all_dirs,    all_dirs   },
    {"Dionysos",   'D', "🍇",  4,   1,  4,  0, false,     false,     knight_dirs, no_dirs    },
    {"arTemis",    'T', "🦌",  4,   2,  4,  2, false,     true,      all_dirs,    diag_dirs  },
    {"hadeS",      'S', "🐕",  3,   3,  3,  1, true,      false,     all_dirs,    no_dirs    },
    {"atheNa",     'N', "🛡️",  3,   1,  3,  3, false,     true,      all_dirs,    all_dirs   },
};

State State::Initial() {
    State state;
    for (int p = 0; p < 2; ++p) {
        for (int i = 0; i < GOD_COUNT; ++i) {
            state.gods[p][i] = GodState{
                .hp = pantheon[i].hit,
                .fi = -1,
                .fx = UNAFFECTED,
            };
        }
    }
    for (int i = 0; i < FIELD_COUNT; ++i) {
        state.fields[i] = FieldState::UNOCCUPIED;
    }
    state.player = LIGHT;
    return state;
}
