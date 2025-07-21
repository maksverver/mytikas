#include "moves.h"

#include <sstream>

namespace {

// A rectangular area to be attacked. Includes all fields with coords (r, c)
// where r1 <= r <= r2 and c1 <= c <= c2 (i.e., all endpoints are inclusive).
struct Area {
    int r1, c1, r2, c2;

    bool Contains(field_t field) const {
        auto [r, c] = FieldCoords(field);
        return Contains(r, c);
    }

    bool Contains(int r, int c) const {
        return r1 <= r && r <= r2 && c1 <= c && c <= c2;
    }

    static Area Get(Player player, God god, field_t field) {
        auto [r, c] = FieldCoords(field);
        int r1, c1, r2, c2;
        switch (god) {
            case POSEIDON:
                c1 = c - 1;
                c2 = c + 1;
                if (player == LIGHT) {
                    r1 = r + 1;
                    r2 = r + 2;
                } else {
                    r1 = r - 2;
                    r2 = r - 1;
                }
                break;
            case DIONYSOS:
                c1 = c2 = c;
                r1 = r2 = r;
                // TODO
                break;
            case HADES:
                c1 = c2 = c;
                r1 = r2 = r;
                // TODO
                break;
            default:
                assert(false);
        }
        if (r1 < 0) r1 = 0;
        if (c1 < 0) c1 = 0;
        if (r2 >= BOARD_SIZE) r2 = BOARD_SIZE - 1;
        if (c2 >= BOARD_SIZE) c2 = BOARD_SIZE - 1;
        return Area{r1, c1, r2, c2};
    }
};

void GenerateSummons(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    bool may_move_after);

void GenerateMovesOne(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    field_t field, bool may_summon_after
) {
    const God god = state.GodAt(field);
    assert(god < GOD_COUNT);

    // TODO: support Hermes movement boost! (make sure Artemis, Dionysus work correctly)
    const int max_dist = pantheon[god].mov;
    std::span<const Dir> dirs = pantheon[god].mov_dirs;

    auto add_action = [&](field_t field) {
        Action action{
            .type      = Action::MOVE,
            .god       = god,
            .field     = field,
        };
        turn.actions[turn.naction++] = action;
        turns.push_back(turn);
        if (may_summon_after) {
            State new_state = state;
            ExecuteAction(new_state, action);
            GenerateSummons(new_state, turns, turn, false);
        }
        // TODO: if Ares kills an enemy at their gate by moving next to
        // them, he should get to move again
        --turn.naction;
    };

    // The logic below is similar to GenerateAttacksOne(), defined below.
    // Try to keep the two in sync.

    if (pantheon[god].mov_direct) {
        // Direct moves only: scan each direction until we reach the end of the
        // board or an occupied field.
        Coords coords = FieldCoords(field);
        for (auto [dr, dc] : dirs) {
            auto [r, c] = coords;
            for (int dist = 1; dist <= max_dist; ++dist) {
                r += dr;
                c += dc;
                field_t i = FieldIndex(r, c);
                if (i == -1 || state.IsOccupied(i)) break;
                add_action(i);
            }
        }
    } else {
        // Indirect moves: breadth first search from the start.
        //
        // (Currently we don't allow a hero to land on the same field they
        // started from, which is implemented by initializing seen[field] to
        // true. The distinction only matters for Ares, but since he only moves
        // in one direction, the distinction doesn't matter.)
        Coords todo[FIELD_COUNT];
        bool seen[FIELD_COUNT] = {};
        seen[field] = true;
        todo[0] = FieldCoords(field);
        int pos = 0, end = 1;
        for (int dist = 1; dist <= max_dist; ++dist) {
            int cur_end = end;
            while (pos < cur_end) {
                auto [r, c] = todo[pos++];
                for (auto [dr, dc] : dirs) {
                    int8_t rr = r + dr;
                    int8_t cc = c + dc;
                    field_t i = FieldIndex(rr, cc);
                    if (i == -1 || state.IsOccupied(i) || seen[i]) continue;
                    add_action(i);
                    seen[i] = true;
                    todo[end++] = Coords{rr, cc};
                }
            }
        }
    }
    // TODO: special moves (Hades), Hermes
}

void GenerateMovesAll(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    bool may_summon_after
) {
    const Player player = state.NextPlayer();
    const field_t gate = gate_index[player];
    for (field_t i = 0; i < FIELD_COUNT; ++i) {
        if (state.PlayerAt(i) == player) {
            GenerateMovesOne(state, turns, turn, i, i == gate && may_summon_after);
        }
    }
    // TODO: support Hermes movement boost!
}

void GenerateAttacksOne(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    field_t field
) {
    const Player player = state.NextPlayer();
    const Player opponent = Other(player);
    const God god = state.GodAt(field);
    assert(god < GOD_COUNT);

    int max_dist = pantheon[god].rng;
    std::span<const Dir> dirs = pantheon[god].atk_dirs;

    auto add_action_with_area = [&](field_t field, Area area) {
        Action action{
            .type      = Action::ATTACK,
            .god       = god,
            .field     = field,
        };
        turn.actions[turn.naction++] = action;
        turns.push_back(turn);
        if (area.Contains(gate_index[opponent])) {
            State new_state = state;
            ExecuteAction(new_state, action);
            if (!new_state.IsOccupied(field)) {
                // Special rule 3: when you kill an enemy on the opponent's
                // gate, you get an extra move.
                GenerateMovesAll(new_state, turns, turn, false);
            }
        }
        --turn.naction;
    };
    auto add_action = [&](field_t field) {
        auto [r, c] = FieldCoords(field);
        add_action_with_area(field, Area{r, c, r, c});
    };

    if (dirs.empty()) {
        // Area attacks. Handle specially.
        Area area = Area::Get(player, god, field);
        for (int r = area.r1; r <= area.r2; ++r) {
            for (int c = area.c1; c <= area.c2; ++c) {
                if (int i = FieldIndex(r, c); i != -1 && state.PlayerAt(i) == opponent) {
                    add_action_with_area(field, area);
                    goto done;
                }
            }
        }
    done:
        return;
    }

    // The logic below is similar to GenerateMovesOne(), defined above.
    // Try to keep the two in sync.

    if (pantheon[god].atk_direct) {
        // Direct attacks only: scan each direction until we reach the end of
        // the board or an occupied field.
        Coords coords = FieldCoords(field);
        for (auto [dr, dc] : dirs) {
            auto [r, c] = coords;
            for (int dist = 1; dist <= max_dist; ++dist) {
                r += dr;
                c += dc;
                field_t i = FieldIndex(r, c);
                if (i == -1) break;
                int pl = state.PlayerAt(i);
                if (pl == opponent) add_action(i);

                if (god == ZEUS) {
                    // Zeus' special attack can pass over enemies and allies
                } else {
                    if (pl != -1) break;
                }
            }
        }
    } else {
        // Indirect attacks: breadth first search from the start.
        Coords todo[FIELD_COUNT];
        bool seen[FIELD_COUNT] = {};
        seen[field] = true;
        todo[0] = FieldCoords(field);
        int pos = 0, end = 1;
        for (int dist = 1; dist <= max_dist; ++dist) {
            int cur_end = end;
            while (pos < cur_end) {
                auto [r, c] = todo[pos++];
                for (auto [dr, dc] : dirs) {
                    int8_t rr = r + dr;
                    int8_t cc = c + dc;
                    field_t i = FieldIndex(rr, cc);
                    if (i == -1 || seen[i]) continue;
                    seen[i] = true;
                    int pl = state.PlayerAt(i);
                    if (pl == -1) {
                        todo[end++] = Coords{rr, cc};
                    } else if (pl == opponent) {
                        add_action(i);
                    }
                }
            }
        }
    }
}

void GenerateAttacksAll(
    const State &state, std::vector<Turn> &turns, Turn &turn
) {
    const Player player = state.NextPlayer();
    for (field_t i = 0; i < FIELD_COUNT; ++i) {
        if (state.PlayerAt(i) == player) {
            GenerateAttacksOne(state, turns, turn, i);
        }
    }
}

void GenerateSummons(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    bool may_move_after
) {
    const Player player = state.NextPlayer();
    const field_t gate = gate_index[player];

    if (state.IsOccupied(gate)) return;

    for (int g = 0; g != GOD_COUNT; ++g) {
        if (state.IsSummonable(player, (God) g)) {
            Action action{
                .type      = Action::SUMMON,
                .god       = AsGod(g),
                .field     = gate,
            };
            turn.actions[turn.naction++] = action;
            turns.push_back(turn);
            State new_state = state;
            ExecuteAction(new_state, action);

            if (may_move_after) {
                GenerateMovesOne(new_state, turns, turn, gate, false);
            }

            GenerateAttacksOne(new_state, turns, turn, gate);

            turn.naction--;
        }
    }
}

}  // namespace

