// This file contains logic to generate and execute valid turns.
//
// A turn is a sequence of actions that a single player may take before
// passing the turn to the other player.

#include "moves.h"

#include <sstream>

namespace {

constexpr int hermes_speed_boost     = 1;
constexpr int ares_special_dmg       = 1;
constexpr int ares_special_rng       = 1;
constexpr int artemis_horizontal_rng = 7;
constexpr int artemis_special_dmg    = 1;

// Helper class to collect the list of valid turns. Each turn consists of a
// sequence of actions, which is generated recursively. This class helps
// maintain the intermediate sequence of actions and the corresponding state
// after applying those actions to the initial state.
class TurnBuilder {
public:
    TurnBuilder(std::vector<Turn> &turns, State initial_state) : turns(turns) {
        turn.naction = 0;
        states[0] = std::move(initial_state);
        nstate = 1;
    }

    // Not copyable.
    TurnBuilder(const TurnBuilder&) = delete;
    TurnBuilder& operator=(const TurnBuilder&) = delete;

    ~TurnBuilder() {
        assert(turn.naction == 0);
        assert(nstate == 1);
    }

    void PushAction(Action action) {
        assert(turn.naction < Turn::MAX_ACTION);
        turn.actions[turn.naction++] = std::move(action);
    }

    void PopAction() {
        assert(turn.naction > 0);
        --turn.naction;
        if (nstate > turn.naction + 1) --nstate;
    }

    void PopActions(size_t n) {
        while (n --> 0) PopAction();
    }

    void AddTurn() {
        turns.push_back(turn);
    }

    const State &StateByIndex(int index) {
        assert(0 <= index && index <= turn.naction);
        while (nstate <= index) {
            states[nstate] = states[nstate - 1];
            ExecuteAction(states[nstate], turn.actions[nstate - 1]);
            ++nstate;
        }
        return states[index];
    }

    const State &PreviousState() {
        assert(turn.naction > 0);
        return StateByIndex(turn.naction - 1);
    }

    const State &CurrentState() {
        return StateByIndex(turn.naction);
    };

    // Helper class that pushes an Action when created, and pops it when
    // destroyed. This allows creating lexically scoped action without having
    // to manually balancing the push and pop operations.
    class Scoped {
    public:
        Scoped(TurnBuilder &builder, Action action, bool add_immediately=true) : builder(builder) {
            builder.PushAction(action);
            if (add_immediately) builder.AddTurn();
        }

        ~Scoped() {
            builder.PopAction();
        }

    private:
        TurnBuilder &builder;
    };

    Scoped MakeScoped(Action action, bool add_immediately=true) {
        return Scoped(*this, action, add_immediately);
    }

private:
    std::vector<Turn> &turns;

    Turn turn;

    // states[n] is the state obtained after `n` actions. These states are
    // computed lazily, since we don't always need to generate the state to
    // determine a turn is valid.
    //
    // Invariant: nstates <= turn.naction + 1
    //
    // Important: the reference returned by CurrentState() MUST NOT be
    // invalidated by mutating calls to PushAction(), PopAction() or AddTurn()
    // (unless the active state itself is popped), which is why we use a fixed
    // size array here, instead of e.g. std::vector<>.
    std::array<State, Turn::MAX_ACTION + 1> states;
    size_t nstate = 0;
};

// A rectangular area to be attacked. Includes all fields with coords (r, c)
// where r1 <= r <= r2 and c1 <= c <= c2 (i.e., all endpoints are inclusive).
//
// The coordinates are clamped to the bounding box of the board
// (0..BOARD_SIZE-1) but not necessarily all of the included coordinates
// correspond to existing fields.
struct Area {
    int r1, c1, r2, c2;

    Area() : r1(0), c1(0), r2(-1), c2(-1) {}

    Area(int r1, int c1, int r2, int c2) :
        r1(std::max(r1, 0)),
        c1(std::max(c1, 0)),
        r2(std::min(r2, BOARD_SIZE - 1)),
        c2(std::min(c2, BOARD_SIZE - 1)) {}

    bool Empty() const { return r1 > r2 || c1 > c2; }

    static Area around(field_t field, int range) {
        auto [r, c] = FieldCoords(field);
        return Area(r - range, c - range, r + range, c + range);
    }

    static Area around(int r, int c, int range) {
        return Area(r - range, c - range, r + range, c + range);
    }

