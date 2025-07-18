#ifndef MOVES_H_INCLUDED
#define MOVES_H_INCLUDED

#include "state.h"

#include <algorithm>
#include <ostream>
#include <vector>

// Encodes a single action, which is either moving, attacking or summoning.
//
//              field       move_to     attack_at
//  Move        source  destination             -
//  Attack      source            -        target
//  Summon      gate              -             -
//
// (For heroes like Poseidon the target may be a pseudo-target that is
// identicial to the source, but it must be set to identify this action as
// an attack.)
//
// The special field is an extra ability dependent on the god.
//
//  - Hermes: second target
//  - Hades: target chained
//
struct Action {
    God god;
    field_t field;          // source field, or gate when summoning
    field_t move_to;        // -1 if attacking/summoning
    field_t attack_at;      // -1 if moving/summoning
    field_t special;        // -1 if not applicable

    bool IsMove() const { return move_to != -1; }
    bool IsAttack() const { return attack_at != -1; }
    bool IsSummon() const { return move_to == -1 && attack_at == -1; }

    auto operator<=>(const Action&) const = default;
};

// The `Turn` structure encodes all actions that can happen during a player's
// turn. It's possible to have a sequence of up to three actions, with the
// following combinations:
//
//   1. - (Pass)
//   2. Move
//   3. Attack
//   4. Summon
//   5. Summon, Move                          (special rule 1)
//   6. Summon, Attack                        (special rule 1)
//   7. Move (from gate), Summon              (special rule 2)
//   8. Move (from gate), Summon, Attack      (special rule 2)
//   9. Attack (kill at gate), Move           (special rule 3)
//  10. Move (kill at gate), Move             (special rule 3)
//
// Note: Move, Move is rare. The only case I can think of is if Ares kills an
// enemy with 1 HP at the enemy's gate by moving next to them.
//
// Note: in theory, the rules permit a sequence of 4 actions (Move, Summon,
// Attack, Move) by combining special rules 2 and 3, but this cannot happen
// in practice, because no newly summoned god has an ability that can kill an
// enemy at the opponent's gate.
//
struct Turn {
    uint8_t naction;
    Action actions[3];

    bool operator==(const Turn &t) const {
        return naction == t.naction && std::equal(actions, actions + naction, t.actions);
    }

    auto operator<=>(const Turn &t) const {
        return std::lexicographical_compare_three_way(
            actions, actions + naction,
            t.actions, t.actions + naction);
    }
};

std::vector<Turn> GenerateTurns(const State &state);

void ExecuteAction(State &state, const Action &action);
void ExecuteTurn(State &state, const Turn &turn);

#endif  // ndef MOVES_H_INCLUDED