std::vector<Turn> GenerateTurns(const State &state) {
    std::vector<Turn> turns;

    Turn turn{.naction=0, .actions={}};
    GenerateSummons(state, turns, turn, true);
    assert(turn.naction == 0);

    GenerateMovesAll(state, turns, turn, true);
    assert(turn.naction == 0);

    GenerateAttacksAll(state, turns, turn);
    assert(turn.naction == 0);

    if (turns.empty()) {
        // Is passing always allowed?
        turns.push_back(Turn{.naction=0, .actions={}});
    }
    return turns;
}

static int GetDamage(const State &state, Player player, God god, field_t target) {
    int damage = pantheon[god].dmg;

    switch (god) {
        // Hera does double damage when attacking from the side or behind.
        //
        // Note the rules don't specify if doubling happens before or after
        // Hephaestus' damage boost. Here I do before, but it doesn't really
        // matter because the base damage is 5 and max HP is 10, so double
        // damage can insta-kill any god anyway.
        case HERA:
            {
                field_t source = state.fi(player, god);
                int delta = FieldCoords(target).r - FieldCoords(source).r;
                if (player == LIGHT ? delta <= 0 : delta >= 0) damage *= 2;
            }
            break;

        // Apollo deals +1 damage on direct attacks.
        case APOLLO:
            {
                field_t source = state.fi(player, god);
                auto [r1, c1] = FieldCoords(source);
                auto [r2, c2] = FieldCoords(target);
                int dr = r2 - r1;
                int dc = c2 - c1;
                if (dr == 0 || dc == 0 || abs(dr) == abs(dc)) ++damage;
            }
            break;

        default:
            // No special handling
            break;
    }

    // Hephaestus damage boost
    if (state.fx(player, god) & DAMAGE_BOOST) ++damage;

    return damage;
}