    bool Contains(field_t field) const {
        auto [r, c] = FieldCoords(field);
        return Contains(r, c);
    }

    bool Contains(int r, int c) const {
        return r1 <= r && r <= r2 && c1 <= c && c <= c2;
    }

    static Area Get(Player player, God god, field_t field) {
        auto [r, c] = FieldCoords(field);
        switch (god) {
            case POSEIDON:
                return
                    (player == LIGHT)
                        ? Area(r + 1, c - 1, r + 2, c + 1)
                        : Area(r - 2, c - 1, r - 1, c + 1);

            case DIONYSUS:
                return
                    (player == LIGHT)
                        ? Area(r + 1, 0, r + 1, BOARD_SIZE - 1)
                        : Area(r - 1, 0, r - 1, BOARD_SIZE - 1);

            case HADES:
                return Area::around(r, c, pantheon[god].rng);

            default:
                assert(false);
        }
        return Area();  // unreachable
    }
};

void GenerateSpecialsHades(
    TurnBuilder &builder, bool hades_may_move_after,
    bool hades_may_attack_after, bool may_summon_after);

void GenerateSpecialsAres(TurnBuilder &builder, Player player, field_t field);

void GenerateSummons(TurnBuilder &builder, bool may_move_after);

void GenerateMovesOne(TurnBuilder &builder, field_t field, bool may_summon_after) {
    const State &state = builder.CurrentState();
    const Player player = state.NextPlayer();
    const God god = state.GodAt(field);
    assert(god < GOD_COUNT);

    // Cannot move when chained by Hades.
    if (state.has_fx(player, god, CHAINED)) return;

    const int speed_boost = state.has_fx(player, god, SPEED_BOOST) ? hermes_speed_boost : 0;
    const int max_dist = pantheon[god].mov + speed_boost;
    std::span<const Dir> dirs = GetDirs(pantheon[god].mov_dirs);

    auto add_move_action = [&](field_t field) {
        auto scoped_action = builder.MakeScoped(Action{
            .type  = Action::MOVE,
            .god   = god,
            .field = field,
        });
        if (may_summon_after) {
            GenerateSummons(builder, false);
        }
        if (god == ARES) {
            GenerateSpecialsAres(builder, player, field);
        }
        if (god == HADES) {
            GenerateSpecialsHades(builder, false, false, may_summon_after);
        }
    };

    // The logic below is similar to GenerateAttacksOne(), defined below.
    // Try to keep the two in sync.

    if (pantheon[god].mov_dirs & Dirs::DIRECT) {
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
                add_move_action(i);
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
                    add_move_action(i);
                    seen[i] = true;
                    todo[end++] = Coords{rr, cc};
                }
            }
        }

        // Special case: Dionysus can jump on enemies to eliminate them.
        // This covers all cases where there is at least 1 elimination.
        if (god == DIONYSUS) {
            assert(max_dist == 1 || max_dist == 2);
            auto accessible = [&](field_t field) {
                if (field == -1) return false;
                if (!state.IsOccupied(field)) return true;
                int p = state.PlayerAt(field);
                if (p == player) return false;  // cannot jump on ally
                if (state.has_fx(AsPlayer(p), AsGod(state.GodAt(field)), SHIELDED)) return false;
                return true;
            };
            auto [r0, c0] = FieldCoords(field);
            for (auto [dr, dc] : dirs) {
                int8_t r1 = r0 + dr;
                int8_t c1 = c0 + dc;
                field_t field1 = FieldIndex(r1, c1);
                if (!accessible(field1)) continue;
                bool special1 = state.IsOccupied(field1);
                if (special1) {
                    builder.PushAction(Action{
                        .type  = special1 ? Action::SPECIAL : Action::MOVE,
                        .god   = god,
                        .field = field1,
                    });
                    builder.AddTurn();
                }
                if (max_dist == 2) {
                    for (auto [dr, dc] : dirs) {
                        int8_t r2 = r1 + dr;
                        int8_t c2 = c1 + dc;
                        field_t field2 = FieldIndex(r2, c2);
                        if (!accessible(field2)) continue;

                        if (!special1) {
                            // Deduplicate only if we didn't hit anything on the
                            // first move, because turns will be equivalent:
                            if (seen[field2]) continue;
                            seen[field2] = true;
                        }

                        bool special2 = state.IsOccupied(field2);
                        auto scoped_action2 = builder.MakeScoped(Action{
                            .type  = special2 ? Action::SPECIAL : Action::MOVE,
                            .god   = god,
                            .field = field2,
                        }, special1 || special2);
                    }
                }
                if (special1) {
                    builder.PopAction();
                }
            }
        }

        // Artemis can move up to 7 sideways (or 8 when boosted by Hermes)
        if (god == ARTEMIS) {
            int horiz_dist = artemis_horizontal_rng + speed_boost;
            auto [r, c] = FieldCoords(field);
            for (int i = 1; i <= horiz_dist; ++i) {
                field_t field = FieldIndex(r, c + i);
                if (field == -1 || state.IsOccupied(field)) break;
                if (i > max_dist) add_move_action(field);
            }
            for (int i = 1; i <= horiz_dist; ++i) {
                field_t field = FieldIndex(r, c - i);
                if (field == -1 || state.IsOccupied(field)) break;
                if (i > max_dist) add_move_action(field);
            }
        }
    }
}

