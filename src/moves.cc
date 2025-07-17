#include "moves.h"

#include <stdio.h>  // TEMP

namespace {

void GenerateSummons(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    bool may_move_after);

void GenerateMovesOne(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    field_t field, bool may_summon_after
) {
    God god = state.GodAt(field);
    assert(god < GOD_COUNT);

    // TODO: support Hermes movement boost! (make sure Artemis, Dionysus work correctly)
    int max_dist = pantheon[god].mov;
    std::span<const Dir> dirs = pantheon[god].mov_dirs;

    auto add_move = [&](field_t move_to) {
        Action action{
            .god       = god,
            .field     = field,
            .move_to   = move_to,
            .attack_at = -1,
            .special   = -1,  // TODO
        };
        turn.actions[turn.naction++] = action;
        turns.push_back(turn);
        if (may_summon_after) {
            State new_state = state;
            ExecuteAction(new_state, action);
            GenerateSummons(state, turns, turn, false);
        }
        --turn.naction;
    };

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
                add_move(i);
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
                    add_move(i);
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
    // TODO: if moving from the gate, allow following up with summons.
}

void GenerateAttacksOne(
    const State &state, std::vector<Turn> &turns, Turn &turn,
    field_t field
) {
    // TODO
    // TODO: rule 3: can move again if killing an enemy at the final gate.
}

void GenerateAttacksAll(
    const State &state, std::vector<Turn> &turns, Turn &turn
) {
    const Player player = state.NextPlayer();
    const field_t opponent_gate = gate_index[Other(player)];
    for (field_t i = 0; i < FIELD_COUNT; ++i) {
        if (state.PlayerAt(i) == player) {
            GenerateAttacksOne(state, turns, turn, i);
            // TODO: generate attacks
            // TODO: allow additional move if killing enemy at opponent's gate
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
        int field = state.fi(player, (God) g);
        if (field == -1) {
            Action action{
                .god       = (God) g,
                .field     = gate,
                .move_to   = -1,
                .attack_at = -1,
                .special   = -1,
            };
            turn.actions[turn.naction++] = action;
            turns.push_back(turn);
            State new_state = state;
            ExecuteAction(new_state, action);

            if (may_move_after) {
                GenerateMovesOne(new_state, turns, turn, gate, false);
            }

            // TODO: summon + attack

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

// TODO:
//   1. - (Pass)
//   2. Move
//   3. Attack
//   4. Summon
//   5. Summon, Move                (special rule 1)
//   6. Summon, Attack              (special rule 1)
//   7. Move, Summon                (special rule 2)
//   8. Move, Summon, Attack        (special rule 2)
//   9. Attack, Move                (special rule 3)
//  10. Move, Move                  (special rule 3)

static void DamageAt(State &state, field_t field, Player opponent, int damage) {
    if (state.PlayerAt(field) != opponent) return;
    state.DealDamage(field, damage);
}

void ExecuteAction(State &state, const Action &action) {
    // TODO? check game is not over yet? (should update movegen to match)
    Player player   = state.NextPlayer();
    Player opponent = Other(player);
    if (action.IsSummon()) {
        field_t gate = gate_index[player];
        assert(gate == action.field);
        assert(state.IsEmpty(gate));
        assert(state.IsSummonable(player, action.god));
        state.Place(player, action.god, gate);
        // TODO: special case heros
    } else if (action.IsMove()) {
        assert(state.GodAt(action.field) == action.god);
        state.Move(player, action.god, action.move_to);
        // TODO: special case heros
    } else if (action.IsAttack()) {
        assert(state.GodAt(action.field) == action.god);
        DamageAt(state, action.attack_at, opponent, pantheon[action.god].dmg);
        // TODO: respect invulnerability bits!
        // TODO: special case heros
    }
}

void ExecuteTurn(State &state, const Turn &turn) {
    assert(!state.IsOver());
    for (int i = 0; i < turn.naction; ++i) {
        ExecuteAction(state, turn.actions[i]);
    }
    state.EndTurn();
}