static void DamageField(State &state, field_t field, Player opponent, int damage) {
    assert(field != -1 && state.PlayerAt(field) == opponent);

    if (state.fx(opponent, state.GodAt(field)) & SHIELDED) {
        // Athena protects against damage
    } else {
        state.DealDamage(opponent, field, damage);
    }
}

static void DamageArea(State &state, Area area, Player opponent, int damage, int knock_dir) {
    // First: damage enemy Athena if she's in range, since if Athena dies, she
    // cannot protect anyone else this turn.
    field_t athena_field = state.fi(opponent, ATHENA);
    if (athena_field != -1 && area.Contains(athena_field)) {
        DamageField(state, athena_field, opponent, damage);
    }

    // Second pass: damage everyone in range except enemy Athena.
    for (int r = area.r1; r <= area.r2; ++r) {
        for (int c = area.c1; c <= area.c2; ++c) {
            if (field_t field = FieldIndex(r, c); field != -1 &&
                    field != athena_field && state.PlayerAt(field) == opponent) {
                DamageField(state, field, opponent, damage);
            }
        }
    }

    if (knock_dir != 0) {
        // Apply knock back (Poseidon's special ability)
        //
        // In the current interpretation, all attacked enemies (including those
        // protected from damage) are pushed back, unless they are at the edge
        // of the board, or there is a non-pushed-back piece behind them (which
        // may be either a friend, or a foe outside the attack range). Note that
        // means that if there is a friendly piece between Poseidon and an enemy,
        // the enemy is still knocked back.
        for (int r = knock_dir > 0 ? area.r2 : area.r1;
                (knock_dir > 0 ? r >= area.r1 : r <= area.r2);
                (knock_dir > 0 ? --r : ++r)) {
            for (int c = area.c1; c <= area.c2; ++c) {
                if (int field = FieldIndex(r, c); field != -1 && state.PlayerAt(field) == opponent) {
                    if (int behind = FieldIndex(r + knock_dir, c); behind != -1 && !state.IsOccupied(behind)) {
                        state.Move(opponent, state.GodAt(field), behind);
                    }
                }
            }
        }
    }
}