void GenerateMovesAll(TurnBuilder &builder, bool may_summon_after) {
    const State &state = builder.CurrentState();
    const Player player = state.NextPlayer();
    const field_t gate = gate_index[player];
    for (field_t field = 0; field < FIELD_COUNT; ++field) {
        if (state.PlayerAt(field) == player) {
            GenerateMovesOne(builder, field, field == gate && may_summon_after);
        }
    }
}

// Determines damage at an area killed an enemy at the opponent's gate,
// which would enable a second move.
//
// The implementation is slightly convoluted in the interest of performance:
// it tries not to evaluate PreviousState() and CurrentState() when it can
// be determined no enemy was killed.
bool KilledEnemyAtGate(TurnBuilder &builder, const Area &damage_area, Player opponent) {
    field_t opponent_gate = gate_index[opponent];
    if (!damage_area.Contains(opponent_gate)) return false;
    const State &prev_state = builder.PreviousState();
    if (prev_state.PlayerAt(opponent_gate) != opponent) return false;
    God enemy = AsGod(prev_state.GodAt(opponent_gate));
    const State &next_state = builder.CurrentState();
    return next_state.IsDead(opponent, enemy);
}

void GenerateAttacksOne(TurnBuilder &builder, field_t field) {
    const State &state = builder.CurrentState();
    const Player player = state.NextPlayer();
    const Player opponent = Other(player);
    const God god = state.GodAt(field);
    assert(god < GOD_COUNT);

    // Cannot attack when chained by Hades.
    if (state.has_fx(player, god, CHAINED)) return;

    int max_dist = pantheon[god].rng;
    std::span<const Dir> dirs = GetDirs(pantheon[god].atk_dirs);

    struct Attack {
        field_t field;
        Area area;

        static Attack AtArea(field_t field, Area area) {
            return Attack{.field = field, .area = area};
        }

        static Attack AtField(field_t field) {
            return Attack{.field = field, .area = Area::around(field, 0)};
        }
    };

    auto add_attack_actions = [&](std::span<const Attack> attacks) {
        bool may_move_after = false;
        for (const Attack &attack : attacks) {
            builder.PushAction(Action{
                .type   = Action::ATTACK,
                .god    = god,
                .field  = attack.field,
            });
            builder.AddTurn();
            may_move_after = may_move_after || KilledEnemyAtGate(builder, attack.area, opponent);
        }

        if (may_move_after) {
            // Special rule 3: when you kill an enemy on the opponent's
            // gate, you get an extra move.
            GenerateMovesAll(builder, false);
        }

        if (god == HADES) {
            GenerateSpecialsHades(builder, may_move_after, false, false);
        }

        builder.PopActions(attacks.size());
    };

    if (dirs.empty()) {
        // Area attacks. Handle specially.
        //
        // To limit the number of moves somewhat, only include attacks if the
        // area contains at least one enemy, even though it still might have no
        // effect when enemies are shielded by Athena.
        Area area = Area::Get(player, god, field);
        for (int r = area.r1; r <= area.r2; ++r) {
            for (int c = area.c1; c <= area.c2; ++c) {
                if (int i = FieldIndex(r, c); i != -1 && state.PlayerAt(i) == opponent) {
                    Attack attacks[1] = {Attack::AtArea(field, area)};
                    add_attack_actions(attacks);
                    goto done;
                }
            }
        }
    done:
        return;
    }

    // The logic below is similar to GenerateMovesOne(), defined above.
    // Try to keep the two in sync.
    if (pantheon[god].atk_dirs & Dirs::DIRECT) {
        // Direct attacks only: scan each direction until we reach the end of
        // the board or an occupied field. (Note that we need more than the
        // obvious 8 fields [one per direction] here, because Zeus' attacks can
        // pass over enemies. GCC seems to think [incorrectly] that we need at
        // least 16 elements, though I think the real limit is GOD_COUNT.)
        field_t field_data[16];
        size_t field_size = 0;
        static_assert(GOD_COUNT <= std::size(field_data));
        assert(std::size(field_data) >= std::size(dirs));

        Coords coords = FieldCoords(field);
        for (auto [dr, dc] : dirs) {
            auto [r, c] = coords;
            for (int dist = 1; dist <= max_dist; ++dist) {
                r += dr;
                c += dc;
                field_t i = FieldIndex(r, c);
                if (i == -1) break;
                int pl = state.PlayerAt(i);
                if (pl == opponent) {
                    assert(field_size < std::size(field_data));
                    field_data[field_size++] = i;
                }
                if (god == ZEUS) {
                    // Zeus' special attack can pass over enemies and allies
                } else {
                    if (pl != -1) break;
                }
            }
        }

        // Execute any single attack:
        std::span fields(field_data, field_size);
        for (field_t field : fields) {
            Attack attacks[1] = {Attack::AtField(field)};
            add_attack_actions(attacks);
        }

        // Hermes can attack two targets in the same turn:
        if (god == HERMES) {
            if (hermes_canonicalize_attacks) {
                // Sort by gods to normalize attacks, and so we can kill Athena first,
                // so she doesn't shield the second god we attack.
                static_assert(ATHENA == GOD_COUNT - 1);
                std::ranges::sort(fields, {}, [&state](field_t f){ return state.GodAt(f); });
                for (size_t i = 1; i < fields.size(); ++i) {
                    for (size_t j = 0; j < i; ++j) {
                        Attack attacks[2] = {
                            Attack::AtField(fields[i]),
                            Attack::AtField(fields[j]),
                        };
                        add_attack_actions(attacks);
                    }
                }
            } else {
                // Generate two attacks in any order.
                for (field_t f : fields) {
                    for (field_t g : fields) {
                        if (f != g) {
                            Attack attacks[2] = {
                                Attack::AtField(f),
                                Attack::AtField(g),
                            };
                            add_attack_actions(attacks);
                        }
                    }
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
                        Attack attacks[1] = {Attack::AtField(i)};
                        add_attack_actions(attacks);
                    }
                }
            }
        }
    }

    // Artemis can use her Withering Moon special ability instead of attacking.
    if (god == ARTEMIS) {
        for (field_t field = 0; field < FIELD_COUNT; ++field) {
            if (state.PlayerAt(field) == opponent) {
                God god = AsGod(state.GodAt(field));
                if (!state.has_fx(opponent, god, SHIELDED)) {
                    TurnBuilder::Scoped action = builder.MakeScoped(Action{
                        .type = Action::SPECIAL,
                        .god = ARTEMIS,
                        .field = field,
                    });
                    if (KilledEnemyAtGate(builder, Area::around(field, 0), opponent)) {
                        // Special rule 3: when you kill an enemy on the opponent's
                        // gate, you get an extra move.
                        GenerateMovesAll(builder, false);
                    }
                }
            }
        }
    }
}

