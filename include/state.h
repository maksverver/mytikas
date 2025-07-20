#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>

enum Player : uint8_t {
    LIGHT = 0,
    DARK  = 1,
};

inline Player AsPlayer(int i) {
    assert(0 <= i && i < 2);
    return (Player) i;
}

inline Player Other(Player p) { return (Player)(1 - p); }

//
//       a  b  c  d  e  f  g  h  i
//   9              40               9
//
//   8           37 38 39            8
//
//   7        32 33 34 35 36         7
//
//   6     25 26 27 28 29 30 31      6
//
//   5  16 17 18 19 20 21 22 23 24   5
//
//   4      9 10 11 12 13 14 15      4
//
//   3         4  5  6  7  8         3
//
//   2            1  2  3            2
//
//   1               0               1
//
//       a  b  c  d  e  f  g  h  i
//

constexpr int FIELD_COUNT = 41;
constexpr int BOARD_SIZE  =  9;

using field_t = int8_t;

extern const int8_t field_index_by_coords[BOARD_SIZE][BOARD_SIZE];
extern const uint8_t coords_by_field_index[FIELD_COUNT];
extern const char *field_names[FIELD_COUNT];

constexpr field_t gate_index[2] = {0, FIELD_COUNT - 1};

extern const field_t neighbors_data[];
extern const size_t neighbors_index[FIELD_COUNT + 1];

// Returns the neighbors of a field, in sorted order.
inline std::span<const field_t> Neighbors(field_t field) {
    return std::span(
        &neighbors_data[neighbors_index[field    ]],
        &neighbors_data[neighbors_index[field + 1]]);
}

// Computes the difference between old and new neighbors when moving
// from field `src` to `dst`.
//
// For each field f that was a neighbor of src but is not a neighbor of dst,
// excluding dst itself, on_old(f) is called.
//
// For each field g that is a neighbor of dst but not a neighbor of src,
// excluding src itself, on_new(g) is called.
//
// (This is used to update status effects when pieces move.)
template<typename T, typename U>
void NeighborsDiff(field_t src, field_t dst, T on_old, U on_new) {
    std::span<const field_t> src_nbs = Neighbors(src);
    std::span<const field_t> dst_nbs = Neighbors(dst);
    size_t i = 0, j = 0;
    for (;;) {
        field_t f = i < src_nbs.size() ? src_nbs[i] : FIELD_COUNT;
        field_t g = j < dst_nbs.size() ? dst_nbs[j] : FIELD_COUNT;
        if (f < g) {
            if (f != dst) on_old(f);
            ++i;
        } else if (f > g) {
            if (g != src) on_new(g);
            ++j;
        } else {  // f == g
            if (f == FIELD_COUNT) break;
            ++i, ++j;
        }
    }
}

inline bool OnBoard(int r, int c) {
    return abs(r - 4) + abs(c - 4) <= 4;
}

inline field_t FieldIndex(int r, int c) {
    return
        0 <= r && r < BOARD_SIZE &&
        0 <= c && c < BOARD_SIZE
            ? field_index_by_coords[r][c] : -1;
}

inline const char *FieldName(field_t i) {
    return 0 <= i && i < FIELD_COUNT ? field_names[i] : "-";
}

inline field_t ParseField(std::string_view sv) {
    if (sv.size() != 2) return -1;
    return FieldIndex(sv[1] - '1', sv[0] - 'a');
}

struct Coords {
    int8_t r;
    int8_t c;
};

inline Coords FieldCoords(field_t i) {
    assert(0 <= i && i < FIELD_COUNT);
    uint8_t v = coords_by_field_index[i];
    int8_t r = v>>4, c = v&15;
    return Coords{.r=r, .c=c};
}

struct Dir {
    int8_t dr;
    int8_t dc;
};

enum God : uint8_t {
    ZEUS,        //  0
    HEPHAESTUS,  //  1
    HERA,        //  2
    POSEIDON,    //  3
    APOLLO,      //  4
    APHRODITE,   //  5
    ARES,        //  6
    HERMES,      //  7
    DIONYSOS,    //  8
    ARTEMIS,     //  9
    HADES,       // 10
    ATHENA,      // 11
    GOD_COUNT
};

static_assert(GOD_COUNT == 12);

inline God AsGod(int i) {
    assert(0 <= i && i < GOD_COUNT);
    return (God) i;
}