void ExecuteAction(State &state, const Action &action) {
    // TODO? check game is not over yet? (should update movegen to match)
    Player player   = state.NextPlayer();
    Player opponent = Other(player);
    switch (action.type) {
        case Action::SUMMON:
            assert(action.field == gate_index[player]);
            state.Place(player, action.god, action.field);
            // TODO: special case heros (Ares, Hades, Athena, etc.)
            break;

        case Action::MOVE:
            state.Move(player, action.god, action.field);
            // TODO: special case heros (Ares, Hades, ...)
            break;

        case Action::ATTACK:
            {
                int damage = GetDamage(state, player, action.god, action.field);
                if (pantheon[action.god].atk_dirs.empty()) {
                    DamageArea(
                            state, Area::Get(player, action.god, action.field),
                            opponent, damage,
                            action.god == POSEIDON ? (player == LIGHT ? +1 : -1) : 0);
                } else {
                    DamageField(state, action.field, opponent, damage);
                }
            }
            break;

        case Action::SPECIAL:
            // TODO!
            break;

        default:
            assert(false);
    }
}

void ExecuteTurn(State &state, const Turn &turn) {
    assert(!state.IsOver());
    for (int i = 0; i < turn.naction; ++i) {
        ExecuteAction(state, turn.actions[i]);
    }
    state.EndTurn();
}

std::string Action::ToString() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

std::string Turn::ToString() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

std::optional<Action> Action::FromString(std::string s) {
    std::istringstream iss(std::move(s));
    Action action;
    if (iss >> action) return action;
    return {};
}

std::optional<Turn> Turn::FromString(std::string s) {
    std::istringstream iss(std::move(s));
    Turn turn;
    if (iss >> turn) return turn;
    return {};
}

constexpr std::string_view type_chars = "@>!+";

std::ostream &operator<<(std::ostream &os, const Action &a) {
    return os << pantheon[a.god].ascii_id << type_chars[a.type] << field_names[a.field];
}

std::ostream &operator<<(std::ostream &os, const Turn &t) {
    if (t.naction == 0) return os << "x";
    os << t.actions[0];
    for (int i = 1; i < t.naction; ++i) os << ',' << t.actions[i];
    return os;
}

std::istream &operator>>(std::istream &is, Action &a) {
    char god_ch, type_ch, col_ch, row_ch;
    if (is >> god_ch >> type_ch >> col_ch >> row_ch) {
        God god;
        std::string_view::size_type type;
        field_t field;
        if ( (god = GodById(god_ch)) < GOD_COUNT &&
             (type = type_chars.find(type_ch)) != std::string_view::npos &&
             '1' <= row_ch && row_ch <= '9' &&
             'a' <= col_ch && col_ch <= 'i' &&
             (field = field_index_by_coords[row_ch - '1'][col_ch - 'a']) != -1 ) {
            a = Action{
                .type  = (Action::Type) type,
                .god   = (God) god,
                .field = field,
            };
        } else {
            is.setstate(std::ios::failbit);
        }
    }
    return is;
}

std::istream &operator>>(std::istream &is, Turn &turn) {
    turn.naction = 0;
    if (is.peek() == 'x') {
        is.get();
    } else {
        Action a;
        for (;;) {
            if (!(is >> a)) break;
            turn.actions[turn.naction++] = a;
            if (is.peek() != ',') break;
            if (turn.naction == Turn::MAX_ACTION) {
                // Too many actions!
                is.setstate(std::ios::failbit);
                break;
            }
            is.get();
        }
    }
    return is;
}