void GenerateAttacksAll(TurnBuilder &builder) {
    const State &state = builder.CurrentState();
    const Player player = state.NextPlayer();
    for (field_t field = 0; field < FIELD_COUNT; ++field) {
        if (state.PlayerAt(field) == player) {
            GenerateAttacksOne(builder, field);
        }
    }
}

// Ares special is to deal damage whenever he lands next to an enemy. Since
// this happens automatically it is NOT modeled as a separate action. However,
// if this ability kills an enemy at the opponent's gate, this should trigger
// an extra move. This function exists solely to generate that extra move.
//
// That's why this should only be called after moves/specials that do not
// themselves trigger a second move on their own.
void GenerateSpecialsAres(TurnBuilder &builder, Player player, field_t field) {
    Area areas = Area::around(field, ares_special_rng);
    if (KilledEnemyAtGate(builder, areas, Other(player))) {
        // Special rule 3: when you kill an enemy on the opponent's
        // gate, you get an extra move.
        GenerateMovesAll(builder, false);
    }
}

void GenerateSpecialsAphrodite(TurnBuilder &builder) {
    const State &state = builder.CurrentState();
    const Player player = state.NextPlayer();
    const field_t src = state.fi(player, APHRODITE);
    if (src == -1) return;  // Aphrodite not on the board

    // Find ally to swap with:
    for (field_t dst = 0; dst < FIELD_COUNT; ++dst) {
        if (state.PlayerAt(dst) == player && src != dst) {
            auto scoped_action = builder.MakeScoped(Action{
                .type  = Action::SPECIAL,
                .god   = APHRODITE,
                .field = dst,
            });
            if (state.GodAt(dst) == ARES) {
                GenerateSpecialsAres(builder, player, src);
            }
            if (state.GodAt(dst) == HADES) {
                GenerateSpecialsHades(builder, false, false, false);
            }
        }
    }
}

