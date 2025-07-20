#include "state.h"

#include <iostream>
#include <string>
#include <string_view>

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
    // name        id  emoji  hit mov dmg rng  mov_direct atk_direct mov_dirs     atk_dirs     aura
    {"Zeus",       'Z', "⚡️", 10,   1, 10,  3, false,     true,      all_dirs,    ortho_dirs,  UNAFFECTED   },
    {"Hephaestus", 'H', "🔨",  9,   2,  7,  2, false,     true,      ortho_dirs,  ortho_dirs,  DAMAGE_BOOST },
    {"hEra",       'E', "👸",  8,   2,  5,  2, false,     false,     diag_dirs,   diag_dirs,   UNAFFECTED   },
    {"Poseidon",   'P', "🔱",  7,   3,  4,  0, false,     false,     ortho_dirs,  no_dirs,     UNAFFECTED   },
    {"apOllo",     'O', "🏹",  6,   2,  2,  3, false,     false,     all_dirs,    all_dirs,    UNAFFECTED   },
    {"Aphrodite",  'A', "🌹",  6,   3,  6,  1, false,     false,     all_dirs,    all_dirs,    UNAFFECTED   },
    {"aRes",       'R', "⚔️",  5,   3,  5,  3, true,      true,      all_dirs,    all_dirs,    UNAFFECTED   },
    {"herMes",     'M', "🪽",  5,   3,  3,  2, false,     true,      all_dirs,    all_dirs,    SPEED_BOOST  },
    {"Dionysos",   'D', "🍇",  4,   1,  4,  0, false,     false,     knight_dirs, no_dirs,     UNAFFECTED   },
    {"arTemis",    'T', "🦌",  4,   2,  4,  2, false,     true,      all_dirs,    diag_dirs,   UNAFFECTED   },
    {"hadeS",      'S', "🐕",  3,   3,  3,  1, true,      false,     all_dirs,    no_dirs,     UNAFFECTED   },
    {"atheNa",     'N', "🛡️",  3,   1,  3,  3, false,     true,      all_dirs,    all_dirs,    SHIELDED     },
};

God GodById(char ch) {
    int i = 0;
    while (i < GOD_COUNT && pantheon[i].ascii_id != ch) ++i;
    return static_cast<God>(i);
}

constexpr bool all_gods[2][GOD_COUNT] = {
    {true, true, true, true, true, true, true, true, true, true, true, true},
    {true, true, true, true, true, true, true, true, true, true, true, true},
};
static_assert(GOD_COUNT == 12);

State State::Initial() {
    return InitialWithGods(all_gods);
}

State State::InitialWithGods(const bool gods[2][GOD_COUNT]) {
    State state;
    for (int p = 0; p < 2; ++p) {
        for (int g = 0; g < GOD_COUNT; ++g) {
            state.gods[p][g] = GodState{
                .hp = gods[p][g] ? pantheon[g].hit : uint8_t{0},
                .fi = -1,
                .fx = UNAFFECTED,
            };
        }
    }
    std::fill_n(state.fields, FIELD_COUNT, FieldState::UNOCCUPIED);
    state.player = LIGHT;
    return state;
}

constexpr std::string_view base64_digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string State::Encode() const {
    std::string res;
    res += base64_digits[player];
    for (int p = 0; p < 2; ++p) {
        for (int g = 0; g < GOD_COUNT; ++g) {
            const auto &gs = gods[p][g];
            res += base64_digits[gs.hp > 0 ? gs.fi + 1 : FIELD_COUNT + 1];
            if (gs.hp > 0 && gs.fi != -1) {
                // Status effects except for CHAINED can be inferred from
                // adjacent characters, so we only encode CHAINED:
                res += base64_digits[gs.hp | ((gs.fx & CHAINED) ? 0x10 : 0)];
            }
        }
    }
    return res;
}

constexpr bool debug_decode = true;

std::optional<State> State::Decode(std::string_view sv) {
    size_t pos = 0;
    auto read = [&]<class T>(T &t, int lim) -> bool {
        assert(0 < lim && lim <= 64);
        if (pos >= sv.size()) {
            if (debug_decode) {
                std::cerr << "Decode(\"" << sv << "\"): read past end of string\n";
            }
            return false;
        }
        auto i = base64_digits.find(sv[pos++]);
        if (i >= lim) {
            if (debug_decode) {
                std::cerr
                    << "Decode(\"" << sv << "\"): base64 value " << i
                    << " exceeds limit " << lim << " at pos " << pos << "\n";
            }
            return false;
        }
        t = static_cast<T>(i);
        return true;
    };
    State state = State::Initial();
    if (!read(state.player, 2)) return {};
    for (int p = 0; p < 2; ++p) {
        for (int g = 0; g < GOD_COUNT; ++g) {
            auto &gs = state.gods[p][g];
            int8_t fi;
            if (!read(fi, FIELD_COUNT + 2)) return {};
            if (fi == FIELD_COUNT + 1) {
                // Dead
                gs.hp = 0;
            } else if (fi == 0) {
                // Not yet summoned
            } else {
                // Alive and in play
                int hpfx;
                if (!read(hpfx, (pantheon[g].hit + 1)*2)) return {};
                gs.hp = hpfx & 0xf;
                gs.fx = (hpfx & 0x10) ? CHAINED : UNAFFECTED;
                state.Place(AsPlayer(p), AsGod(g), fi - 1);
                // TODO: apply status effects by Hermes, Hepahestus, Athena
            }
        }
    }
    if (pos != sv.size()) return {};  // unexpected trailing data
    return state;
}

void State::Place(Player player, God god, field_t field) {
    assert(!fields[field].occupied);
    assert(gods[player][god].fi == -1);
    fields[field] = FieldState {
        .occupied = true,
        .player   = player,
        .god      = god,
    };
    gods[player][god].fi = field;
    // TODO: add aura effects
}

void State::Kill(Player player, God god, field_t field) {
    gods[player][god] = GodState{
        .hp =  0,
        .fi = -1,
        .fx = UNAFFECTED,
    };
    fields[field] = FieldState::UNOCCUPIED;
    // TODO: remove aura effects
    // TODO: if Hades is removed, remove chained effects
}

void State::Move(Player player, God god, field_t dst) {
    assert(!fields[dst].occupied);
    int8_t &src = gods[player][god].fi;
    assert(src != -1);
    fields[dst] = fields[src];
    fields[src] = FieldState::UNOCCUPIED;
    src = dst;
    // TODO: update status effects
}

uint8_t State::DealDamage(field_t field, int damage) {
    assert(fields[field].occupied);
    Player player = fields[field].player;
    God god = fields[field].god;
    auto &hp = gods[player][god].hp;
    if (hp > damage) {
        hp -= damage;
    } else {
        Kill(player, god, field);
    }
    return hp;
}
