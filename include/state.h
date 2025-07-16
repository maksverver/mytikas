#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <utility>

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

extern const int8_t field_index_by_coords[BOARD_SIZE][BOARD_SIZE];
extern const uint8_t coords_by_field_index[FIELD_COUNT];
extern const char *field_names[FIELD_COUNT];

constexpr int gate_index[2] = {0, FIELD_COUNT - 1};

inline bool OnBoard(int r, int c) {
    return abs(r - 4) + abs(c - 4) <= 4;
}

inline int FieldIndex(int r, int c) {
    return
        0 <= r && r < BOARD_SIZE &&
        0 <= c && c < BOARD_SIZE
            ? field_index_by_coords[r][c] : -1;
}

inline const char *FieldName(int i) {
    return 0 <= i && i < FIELD_COUNT ? field_names[i] : "-";
}

inline std::pair<int, int> FieldCoords(int i) {
    assert(0 <= i && i < FIELD_COUNT);
    uint8_t v = coords_by_field_index[i];
    return {v>>4, v&15};
}

enum Gods : uint8_t {
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

struct GodInfo {
    char name[16];
    char ascii_id;
    char emoji_utf8[16];
    uint8_t hit;  // hit points (base)
    uint8_t mov;  // movement speed
    uint8_t dmg;  // attack damage (base)
    uint8_t rng;  // attack range
};

extern const GodInfo pantheon[GOD_COUNT];

enum StatusEffects : uint8_t {
    UNAFFECTED   = 0,
    DAMAGE_BOOST = 1,  // Buff: +1 damage boost from Hephaestus
    SPEED_BOOST  = 2,  // Buff: +1 move from Hermes
    SHIELDED     = 4,  // Protected by Athena's shield
    CHAINED      = 8,  // Chained by enemy Hades
};

enum Player : uint8_t {
    LIGHT = 0,
    DARK  = 1,
};

inline Player Other(Player p) { return (Player)(1 - p); }

struct GodState {
    uint8_t       hp;  // hit points left
    int8_t        fi;  // field index (or -1 if not yet summoned or dead)
    StatusEffects fx;  // bitmask of status effects
};

struct FieldState {
    bool   occupied : 1;  // 0 if empty, or 1 if occupied
    Player player   : 1;  // 0 (light) or 1 (dark)
    Gods   god      : 6;  // between 0 and GOD_COUNT (exclusive)
};

class State {
public:
    int Hp(Player player, Gods god) const { return gods[player][god].hp; }
    int Fi(Player player, Gods god) const { return gods[player][god].fi; }
    StatusEffects Fx(Player player, Gods god) const { return gods[player][god].fx; }

    bool IsEmpty(int i) const       { return !IsOccupied(i); }
    bool IsOccupied(int i) const    { return fields[i].occupied; }
    int PlayerAt(int i) const       { return fields[i].occupied ? fields[i].player : -1; }
    int GodAt(int i) const          { return fields[i].occupied ? fields[i].god : -1; }

    int Winner() const {
        if (PlayerAt(gate_index[1]) == 0) return 0;
        if (PlayerAt(gate_index[0]) == 1) return 1;
        return -1;
    }

    bool IsOver() const {
        return Winner() != -1;
    }

    inline void Summon(Gods god) {
        PlaceAt(god, gate_index[player]);
    }

    void PlaceAt(Gods god, int field) {
        PlaceAt(god, field, player);
    }
    void PlaceAt(Gods god, int field, Player player) {
        assert(!fields[field].occupied);
        fields[field] = FieldState {
            .occupied = true,
            .player   = player,
            .god      = god,
        };
        gods[player][god].fi = field;
    }

    void RemoveAt(int field) {
        assert(fields[field].occupied);
        Player player = fields[field].player;
        Gods god      = fields[field].god;
        gods[player][god].fi = field;
        fields[field] = FieldState {
            .occupied = true,
            .player   = player,
            .god      = god,
        };
        // TODO
    }

    void EndTurn() {
        player = Other(player);
    }

    static State Initial();

private:
    GodState    gods[2][GOD_COUNT];
    FieldState  fields[FIELD_COUNT];
    Player      player;
};

#endif  // ndef STATE_H_INCLUDED