enum StatusFx : uint8_t {
    UNAFFECTED   = 0,
    CHAINED      = 1,  // Chained by enemy Hades
    DAMAGE_BOOST = 2,  // Buff: +1 damage boost from Hephaestus
    SPEED_BOOST  = 4,  // Buff: +1 move from Hermes
    SHIELDED     = 8,  // Protected by Athena's shield
};

struct GodInfo {
    char name[16];
    char ascii_id;
    char emoji_utf8[16];
    uint8_t hit;  // hit points (base)
    uint8_t mov;  // movement speed
    uint8_t dmg;  // attack damage (base)
    uint8_t rng;  // attack range
    bool mov_direct;
    bool atk_direct;
    std::span<const Dir> mov_dirs;
    std::span<const Dir> atk_dirs;
    StatusFx aura;  // friendly effect (must be disjoint between heros)
};

extern const GodInfo pantheon[GOD_COUNT];

// Returns the god with ascii_id == ch, or GOD_COUNT if none found.
God GodById(char ch);

struct GodState {
    uint8_t  hp;  // hit points left
    int8_t   fi;  // field index (or -1 if not yet summoned or dead)
    StatusFx fx;  // bitmask of status effects

    // Mostly intedend for debugging/testing.
    auto operator<=>(const GodState &) const = default;
};

struct FieldState {
    bool   occupied : 1;  // 0 if empty, or 1 if occupied
    Player player   : 1;  // 0 (light) or 1 (dark)
    God    god      : 6;  // between 0 and GOD_COUNT (exclusive)

    static const FieldState UNOCCUPIED;

    // Mostly intended for debugging/testing.
    auto operator<=>(const FieldState &) const = default;
};

constexpr FieldState FieldState::UNOCCUPIED = FieldState{
    .occupied = false,
    .player = LIGHT,
    .god = (God)0,
};

class State {
public:
    // Returns an initialized start state.
    static State Initial();

    // Returns a start state with only the given gods (used for testing)
    static State InitialWithGods(const bool gods[2][GOD_COUNT]);

    // Decodes the string produced by Encode().
    static std::optional<State> Decode(std::string_view sv);

    // Encodes the state as a base-64 string. This is not intended to be
    // portable; it's a just a dump of the internal state so I can save and
    // restore states easily, and share them for testing purposes.
    std::string Encode() const;

    int hp(Player player, God god) const { return gods[player][god].hp; }
    int fi(Player player, God god) const { return gods[player][god].fi; }
    StatusFx fx(Player player, God god) const { return gods[player][god].fx; }

    Player NextPlayer() const { return player; }

    bool IsEmpty(field_t i) const       { return !IsOccupied(i); }
    bool IsOccupied(field_t i) const    { return fields[i].occupied; }
    int PlayerAt(field_t i) const       { return fields[i].occupied ? fields[i].player : -1; }
    God GodAt(field_t i) const          { return fields[i].occupied ? fields[i].god : GOD_COUNT; }

    bool IsDead(Player player, God god) const { return gods[player][god].hp == 0; }
    bool IsDeployed(Player player, God god) const { return gods[player][god].fi != -1; }
    bool IsSummonable(Player player, God god) const { return !IsDead(player, god) && !IsDeployed(player, god); }

    int Winner() const {
        if (PlayerAt(gate_index[1]) == 0) return 0;
        if (PlayerAt(gate_index[0]) == 1) return 1;
        return -1;
    }
    bool IsOver() const { return Winner() != -1; }

    void Summon(God god) {
        Place(player, god, gate_index[player]);
    }

    void Place(Player player, God god, field_t field);

    void Remove(Player player, God god) {
        Remove(player, god, gods[player][god].fi);
    }

    void Move(Player player, God god, field_t dst);

    uint8_t DealDamage(Player player, field_t field, int damage);

    void EndTurn() {
        player = Other(player);
    }

    // Mostly intended for debugging/testing.
    auto operator<=>(const State &) const = default;

private:
    void Remove(Player player, God god, field_t field);

    void AddFx(Player player, God god, StatusFx new_fx) {
        StatusFx &fx = gods[player][god].fx;
        fx = static_cast<StatusFx>(fx | new_fx);
    }

    void RemoveFx(Player player, God god, StatusFx old_fx) {
        StatusFx &fx = gods[player][god].fx;
        fx = static_cast<StatusFx>(fx & ~old_fx);
    }

    bool HasFx(Player player, God god, StatusFx fx) const {
        return (gods[player][god].fx & fx) == fx;
    }

    GodState    gods[2][GOD_COUNT];
    FieldState  fields[FIELD_COUNT];
    Player      player;
};

#endif  // ndef STATE_H_INCLUDED
