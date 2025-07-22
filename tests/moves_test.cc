#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

#include "state.h"
#include "moves.h"

#include <iostream>
#include <cassert>
#include <cctype>
#include <string_view>
#include <vector>

std::ostream& operator<<(std::ostream &os, const std::vector<field_t> &v) {
    os << "{";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) os << ", ";
        os << static_cast<int>(v[i]);
    }
    os << "}";
    return os;
}

std::ostream& operator<<(std::ostream &os, const std::vector<God> &v) {
    os << "{";
    for (size_t i = 0; i < v.size(); ++i) {
        God god = v[i];
        if (i > 0) os << ", ";
        os << (god == GOD_COUNT ? "N/A" : pantheon[god].name);
    }
    os << "}";
    return os;
}

namespace {

std::vector<std::string> TurnStrings(const State &state) {
    std::vector<std::string> res;
    for (const Turn &turn : GenerateTurns(state)) {
        res.push_back(turn.ToString());
    }
    return res;
}

std::optional<Turn> FindTurn(const State &state, std::string_view sv) {
    for (const Turn &turn : GenerateTurns(state)) {
        if (turn.ToString() == sv) return turn;
    }
    return {};
}

void ExecuteTurn(State &state, std::string_view sv) {
    ASSERT_THAT(TurnStrings(state), Contains(sv));
    auto turn = FindTurn(state, sv);
    ASSERT_TRUE(turn);
    ExecuteTurn(state, *turn);
}

std::vector<field_t> MoveDestinations(const State &state, God god) {
    std::vector<field_t> res;
    for (const Turn &turn : GenerateTurns(state)) {
        for (int i = 0; i < turn.naction; ++i) {
            const Action &action = turn.actions[i];
            if (action.type == Action::MOVE && action.god == god) {
                res.push_back(action.field);
            }
        }
    }
    return res;
}

std::vector<God> AttackTargets(const State &state, God god) {
    std::vector<God> res;
    for (const Turn &turn : GenerateTurns(state)) {
        for (int i = 0; i < turn.naction; ++i) {
            const Action &action = turn.actions[i];
            if (action.type == Action::ATTACK && action.god == god) {
                res.push_back(state.GodAt(action.field));
            }
        }
    }
    return res;
}

field_t Flip(field_t f) {
    auto [r, c] = FieldCoords(f);
    return FieldIndex(BOARD_SIZE - 1 - r, c);
}

struct BoardTemplate {
    std::array<char, FIELD_COUNT> data;

    BoardTemplate(std::string_view sv);
    BoardTemplate(const char *p) : BoardTemplate(std::string_view(p)) {}

    char operator[](size_t i) const { return data[i]; }

    State ToState(Player player) const;
};

BoardTemplate::BoardTemplate(std::string_view sv) {
    size_t j = 0;
    for (char ch : sv) {
        if (isspace(ch)) continue;
        assert(j < FIELD_COUNT);
        data[Flip(j++)] = ch;
    }
    assert(j == FIELD_COUNT);
}

/* Example usage (for easy copy/pasting):

    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

Note only gods present on the template are given hitpoints, so other gods cannot
be summoned! (Maybe I need an additional parameter here to allow that.)
*/
State BoardTemplate::ToState(Player player) const {
    struct Place {
        Player player;
        God god;
        field_t field;
    };
    std::vector<Place> places;
    for (field_t field = 0; field < FIELD_COUNT; ++field) {
        char ch = data[field];
        if (isupper(ch)) {
            God god = GodById(ch);
            assert(god < GOD_COUNT);
            places.push_back({LIGHT, god, field});
        } else if (islower(ch)) {
            God god = GodById(toupper(ch));
            assert(god < GOD_COUNT);
            places.push_back({DARK, god, field});
        }
    }

    bool init_gods[2][GOD_COUNT] = {};
    for (auto [player, god, field] : places) {
        assert(init_gods[player][god] == false);  // duplicate god on board
        init_gods[player][god] = true;
    }

    State state = State::InitialWithGods(init_gods);
    for (auto [player, god, field] : places) {
        state.Place(player, god, field);
    }
    if (player != state.NextPlayer()) state.EndTurn();
    return state;
}

void TestMovement(Player player, God god, const BoardTemplate &t) {
    State state = t.ToState(player);

    std::vector<field_t> expected;
    for (field_t field = 0; field < FIELD_COUNT; ++field) {
        if (t[field] == '+') expected.push_back(field);
    }

    std::vector<field_t> received = MoveDestinations(state, god);
    std::ranges::sort(received);

    EXPECT_EQ(expected, received);
}

void TestAttack(Player player, God god, std::vector<God> expected, const BoardTemplate &t) {
    std::vector<God> received = AttackTargets(t.ToState(player), god);
    std::ranges::sort(expected);
    std::ranges::sort(received);
    EXPECT_THAT(received, UnorderedElementsAreArray(expected))
        << "expected: " << expected << '\n'
        << "received: " << received;
}

}  // namespace

TEST(SpecialRule1, Basic) {
    // On the turn that a character is “summoned” onto your Gates Space,
    // that character can also move or attack on the same turn.

    State state = State::Initial();
    state.Place(DARK, ZEUS, ParseField("e2"));

    auto turns = TurnStrings(state);

    EXPECT_THAT(turns, Contains("N@e1,N!e2"));
    EXPECT_THAT(turns, Contains("N@e1,N!e2"));
}

