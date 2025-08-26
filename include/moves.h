#ifndef MOVES_H_INCLUDED
#define MOVES_H_INCLUDED

#include "state.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
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
    enum Type { SUMMON, MOVE, ATTACK, SPECIAL } type;
    God god;
    field_t field;

    static std::optional<Action> FromString(std::string s);
    std::string ToString() const;

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
// Note: Move, Move is rare; the only case I can think of is if Ares kills an
// enemy with 1 HP at the enemy's gate by moving next to them.
//
// Each of the primary actions (move, attack, summon) can be followed by a
// special action (e.g., Hades can chain one enemy after moving or attacking).
// That's why the maximum number of actions in a turn is 6 instead of 3.
//
// In theory, the rules permit a sequence of 4 actions (Move, Summon, Attack,
// Move) by combining special rules 2 and 3. However, we cannot achieve this
// with a regular attack, though it is possible if the attack is replaced with
// Artemis special ability (Withering Moon). This only leads to a 6 action
// sequence though (see moves_test.cc for details).
struct Turn {
    static constexpr int MAX_ACTION = 6;

    uint8_t naction;
    Action actions[MAX_ACTION];

    static std::optional<Turn> FromString(std::string s);
    std::string ToString() const;

    bool operator==(const Turn &t) const {
        return naction == t.naction && std::equal(actions, actions + naction, t.actions);
    }

    auto operator<=>(const Turn &t) const {
        return std::lexicographical_compare_three_way(
            actions, actions + naction,
            t.actions, t.actions + t.naction);
    }
};

std::vector<Turn> GenerateTurns(const State &state);

void ExecuteAction(State &state, const Action &action);
void ExecuteActions(State &state, const Turn &turn);
void ExecuteTurn(State &state, const Turn &turn);

std::ostream &operator<<(std::ostream &os, const Action &a);
std::ostream &operator<<(std::ostream &os, const Turn &t);

std::istream &operator>>(std::istream &os, Action &a);
std::istream &operator>>(std::istream &os, Turn &t);

#endif  // ndef MOVES_H_INCLUDED
