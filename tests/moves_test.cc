#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Contains;
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
        data[Flip(j++)] = ch;
    }
    assert(j == FIELD_COUNT);
}

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
    TestAttack(LIGHT, ZEUS, {POSEIDON, APOLLO, DIONYSOS, ATHENA, HERA},
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
    TestAttack(LIGHT, HERA, {DIONYSOS, APOLLO},
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
    TestAttack(LIGHT, APOLLO, {DIONYSOS, HERA},
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

// TODO: aphrodite, ares, hermes, dionysus, artemis, hades, athena

// TODO: Poseidon/Hades test:
//      - when Hades gets knocked back, chains should stay on enemies that remain
//        adjacent, but are removed from enemies that are no longer adjacent (note
//        that when Hades is knocked back, his enemies aren't)
//      - when enemies get knocked back, they should be dechained from friendly Hades!