TEST(SpecialRule2, Basic) {
    // If your home gate is occupied, you may move a piece and summon a god afterward,
    // who may then attack but not move, but may use a special ability.

    State state = State::Initial();
    state.Place(DARK, ZEUS, ParseField("e2"));
    state.Place(LIGHT, ZEUS, ParseField("e1"));

    auto turns = TurnStrings(state);

    EXPECT_THAT(turns,     Contains("Z>d2,N@e1,N!e2") );
    EXPECT_THAT(turns, Not(Contains("Z>d2,N@e1,N>f2")));

    // TODO: check Aphrodite can still swap
    // TODO: check Hades can still bind up to twice
}

TEST(SpecialRule3, Basic) {
    // If you kill an enemy on their Gates Space, you may move any of your
    // characters that are already on the board immediately.

    State state = BoardTemplate(
            "     z     "
            "    ...    "
            "   E....   "
            "  ...Z...  "
            " ......... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);

    auto turns = TurnStrings(state);
    EXPECT_THAT(turns, Contains("Z!e9,Z>e5"));  // doesn't have to move to goal
    EXPECT_THAT(turns, Contains("Z!e9,E>e9"));  // doesn't have to be the same God
}

TEST(StatusEffects, Basic) {
    // 3  . . . P .
    // 2    Z z .
    // 1      N
    //    c d e f g

    God N = ATHENA;
    God Z = ZEUS;
    God P = POSEIDON;

    // When summoning a god, its aura gets applied to neighboring allies:
    State state = State::Initial();
    state.Place(LIGHT, Z, ParseField("d2"));
    state.Place(DARK,  Z, ParseField("e2"));
    state.Place(LIGHT, P, ParseField("f3"));
    state.Place(LIGHT, N, ParseField("e1"));
    EXPECT_EQ(state.fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, Z), SHIELDED);
    EXPECT_EQ(state.fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(state.fx(DARK,  Z), UNAFFECTED);

    // Ally moving away removes status:
    state.Move(LIGHT, Z, ParseField("c3"));
    EXPECT_EQ(state.fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, Z), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(state.fx(DARK,  Z), UNAFFECTED);

    // Ally moving towards adds status:
    state.Move(LIGHT, P, ParseField("f2"));
    EXPECT_EQ((int)state.fx(LIGHT, N), (int)UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, Z), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, P), SHIELDED);
    EXPECT_EQ(state.fx(DARK,  Z), UNAFFECTED);

    // Moving towards ally adds status, way from ally removes status:
    state.Move(LIGHT, N, ParseField("d2"));
    EXPECT_EQ(state.fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, Z), SHIELDED);
    EXPECT_EQ(state.fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(state.fx(DARK,  Z), UNAFFECTED);

    // Removing god removes status.
    state.Remove(LIGHT, N);
    EXPECT_EQ(state.fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, Z), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(state.fx(DARK,  Z), UNAFFECTED);
}

TEST(StatusEffects, Overlap) {
    State state = State::Initial();
    state.Place(LIGHT, ZEUS,       ParseField("e1"));
    state.Place(LIGHT, HEPHAESTUS, ParseField("d2"));
    state.Place(LIGHT, ATHENA,     ParseField("e2"));
    state.Place(LIGHT, HERMES,     ParseField("f2"));

    EXPECT_EQ(state.fx(LIGHT, ZEUS),        DAMAGE_BOOST | SPEED_BOOST | SHIELDED);
    EXPECT_EQ(state.fx(LIGHT, HEPHAESTUS),  SHIELDED);
    EXPECT_EQ(state.fx(LIGHT, ATHENA),      DAMAGE_BOOST | SPEED_BOOST);
    EXPECT_EQ(state.fx(LIGHT, HERMES),      SHIELDED);

    state.Remove(LIGHT, ATHENA);

    EXPECT_EQ(state.fx(LIGHT, ZEUS),       DAMAGE_BOOST | SPEED_BOOST);
    EXPECT_EQ(state.fx(LIGHT, HEPHAESTUS), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, ATHENA),     UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, HERMES),     UNAFFECTED);
}