// Hades can use his special move (chain adjacent enemy) after:
//
//  - he is summoned (he may move after, and possibly special again)
//  - he moved (when moving from the gate, summoning afterwards is allowed)
//  - he attacked
//  - he was swapped by Aphrodite
//
void GenerateSpecialsHades(
    TurnBuilder &builder, bool hades_may_move_after,
    bool hades_may_attack_after, bool may_summon_after
) {
    const State &state = builder.CurrentState();
    const Player player = state.NextPlayer();
    const Player opponent = Other(player);
    const field_t src = state.fi(player, HADES);
    assert(src != -1);

    // Find enemy to chain:
    for (field_t dst : Neighbors(src)) {
        God enemy = state.GodAt(dst);
        if (enemy != GOD_COUNT && state.PlayerAt(dst) == opponent &&
                !state.has_fx(opponent, enemy, CHAINED)) {
            auto scoped_action = builder.MakeScoped(Action{
                .type  = Action::SPECIAL,
                .god   = HADES,
                .field = dst,
            });
            if (hades_may_move_after) {
                GenerateMovesOne(builder, src, false);
            }
            if (hades_may_attack_after) {
                GenerateAttacksOne(builder, src);
            }
            if (may_summon_after) {
                GenerateSummons(builder, false);
            }
        }
    }
}

void GenerateSummons(TurnBuilder &builder, bool may_move_after) {
    const State &state = builder.CurrentState();
    const Player player = state.NextPlayer();
    const field_t gate = gate_index[player];

    if (state.IsOccupied(gate)) return;

    for (int g = 0; g != GOD_COUNT; ++g) {
        if (state.IsSummonable(player, (God) g)) {
            auto scoped_action = builder.MakeScoped({
                .type  = Action::SUMMON,
                .god   = AsGod(g),
                .field = gate,
            });

            if (may_move_after) {
                GenerateMovesOne(builder, gate, false);
            }

            GenerateAttacksOne(builder, gate);

            switch (g) {
                case APHRODITE:
                    GenerateSpecialsAphrodite(builder);
                    break;

                case HADES:
                    GenerateSpecialsHades(builder, may_move_after, true, false);
                    break;
            }
        }
    }
}

int GetDamage(const State &state, Player player, God god, field_t target) {
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
    if (state.has_fx(player, god, DAMAGE_BOOST)) ++damage;

    return damage;
}

void DamageField(State &state, field_t field, Player opponent, int damage) {
    assert(field != -1 && state.PlayerAt(field) == opponent);

    God god = state.GodAt(field);
    assert(god != GOD_COUNT);
    if (state.has_fx(opponent, god, SHIELDED)) {
        // Athena protects against damage
    } else {
        state.DealDamage(opponent, god, damage);
    }
}

void DamageArea(State &state, Area area, Player opponent, int damage, int knock_dir) {
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

void AresLand(State &state, field_t field) {
    assert(state.GodAt(field) == ARES);
    Player opponent = Other(AsPlayer(state.PlayerAt(field)));
    Area area = Area::around(field, ares_special_rng);
    DamageArea(state, area, opponent, ares_special_dmg, 0);
}

// Executes Artemis' special ability: deal damage to an enemy and take
// an equal amount of damage herself.
void ArtemisDamage(State &state, Player player, field_t target) {
    int damage = artemis_special_dmg;
    if (state.has_fx(player, ARTEMIS, DAMAGE_BOOST)) ++damage;
    Player opponent = AsPlayer(state.PlayerAt(target));
    God god = AsGod(state.GodAt(target));
    state.DealDamage(player, ARTEMIS, damage);
    state.DealDamage(opponent, god, damage);
}

// Executes Aphrodite's special ability: swapping with a friendly god
// anywhere on the board.
//
// This is carefully designed to work correctly with Hades chain effect:
//
//  - When Aphrodite swaps with an ally, both are freed of enemy Hades.
//
//  - When Aphrodite swaps with Hades, enemies that were chained are
//    kept in chains only if they remain adjacent; this is handled already
//    by State::Move().
//
//    (Note that Hades may chain a new enemy in that case, which is
//    encoded with a separate special ability, so not handled here.)
//
//  - When Aphrodite swaps with a friendly Ares, his special deals
//    damage to adjacent enemies.
//
void AphroditeSwap(State &state, field_t src, field_t dst) {
    assert(src != -1 && dst != -1 && src != dst);
    assert(state.GodAt(src) == APHRODITE);
    God ally = state.GodAt(dst);
    assert(ally != GOD_COUNT);

    Player player = state.NextPlayer();
    assert(state.PlayerAt(src) == player && state.PlayerAt(dst) == player);

    state.Remove(player, APHRODITE);
    state.Move(player, ally, src);
    state.Place(player, APHRODITE, dst);

    if (ally == ARES) {
        AresLand(state, src);
    }
}

}  // namespace

std::vector<Turn> GenerateTurns(const State &state) {
    std::vector<Turn> turns;
    TurnBuilder builder(turns, state);
    GenerateSummons(builder, true);
    GenerateMovesAll(builder, true);
    GenerateAttacksAll(builder);
    GenerateSpecialsAphrodite(builder);
    if (turns.empty()) {
        // Is passing always allowed?
        turns.push_back(Turn{.naction=0, .actions={}});
    }
    return turns;
}

// Executes an action in the given state.
//
// This does NOT verify that the action is valid; the caller must ensure this!
void ExecuteAction(State &state, const Action &action) {
    // TODO? check game is not over yet? (should update movegen to match)
    Player player   = state.NextPlayer();
    Player opponent = Other(player);
    switch (action.type) {
        case Action::SUMMON:
            assert(action.field == gate_index[player]);
            state.Summon(action.god);  // updates `summonable` bitmask
            if (action.god == ARES) {
                AresLand(state, action.field);
            }
            break;

        case Action::MOVE:
            state.Move(player, action.god, action.field);
            if (action.god == ARES) {
                AresLand(state, action.field);
            }
            break;

        case Action::ATTACK:
            {
                int damage = GetDamage(state, player, action.god, action.field);
                if (pantheon[action.god].atk_dirs == Dirs::NONE) {
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
            switch (action.god) {
                case APHRODITE:
                    AphroditeSwap(state, state.fi(player, APHRODITE), action.field);
                    break;

                case DIONYSUS:
                    state.Kill(opponent, state.GodAt(action.field));
                    state.Move(player, DIONYSUS, action.field);
                    break;

                case ARTEMIS:
                    ArtemisDamage(state, player, action.field);
                    break;

                case HADES:
                    state.ChainAt(action.field);
                    break;

                default:
                    assert(false);
            }
            break;

        default:
            assert(false);
    }
}

// Executes all actions.
//
// This does NOT verify that the turn is valid; the caller must ensure this!
// The easiest way is to make sure the turn is included in the result of
// GenerateTurns() when called on the same state.
//
void ExecuteActions(State &state, const Turn &turn) {
    assert(!state.IsOver());
    for (int i = 0; i < turn.naction; ++i) {
        ExecuteAction(state, turn.actions[i]);
    }
}

// Executes a turn by executing all actions and then switching players.
//
// This does NOT verify that the turn is valid; the caller must ensure this!
// The easiest way is to make sure the turn is included in the result of
// GenerateTurns() when called on the same state.
//
void ExecuteTurn(State &state, const Turn &turn) {
    ExecuteActions(state, turn);
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
    if ((iss >> action) && iss.peek() == EOF) return action;
    return {};
}

std::optional<Turn> Turn::FromString(std::string s) {
    std::istringstream iss(std::move(s));
    Turn turn;
    if ((iss >> turn) && iss.peek() == EOF) return turn;
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