TEST(Zeus, Moves) {
    TestMovement(LIGHT, ZEUS,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   .....   "
            "    +++    "
            "     Z     "
    );

    TestMovement(LIGHT, ZEUS,
            "     .     "
            "    ...    "
            "   .....   "
            "  ..+++..  "
            " ...+Z+... "
            "  ..+++..  "
            "   .....   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, ZEUS,
            "     .     "
            "    ...    "
            "   .....   "
            "  o......  "
            " Z+....... "
            "  +......  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

TEST(Zeus, Attacks) {
    TestAttack(LIGHT, ZEUS, {APOLLO, POSEIDON},
            "     .     "
            "    ...    "
            "   .....   "
            "  ..do...  "
            " a...Z..p. "
            "  .......  "
            "   ...m.   "
            "    ...    "
            "     .     "
    );
}

// Lighting Bolt: Attack can pass over friendly or enemy pieces
TEST(Zeus, Special) {
    TestAttack(LIGHT, ZEUS, {POSEIDON, APOLLO, DIONYSUS, ATHENA, HERA},
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " adopZAenm "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

TEST(Hephaestus, Moves) {
    TestMovement(LIGHT, HEPHAESTUS,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   ..+..   "
            "    +++    "
            "     H     "
    );

    TestMovement(LIGHT, HEPHAESTUS,
            "     .     "
            "    ...    "
            "   ..+..   "
            "  ..+++..  "
            " ..++H++.. "
            "  ..+++..  "
            "   ..+..   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, HEPHAESTUS,
            "     .     "
            "    ...    "
            "   .....   "
            "  ..+o+..  "
            " ..++H++.. "
            "  ..+++..  "
            "   ..+..   "
            "    ...    "
            "     .     "
    );
}

TEST(Hephaestus, Attacks) {
    TestAttack(LIGHT, HEPHAESTUS, {APOLLO, POSEIDON},
            "     .     "
            "    ...    "
            "   ..m..   "
            "  ..do...  "
            " .a..H.p.. "
            "  ...A...  "
            "   ..e..   "
            "    ...    "
            "     .     "
    );
}

// Hephaestus boosts attack damage of an adjacent friend by 1
TEST(Hephaestus, Special) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..z..   "
            "  .......  "
            " ..H.N.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);

    EXPECT_EQ(state.hp(DARK, ZEUS), 10);

    ExecuteTurn(state, "N!e7");

    EXPECT_EQ(state.hp(DARK, ZEUS), 7);  // -3

    ExecuteTurn(state, "Z>e8");
    ExecuteTurn(state, "H>d5");
    ExecuteTurn(state, "Z>e7");
    ExecuteTurn(state, "N!e7");

    EXPECT_EQ(state.hp(DARK, ZEUS), 3);  // -4
}

TEST(Hera, Moves) {
    TestMovement(LIGHT, HERA,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   +.+.+   "
            "    +.+    "
            "     E     "
    );

    TestMovement(LIGHT, HERA,
            "     .     "
            "    ...    "
            "   +.+.+   "
            "  ..+.+..  "
            " ..+.E.+.. "
            "  ..+.+..  "
            "   +.+.+   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, HERA,
            "     .     "
            "    ...    "
            "   .....   "
            "  o......  "
            " E.+...... "
            "  +......  "
            "   +....   "
            "    ...    "
            "     .     "
    );
}

TEST(Hera, Attacks) {
    TestAttack(LIGHT, HERA, {DIONYSUS, APOLLO},
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " .z..Ead.. "
            "  m...o..  "
            "   ....p   "
            "    ...    "
            "     .     "
    );
}

// Hera does double damage when attacking from behind or the side
// (note that "behind" depends on which side she's on!)
TEST(Hera, Special) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   P.p..   "
            "  .......  "
            " z.E.e.Z.. "
            "  .......  "
            "   H.h..   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    ExecuteTurn(state, "E!e7");
    ExecuteTurn(state, "E!c7");
    ExecuteTurn(state, "E!a5");
    ExecuteTurn(state, "E!g5");
    ExecuteTurn(state, "E!e3");
    ExecuteTurn(state, "E!c3");

    EXPECT_EQ(state.hp(DARK,  POSEIDON),   2);  // 7 hp - 5 dmg
    EXPECT_EQ(state.hp(LIGHT, POSEIDON),   0);
    EXPECT_EQ(state.hp(DARK,  ZEUS),       0);
    EXPECT_EQ(state.hp(LIGHT, ZEUS),       0);
    EXPECT_EQ(state.hp(DARK,  HEPHAESTUS), 0);
    EXPECT_EQ(state.hp(LIGHT, HEPHAESTUS), 4);  // 9 hp - 5 dmg
}

TEST(Poseidon, Moves) {
    TestMovement(LIGHT, POSEIDON,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  ...+...  "
            "   .+++.   "
            "    +++    "
            "     P     "
    );

    TestMovement(LIGHT, POSEIDON,
            "     .     "
            "    .+.    "
            "   .+++.   "
            "  .+++++.  "
            " .+++P+++. "
            "  .+++++.  "
            "   .+++.   "
            "    .+.    "
            "     .     "
    );

    TestMovement(LIGHT, POSEIDON,
            "     .     "
            "    ...    "
            "   .....   "
            "  o+.....  "
            " P+++..... "
            "  +z.....  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

TEST(Poseidon, AttackLightWithKnockback) {
    State old_state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .ap..   "
            "  ..Ahz..  "
            " ....Pm... "
            "  ...o...  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);
    State new_state = BoardTemplate(
            "     .     "
            "    ap.    "
            "   ..hz.   "
            "  ..A....  "
            " ....Pm... "
            "  ...o...  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(DARK);
    new_state.DecHpForTest(DARK, APHRODITE,  4);
    new_state.DecHpForTest(DARK, POSEIDON,   4);
    new_state.DecHpForTest(DARK, HEPHAESTUS, 4);
    new_state.DecHpForTest(DARK, ZEUS,       4);

    State state = old_state;
    ExecuteTurn(state, "P!e5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

// This is just the reflected version of the above test.
TEST(Poseidon, AttackDarkWithKnockback) {
    State old_state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ...O...  "
            " ....pM... "
            "  ..aHZ..  "
            "   .AP..   "
            "    ...    "
            "     .     "
    ).ToState(DARK);
    State new_state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ...O...  "
            " ....pM... "
            "  ..a....  "
            "   ..HZ.   "
            "    AP.    "
            "     .     "
    ).ToState(LIGHT);
    new_state.DecHpForTest(LIGHT, APHRODITE,  4);
    new_state.DecHpForTest(LIGHT, POSEIDON,   4);
    new_state.DecHpForTest(LIGHT, HEPHAESTUS, 4);
    new_state.DecHpForTest(LIGHT, ZEUS,       4);

    State state = old_state;
    ExecuteTurn(state, "P!e5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

TEST(Poseidon, AttackPushBackBlocked) {
    State old_state = BoardTemplate(
            "     .     "
            "    .m.    "
            "   aZh..   "
            "  .poe...  "
            " ...P..... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);
    State new_state = BoardTemplate(
            "     .     "
            "    .m.    "
            "   aZh..   "
            "  .poe...  "
            " ...P..... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(DARK);
    new_state.DecHpForTest(DARK, APHRODITE,  4);
    new_state.DecHpForTest(DARK, HEPHAESTUS, 4);
    new_state.DecHpForTest(DARK, POSEIDON,   4);
    new_state.DecHpForTest(DARK, APOLLO,     4);
    new_state.DecHpForTest(DARK, HERA,       4);

    State state = old_state;
    ExecuteTurn(state, "P!d5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

// This is debatable: when Poseidon knocks chained enemies back, do they
// become free? In the current implementation yes, even if they remain adjacent
// to Hades (as Zeus in the example below, but not Apollo).
TEST(Poseidon, AttackKnocksBackChainedEnemies) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..o..   "
            "  ..Sz...  "
            " ....P.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    state.Chain(DARK, APOLLO);
    state.Chain(DARK, ZEUS);
    EXPECT_EQ(state.fx(DARK, APOLLO), CHAINED);
    EXPECT_EQ(state.fx(DARK, ZEUS),   CHAINED);

    ExecuteTurn(state, "P!e5");

    EXPECT_EQ(state.fx(DARK, APOLLO), UNAFFECTED);
    EXPECT_EQ(state.fx(DARK, ZEUS),   UNAFFECTED);
}

// Similar to the above: when Poseidon knocks back enemy Hades, chained allies
// that are now out of range are released (Apollo in the example below), while
// those that remain adjacent to Hades remain chained. This is consistent with
// how they stay chained when Hades moves.
//
// Note that this only happens when Hades is shielded by Athena, otherwise he
// would be killed by Poseidon's attack and everyone is released for that
// reason. (And also note that Athena needs to be out of range of Posseidon,
// or she would get killed on the same turn and then cannot protect Hades).
TEST(Poseidon, AttackKnocksBackHades) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ...n.   "
            "  ...sZ..  "
            " ...P.O... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    state.Chain(LIGHT, APOLLO);
    state.Chain(LIGHT, ZEUS);
    EXPECT_EQ(state.fx(LIGHT, APOLLO), CHAINED);
    EXPECT_EQ(state.fx(LIGHT, ZEUS),   CHAINED);

    ExecuteTurn(state, "P!d5");

    EXPECT_EQ(state.fx(LIGHT, APOLLO), UNAFFECTED);  // knocked out of range
    EXPECT_EQ(state.fx(LIGHT, ZEUS),   CHAINED);     // still in range!
}

// Note: because Athena has fewer HP (3) than Poseidon's base damage (4)
// she always dies if she's in range.
TEST(Poseidon, AthenaDies) {
    State old_state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ..znh..  "
            " ....P.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);
    State new_state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .z.h.   "
            "  .......  "
            " ....P.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(DARK);
    new_state.DecHpForTest(DARK, ZEUS,       4);
    new_state.DecHpForTest(DARK, HEPHAESTUS, 4);

    State state = old_state;
    ExecuteTurn(state, "P!e5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

TEST(Poseidon, AthenaPreventsDamageButNotKnockacb) {
    State old_state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ..zh...  "
            " ...P.n... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);
    State new_state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .zh..   "
            "  .......  "
            " ...P.n... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(DARK);
    new_state.DecHpForTest(DARK, ZEUS, 4);
    // Note: Hephaestus takes no damage but is knocked back.

    State state = old_state;
    ExecuteTurn(state, "P!d5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

TEST(Apollo, Moves) {
    TestMovement(LIGHT, APOLLO,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   +++++   "
            "    +++    "
            "     O     "
    );

    TestMovement(LIGHT, APOLLO,
            "     .     "
            "    ...    "
            "   +++++   "
            "  .+++++.  "
            " ..++O++.. "
            "  .+++++.  "
            "   +++++   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, APOLLO,
            "     .     "
            "    ...    "
            "   .....   "
            "  z+.....  "
            " O++...... "
            "  ++.....  "
            "   +....   "
            "    ...    "
            "     .     "
    );
}

TEST(Apollo, Attacks) {
    TestAttack(LIGHT, APOLLO, {DIONYSUS, HERA},
            "     z     "
            "    ...    "
            "   .....   "
            "  ....H..  "
            " .eZ.OAm.. "
            "  ....d..  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

// Apollo does +1 damage when attacking directly.
TEST(Apollo, Special) {
    State state = State::Initial();
    state.Place(LIGHT, APOLLO, ParseField("e5"));
    state.Place(DARK,  ZEUS,   ParseField("c7"));

    ExecuteTurn(state, "O!c7");

    EXPECT_EQ(state.hp(DARK, ZEUS),  7);  // 10 - 3, direct diagonal attack

    ExecuteTurn(state, "Z>c6");
    ExecuteTurn(state, "O!c6");

    EXPECT_EQ(state.hp(DARK,  ZEUS), 5);  // 7 - 2, indirect attack

    ExecuteTurn(state, "Z>c5");
    ExecuteTurn(state, "O!c5");

    EXPECT_EQ(state.hp(DARK,  ZEUS), 2);  // 5 - 3, direct horizontal attack
}

TEST(Aphrodite, Moves) {
    TestMovement(LIGHT, APHRODITE,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  +++++++  "
            "   +++++   "
            "    +++    "
            "     A     "
    );

    TestMovement(LIGHT, APHRODITE,
            "     .     "
            "    +++    "
            "   +++++   "
            "  +++++++  "
            " .+++A+++. "
            "  +++++++  "
            "   +++++   "
            "    +++    "
            "     .     "
    );

    TestMovement(LIGHT, APHRODITE,
            "     .     "
            "    ...    "
            "   .....   "
            "  oZ+....  "
            " A+++..... "
            "  +h+....  "
            "   ++...   "
            "    +..    "
            "     .     "
    );
}

TEST(Aphrodite, Attacks) {
    TestAttack(LIGHT, APHRODITE, {APOLLO, POSEIDON},
            "     .     "
            "    ...    "
            "   ..z..   "
            "  ..p..h.  "
            " ....A.... "
            "  ...o...  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

// Aphrodite can swap with an ally anywhere on the board.
TEST(Aphrodite, Special) {
    State state = BoardTemplate(
            "     .     "
            "    M..    "
            "   .H...   "
            "  .......  "
            " .....N... "
            "  ...A...  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    EXPECT_EQ(state.fx(LIGHT, HEPHAESTUS), SPEED_BOOST);
    EXPECT_EQ(state.fx(LIGHT, APHRODITE),  SHIELDED);
    EXPECT_EQ(state.fx(LIGHT, HERMES),     DAMAGE_BOOST);
    EXPECT_EQ(state.fx(LIGHT, ATHENA),     UNAFFECTED);

    ExecuteTurn(state, "A+d8");

    EXPECT_EQ(state,
        BoardTemplate(
            "     .     "
            "    A..    "
            "   .H...   "
            "  .......  "
            " .....N... "
            "  ...M...  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(DARK));

    // Auras are recomputed based on new positions:
    EXPECT_EQ(state.fx(LIGHT, HEPHAESTUS), UNAFFECTED);
    EXPECT_EQ(state.fx(LIGHT, APHRODITE),  DAMAGE_BOOST);
    EXPECT_EQ(state.fx(LIGHT, HERMES),     SHIELDED);
    EXPECT_EQ(state.fx(LIGHT, ATHENA),     SPEED_BOOST);
}

// Special rule 1: Aphrodite can swap on the turn she is summoned
// (This is clarified under "special rules")
TEST(Aphrodite, Special1) {
    State state = State::Initial();
    state.Place(LIGHT, POSEIDON, ParseField("d8"));

    EXPECT_EQ(state.fi(LIGHT, POSEIDON), ParseField("d8"));

    ExecuteTurn(state, "A@e1,A+d8");

    EXPECT_EQ(state.fi(LIGHT, POSEIDON),  ParseField("e1"));
    EXPECT_EQ(state.fi(LIGHT, APHRODITE), ParseField("d8"));
}

// Special rule 2: Aphrodite can use her special power on the turn she is summoned, even if
// another god already moved:
TEST(Aphrodite, Special2) {
    State state = State::Initial();
    state.Place(LIGHT, POSEIDON, ParseField("d8"));
    state.Place(LIGHT, ZEUS, ParseField("e1"));

    EXPECT_EQ(state.fi(LIGHT, POSEIDON), ParseField("d8"));

    ExecuteTurn(state, "Z>e2,A@e1,A+d8");

    EXPECT_EQ(state.fi(LIGHT, POSEIDON),  ParseField("e1"));
    EXPECT_EQ(state.fi(LIGHT, APHRODITE), ParseField("d8"));
}

// Special rule 3: extra move after killing an enemy at their gate, does not allow
// Aphrodite to swap!
TEST(Aphrodite, Special3) {
    State state = State::Initial();
    state.Place(DARK,  ZEUS,      ParseField("e9"));
    state.Place(LIGHT, ZEUS,      ParseField("e8"));
    state.Place(LIGHT, APHRODITE, ParseField("e7"));

    std::vector<std::string> turns = TurnStrings(state);
    EXPECT_THAT(turns, Contains("A+e8"));            // can swap as only move
    EXPECT_THAT(turns, Contains("Z!e9,A>e9"));       // can move after kill
    EXPECT_THAT(turns, Not(Contains("Z!e9,A+e8")));  // cannot swap after kill
}

// Ares moves up to 3 spaces in any single direction (same as Hades).
TEST(Ares, Moves) {
    TestMovement(LIGHT, ARES,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  +..+..+  "
            "   +.+.+   "
            "    +++    "
            "     R     "
    );

    TestMovement(LIGHT, ARES,
            "     .     "
            "    .+.    "
            "   +.+.+   "
            "  ..+++..  "
            " .+++R+++. "
            "  ..+++..  "
            "   +.+.+   "
            "    .+.    "
            "     .     "
    );

    TestMovement(LIGHT, ARES,
            "     .     "
            "    ...    "
            "   .....   "
            "  o......  "
            " R+Z...... "
            "  +......  "
            "   +....   "
            "    +..    "
            "     .     "
    );
}

TEST(Ares, Attacks) {
    TestAttack(LIGHT, ARES, {DIONYSUS, APOLLO, POSEIDON},
            "     .     "
            "    ...    "
            "   d....   "
            "  ...o...  "
            " a...R..p. "
            "  .......  "
            "   ...m.   "
            "    ...    "
            "     .     "
    );
}

TEST(Ares, Special) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ..Aopz.  "
            " ..nd..... "
            "  ...R...  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    ExecuteTurn(state, "R>e5");

    EXPECT_EQ(state.hp(LIGHT, APHRODITE), pantheon[APHRODITE].hit);  // ally
    EXPECT_EQ(state.hp(DARK,  DIONYSUS),  pantheon[DIONYSUS].hit);  // shielded
    EXPECT_EQ(state.hp(DARK,  APOLLO),    pantheon[APOLLO].hit - 1);
    EXPECT_EQ(state.hp(DARK,  POSEIDON),  pantheon[POSEIDON].hit - 1);
    EXPECT_EQ(state.hp(DARK,  ZEUS),      pantheon[ZEUS].hit);  // out of range
}

TEST(Ares, SpecialOnSummon) {
    State state = State::Initial();
    state.Place(DARK, ZEUS, ParseField("e2"));

    ExecuteTurn(state, "R@e1");

    EXPECT_EQ(state.hp(DARK, ZEUS), pantheon[ZEUS].hit - 1);
}

// When Aphrodite swaps with Ares, he does damage when he lands on the new location.
TEST(Ares, SpecialAfterAphroditeSwaps) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ...Rp..  "
            " ......... "
            "  ..oA...  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    ExecuteTurn(state, "A+e6");

    EXPECT_EQ(state.hp(DARK, APOLLO),   pantheon[APOLLO].hit - 1);
    EXPECT_EQ(state.hp(DARK, POSEIDON), pantheon[POSEIDON].hit);
}

TEST(Ares, SpecialMoveAfterMove) {
    State state = BoardTemplate(
            "     n     "
            "    ...    "
            "   .....   "
            "  ...R...  "
            " ......... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    state.SetHpForTest(DARK, ATHENA, 1);

    EXPECT_THAT(TurnStrings(state), Contains("R>e8,R>e9"));
}

TEST(Ares, SpecialMoveAfterAphroditeSwap) {
    State state = BoardTemplate(
            "     n     "
            "    .A.    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     R     "
    ).ToState(LIGHT);

    state.SetHpForTest(DARK, ATHENA, 1);

    EXPECT_THAT(TurnStrings(state), Contains("A+e1,R>e9"));
}

// Debatable: when Poseidon pushes Ares back, this does NOT count as Ares
// "landing" on a new field, so it does not trigger his damage ability.
TEST(Ares, SpecialNotAfterPoseidonPush) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ....p.... "
            "  ...R...  "
            "   .....   "
            "    .z.    "
            "     .     "
    ).ToState(DARK);

    ExecuteTurn(state, "P!e5");

    ASSERT_THAT(state.hp(LIGHT, ARES), pantheon[ARES].hit - pantheon[POSEIDON].dmg);
    ASSERT_THAT(state.fi(LIGHT, ARES), ParseField("e3"));  // knocked back
    ASSERT_THAT(state.fi(DARK,  ZEUS), ParseField("e2"));  // adjacent
    ASSERT_THAT(state.hp(DARK,  ZEUS), pantheon[ZEUS].hit);  // undamaged
}

// TODO: Hermes

// TODO: Dionysus

// TODO: Artemis

// Hades moves up to 3 spaces in any single direction (same as Ares).
TEST(Hades, Moves) {
    TestMovement(LIGHT, HADES,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  +..+..+  "
            "   +.+.+   "
            "    +++    "
            "     S     "
    );

    TestMovement(LIGHT, HADES,
            "     .     "
            "    .+.    "
            "   +.+.+   "
            "  ..+++..  "
            " .+++S+++. "
            "  ..+++..  "
            "   +.+.+   "
            "    .+.    "
            "     .     "
    );

    // Don't include enemies in this test, because they create additional
    // special actions, which generate additional moves:

    TestMovement(LIGHT, HADES,
            "     .     "
            "    ...    "
            "   .....   "
            "  O......  "
            " S+Z...... "
            "  +......  "
            "   +....   "
            "    +..    "
            "     .     "
    );
}

TEST(Hades, Attacks) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ..Hp...  "
            " .m.So.... "
            "  .zn.a..  "
            "   ..d..   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    ExecuteTurn(state, "S!d5");

    // Note 4 damage instead of 3 because of Hephaestus damage boost
    EXPECT_EQ(state.hp(LIGHT, HEPHAESTUS),      9);  // friendly
    EXPECT_EQ(state.hp(DARK,  HERMES),          5);  // out of range
    EXPECT_EQ(state.hp(DARK,  POSEIDON),    7 - 4);  // hit
    EXPECT_EQ(state.hp(DARK,  APOLLO),      6 - 4);  // hit
    EXPECT_EQ(state.hp(DARK,  ATHENA),          0);  // killed
    EXPECT_EQ(state.hp(DARK,  ZEUS),       10 - 4);  // hit
    EXPECT_EQ(state.hp(DARK,  APHRODITE),       6);  // out of range
    EXPECT_EQ(state.hp(DARK,  DIONYSUS),        4);  // out of range
}

// Chains of Tartarus: bind 1 enemy per turn after landing next to or attacking
// an enemy so they cannot move or attack as Hades remains adjacent.
TEST(Hades, Special) {
    // This test case is essentially the example from the rulebook.
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .O...   "
            "  .ez.h..  "
            " ......... "
            "  .......  "
            "   .S...   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    std::vector<std::string> turns = TurnStrings(state);
    EXPECT_THAT(turns, Contains("S>d5"));
    EXPECT_THAT(turns, Contains("S>d5,S+c6"));
    EXPECT_THAT(turns, Contains("S>d5,S+d6"));
    EXPECT_THAT(turns, Not(Contains("S>d5,S+e6")));       // empty
    EXPECT_THAT(turns, Not(Contains("S>d5,S+f6")));       // not adjacent

    ExecuteTurn(state, "S>d5,S+c6");  // chain Hera at c6

    EXPECT_EQ(state.fx(DARK, HERA), CHAINED);
    EXPECT_EQ(state.fx(DARK, ZEUS), UNAFFECTED);
    EXPECT_EQ(state.fx(DARK, HEPHAESTUS), UNAFFECTED);
    EXPECT_THAT(MoveDestinations(state, HERA), IsEmpty());
    EXPECT_THAT(MoveDestinations(state, ZEUS), Not(IsEmpty()));
    EXPECT_THAT(MoveDestinations(state, HEPHAESTUS), Not(IsEmpty()));
    EXPECT_THAT(AttackTargets(state, HERA), IsEmpty());
    EXPECT_THAT(AttackTargets(state, ZEUS), Contains(HADES));

    state.EndTurn();  // skip dark turn

    EXPECT_THAT(TurnStrings(state), Not(Contains("S!d5,S+c6")));  // cannot chain Hera twice

    ExecuteTurn(state, "S!d5,S+d6");  // attack + chain Zeus at d6

    EXPECT_EQ(state.hp(DARK, HERA),        8 - 3);
    EXPECT_EQ(state.hp(DARK, ZEUS),       10 - 3);
    EXPECT_EQ(state.hp(DARK, HEPHAESTUS),      9);
    EXPECT_EQ(state.fx(DARK, HERA), CHAINED);
    EXPECT_EQ(state.fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(state.fx(DARK, HEPHAESTUS), UNAFFECTED);
    EXPECT_THAT(MoveDestinations(state, HERA), IsEmpty());
    EXPECT_THAT(MoveDestinations(state, ZEUS), IsEmpty());
    EXPECT_THAT(MoveDestinations(state, HEPHAESTUS), Not(IsEmpty()));
    EXPECT_THAT(AttackTargets(state, HERA), IsEmpty());
    EXPECT_THAT(AttackTargets(state, ZEUS), IsEmpty());

    state.EndTurn();  // skip dark turn

    ExecuteTurn(state, "S>c5");  // move does not change chains

    EXPECT_EQ(state.fx(DARK, HERA), CHAINED);
    EXPECT_EQ(state.fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(state.fx(DARK, HEPHAESTUS), UNAFFECTED);

    state.EndTurn();  // skip dark turn

    ExecuteTurn(state, "S>e5,S+f6");  // move + chain Hephaestus at d6

    EXPECT_EQ(state.fx(DARK, HERA), UNAFFECTED);
    EXPECT_EQ(state.fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(state.fx(DARK, HEPHAESTUS), CHAINED);
    EXPECT_THAT(MoveDestinations(state, HERA), Not(IsEmpty()));
    EXPECT_THAT(MoveDestinations(state, ZEUS), IsEmpty());
    EXPECT_THAT(MoveDestinations(state, HEPHAESTUS), IsEmpty());
    EXPECT_THAT(AttackTargets(state, ZEUS), IsEmpty());
    EXPECT_THAT(AttackTargets(state, HEPHAESTUS), IsEmpty());
}

// Hades can chain an enemy when summoning, then move or attack and chain again,
// binding two enemies in one turn.
TEST(Hades, SpecialAfterSummon) {
    State state = State::Initial();
    state.Place(DARK, ZEUS,       ParseField("d2"));
    state.Place(DARK, HEPHAESTUS, ParseField("f2"));

    std::vector<std::string> turns = TurnStrings(state);
    EXPECT_THAT(turns, Contains("S@e1"));
    EXPECT_THAT(turns, Contains("S@e1,S+d2"));
    EXPECT_THAT(turns, Contains("S@e1,S+d2,S>e2"));
    EXPECT_THAT(turns, Contains("S@e1,S+d2,S>e2,S+f2"));
    EXPECT_THAT(turns, Contains("S@e1,S>e2"));
    EXPECT_THAT(turns, Contains("S@e1,S>e2,S+f2"));
    EXPECT_THAT(turns, Contains("S@e1,S+d2,S!e1"));
    EXPECT_THAT(turns, Contains("S@e1,S+d2,S!e1,S+f2"));
    EXPECT_THAT(turns, Contains("S@e1,S!e1"));
    EXPECT_THAT(turns, Contains("S@e1,S!e1,S+f2"));
}

TEST(Hades, SpecialBeforeSummon) {
    State state = State::Initial();
    state.Place(LIGHT, HADES, ParseField("e1"));
    state.Place(DARK,  ZEUS,  ParseField("d2"));

    ExecuteTurn(state, "S>e2,S+d2,O@e1,O!d2");

    EXPECT_EQ(state.fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(state.hp(DARK, ZEUS), 10 - 3);  // 3 = 2 base + 1 Blazing Arrow bonus damage
}

// Special rule 3: if a player kills an enemy at their gate, then the player
// may move again; this is another way Hades can bind two enemies in one turn.
TEST(Hades, SpecialAfterKillingAtEnemyGate) {
    //     d e f
    // 9     n
    // 8   z S p
    // 7 . . . . .
    State state = State::Initial();
    state.Place(LIGHT, HADES,    ParseField("e8"));
    state.Place(DARK,  ZEUS,     ParseField("d8"));
    state.Place(DARK,  POSEIDON, ParseField("f8"));
    state.Place(DARK,  ATHENA,   ParseField("e9"));

    ExecuteTurn(state, "S!e8,S+d8,S>e7,S+f8");

    EXPECT_TRUE(state.IsDead(DARK, ATHENA));
    EXPECT_EQ(state.fx(DARK, ZEUS),     CHAINED);
    EXPECT_EQ(state.fx(DARK, POSEIDON), CHAINED);
    EXPECT_EQ(state.hp(DARK, ZEUS),      10 - 3);
    EXPECT_EQ(state.hp(DARK, POSEIDON),   7 - 3);

    ExecuteTurn(state, "R@e9,R!e7");

    // If Hades gets killed, chained gods are released.
    EXPECT_TRUE(state.IsDead(LIGHT, HADES));
    EXPECT_EQ(state.fx(DARK, ZEUS),     UNAFFECTED);
    EXPECT_EQ(state.fx(DARK, POSEIDON), UNAFFECTED);
}

// Hades can chain Athena and her shielded allies
TEST(Hades, SpecialProtectedByAthena) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   ..n..   "
            "    z..    "
            "     S     "
    ).ToState(LIGHT);

    EXPECT_EQ(state.fx(DARK, ZEUS), SHIELDED);
    EXPECT_THAT(TurnStrings(state), Contains("S!e1,S+d2"));
    EXPECT_THAT(TurnStrings(state), Contains("S>e2,S+e3"));
}

// Aphrodite swapping with Hades allows him to chain another enemy,
// preserving chains to adjacent enemies (only).
TEST(Hades, SpecialAfterAphroditeSwaps) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ...z...  "
            " ...oAe... "
            "  ...S...  "
            "   ..p..   "
            "    ...    "
            "     .     "
    ).ToState(LIGHT);

    state.Chain(DARK, APOLLO);
    state.Chain(DARK, POSEIDON);

    EXPECT_EQ(state.fx(DARK, ZEUS),     UNAFFECTED);
    EXPECT_EQ(state.fx(DARK, APOLLO),   CHAINED);
    EXPECT_EQ(state.fx(DARK, HERA),     UNAFFECTED);
    EXPECT_EQ(state.fx(DARK, POSEIDON), CHAINED);

    ExecuteTurn(state, "A+e4,S+e6");

    EXPECT_EQ(state.fx(DARK, ZEUS),     CHAINED);
    EXPECT_EQ(state.fx(DARK, APOLLO),   CHAINED);
    EXPECT_EQ(state.fx(DARK, HERA),     UNAFFECTED);
    EXPECT_EQ(state.fx(DARK, POSEIDON), UNAFFECTED);
}

TEST(Athena, Moves) {
    TestMovement(LIGHT, ATHENA,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   .....   "
            "    +++    "
            "     N     "
    );

    TestMovement(LIGHT, ATHENA,
            "     .     "
            "    ...    "
            "   .....   "
            "  ..+++..  "
            " ...+N+... "
            "  ..+++..  "
            "   .....   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, ATHENA,
            "     .     "
            "    ...    "
            "   .....   "
            "  o......  "
            " N+....... "
            "  +......  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

TEST(Athena, Attacks) {
    TestAttack(LIGHT, ATHENA, {APOLLO, POSEIDON},
            "     .     "
            "    .o.    "
            "   m....   "
            "  ..p..h.  "
            " z...N.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

// Aegis Shield: friends on adjacent spaces are immune to damage.
//
// There are some special tests regarding Athena's aura interacting
// with area of effect damage under the Poseidon test cases too.
TEST(Athena, Special) {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..zZ.   "
            "  ..TN.O.  "
            " ......... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(DARK);

    EXPECT_TRUE (state.has_fx(LIGHT, ZEUS,    SHIELDED));
    EXPECT_TRUE (state.has_fx(LIGHT, ARTEMIS, SHIELDED));
    EXPECT_FALSE(state.has_fx(LIGHT, ATHENA,  SHIELDED));  // cannot shield self
    EXPECT_FALSE(state.has_fx(LIGHT, APOLLO,  SHIELDED));  // out of range

    ExecuteTurn(state, "Z!e6");

    EXPECT_EQ(state.hp(LIGHT, ZEUS), 10);  // no damage taken
}

// TODO: hermes, dionysus, artemis

// TODO: dionysis cannot kill enemy protected by athena
// TODO: dionysis can move twice when adjacent to hermes
// TODO: arthemis cannot use withering moon on enemy protected athena
// TODO: hermes CAN kill enemy protected by athena when killing athena on the same turn
