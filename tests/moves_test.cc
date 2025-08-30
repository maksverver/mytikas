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
#include <ranges>
#include <vector>

struct Escaped {
    std::string_view sv;
};

std::ostream& operator<<(std::ostream &os, Escaped es) {
    os << '"';
    for (char ch : es.sv) {
        if (ch == '"') {
            os << "\\\"";
        } else if (ch < ' ' || ch >= 127) {
            os << '\\' << std::oct << (int)ch;
        } else {
            os << ch;
        }
    }
    os << '"';
    return os;
}

std::ostream& operator<<(std::ostream &os, const std::vector<std::string> &v) {
    os << "{";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) os << ", ";
        os << Escaped{v[i]};
    }
    os << "}";
    return os;
}

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

// This returns all fields of attack actions for the current god.
// It doesn't work for area of effect attacks, performed by the
// likes of Dionysus, Hades, Poseidon, etc.
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

// This tests area of effect attacks by checking which gods are reduced in HP.
std::vector<God> AreaAttackTargets(const State &old_state, God god) {
    std::vector<God> res;
    for (const Turn &turn : GenerateTurns(old_state)) {
        if ( turn.naction > 0 &&
             turn.actions[0].type == Action::ATTACK &&
             turn.actions[0].god == god ) {
            State new_state = old_state;
            ExecuteAction(new_state, turn.actions[0]);
            for (int p = 0; p < 2; ++p) {
                for (int g = 0; g < GOD_COUNT; ++g) {
                    if ( old_state.hp(AsPlayer(p), AsGod(g)) !=
                         new_state.hp(AsPlayer(p), AsGod(g)) ) {
                        res.push_back(AsGod(g));
                    }
                }
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

    State state = State::InitialNoneSummonable();
    for (auto [player, god, field] : places) {
        state.Place(player, god, field);
    }
    if (player != state.NextPlayer()) state.EndTurn();
    return state;
}

std::vector<std::string_view> GodNames(const std::ranges::range auto &r) {
    std::vector<std::string_view> res;
    for (auto god : r) res.push_back(GodName(god));
    return res;
}

std::vector<std::string_view> FieldNames(const std::ranges::range auto &r) {
    std::vector<std::string_view> res;
    for (auto field : r) res.push_back(FieldName(field));
    return res;
}

#define TestMovement(...) do { SCOPED_TRACE("called here"); TestMovementImpl(__VA_ARGS__); } while(0)

void TestMovementImpl(Player player, God god, const BoardTemplate &t) {
    State state = t.ToState(player);
    std::vector<field_t> expected;
    for (field_t field = 0; field < FIELD_COUNT; ++field) {
        if (t[field] == '+') expected.push_back(field);
    }
    EXPECT_THAT(FieldNames(MoveDestinations(state, god)),
            UnorderedElementsAreArray(FieldNames(expected)));
}

enum class TestAttackOptions {
    DEFAULT,
    DEDUPLICATE
};

#define TestAttack(...) do { SCOPED_TRACE("called here"); TestAttackImpl(__VA_ARGS__); } while(0)

void TestAttackImpl(Player player, God god, std::vector<God> expected, const BoardTemplate &t,
        TestAttackOptions options=TestAttackOptions::DEFAULT) {
    const State &state = t.ToState(player);
    std::vector<God> received = AttackTargets(state, god);
    if (options == TestAttackOptions::DEDUPLICATE) {
        std::ranges::sort(received);
        received.erase(std::unique(received.begin(), received.end()), received.end());
    }
    EXPECT_THAT(GodNames(received),
            UnorderedElementsAreArray(GodNames(expected)));
}

#define TestAreaAttack(...) do { SCOPED_TRACE("called here"); TestAreaAttackImpl(__VA_ARGS__); } while(0)

// This finds all the heroes that were damaged by an attack.
void TestAreaAttackImpl(Player player, God god, std::vector<God> expected, const BoardTemplate &t) {
    const State &state = t.ToState(player);
    std::vector<God> received = AreaAttackTargets(state, god);
    EXPECT_THAT(GodNames(received),
            UnorderedElementsAreArray(GodNames(expected)));
}

// Test fixture for all unit tests.
class MovesTest : public testing::Test {
public:
    ~MovesTest() {
        if (Test::HasFailure()) {
            std::cout << "state=" << State::DebugPrint{state} << '\n';
            std::cout << "turns=" << TurnStrings() << '\n';
        }
    }

protected:

    State state = State::InitialAllSummonable();

    std::vector<std::string> TurnStrings() {
        return ::TurnStrings(state);
    }

    std::optional<Turn> FindTurn(std::string_view sv) {
        return ::FindTurn(state, sv);
    }

    void ExecuteTurn(std::string_view sv) {
        ASSERT_THAT(TurnStrings(), Contains(sv));
        auto turn = FindTurn(sv);
        ASSERT_TRUE(turn);
        ::ExecuteTurn(state, *turn);
    }

    void EndTurn() {
        state.EndTurn();
    }

    std::vector<field_t> MoveDestinations(God god) {
        return ::MoveDestinations(state, god);
    }


    std::vector<God> AttackTargets(God god) {
        return ::AttackTargets(state, god);
    }

    std::vector<God> AreaAttackTargets(God god) {
        return ::AreaAttackTargets(state, god);
    }

    int PlayerAt(field_t field) {
        return state.PlayerAt(field);
    }

    int PlayerAt(std::string_view sv) {
        return PlayerAt(ParseField(sv));
    }

    int GodAt(field_t field) {
        return state.GodAt(field);
    }

    int GodAt(std::string_view sv) {
        return GodAt(ParseField(sv));
    }

    bool IsDead(Player player, God god) {
        return state.IsDead(player, god);
    }

    int hp(Player player, God god) {
        return state.hp(player, god);
    }

    int fi(Player player, God god) {
        return state.fi(player, god);
    }

    StatusFx fx(Player player, God god) {
        return state.fx(player, god);
    }

    bool has_fx(Player player, God god, StatusFx mask) {
        return state.has_fx(player, god, mask);
    }

    void SetHp(Player player, God god, int hp) {
        state.SetHpForTest(player, god, hp);
    }

    void Chain(Player player, God god) {
        return state.Chain(player, god);
    }

    void Unchain(Player player, God god) {
        return state.Unchain(player, god);
    }

    void Place(Player player, God god, std::string_view field) {
        state.Place(player, god, ParseField(field));
    }

    void Move(Player player, God god, std::string_view field) {
        state.Move(player, god, ParseField(field));
    }

    void Remove(Player player, God god) {
        state.Remove(player, god);
    }
};

}  // namespace

TEST_F(MovesTest, SpecialRule1_Basic) {
    // On the turn that a character is “summoned” onto your Gates Space,
    // that character can also move or attack on the same turn.
    Place(DARK, ZEUS, "e2");

    auto turns = TurnStrings();

    EXPECT_THAT(turns, Contains("N@e1,N!e2"));
    EXPECT_THAT(turns, Contains("N@e1,N!e2"));
}

TEST_F(MovesTest, SpecialRule2_Basic) {
    // If your home gate is occupied, you may move a piece and summon a god afterward,
    // who may then attack but not move, but may use a special ability.
    Place(DARK, ZEUS, "e2");
    Place(LIGHT, ZEUS, "e1");

    auto turns = TurnStrings();

    EXPECT_THAT(turns,     Contains("Z>d2,N@e1,N!e2") );
    EXPECT_THAT(turns, Not(Contains("Z>d2,N@e1,N>f2")));
}

TEST_F(MovesTest, SpecialRule3_Basic) {
    // If you kill an enemy on their Gates Space, you may move any of your
    // characters that are already on the board immediately.
    state = BoardTemplate(
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

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("Z!e9,Z>e5"));  // doesn't have to move to goal
    EXPECT_THAT(turns, Contains("Z!e9,E>e9"));  // doesn't have to be the same God
}

TEST_F(MovesTest, StatusEffects_Basic) {
    // 3  . . . P .
    // 2    Z z .
    // 1      N
    //    c d e f g

    God N = ATHENA;
    God Z = ZEUS;
    God P = POSEIDON;

    // When summoning a god, its aura gets applied to neighboring allies:
    Place(LIGHT, Z, "d2");
    Place(DARK,  Z, "e2");
    Place(LIGHT, P, "f3");
    Place(LIGHT, N, "e1");
    EXPECT_EQ(fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, Z), SHIELDED);
    EXPECT_EQ(fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(fx(DARK,  Z), UNAFFECTED);

    // Ally moving away removes status:
    Move(LIGHT, Z, "c3");
    EXPECT_EQ(fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, Z), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(fx(DARK,  Z), UNAFFECTED);

    // Ally moving towards adds status:
    Move(LIGHT, P, "f2");
    EXPECT_EQ(fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, Z), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, P), SHIELDED);
    EXPECT_EQ(fx(DARK,  Z), UNAFFECTED);

    // Moving towards ally adds status, way from ally removes status:
    Move(LIGHT, N, "d2");
    EXPECT_EQ(fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, Z), SHIELDED);
    EXPECT_EQ(fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(fx(DARK,  Z), UNAFFECTED);

    // Removing god removes status.
    Remove(LIGHT, N);
    EXPECT_EQ(fx(LIGHT, N), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, Z), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, P), UNAFFECTED);
    EXPECT_EQ(fx(DARK,  Z), UNAFFECTED);
}

TEST_F(MovesTest, StatusEffects_Overlap) {
    Place(LIGHT, ZEUS,       "e1");
    Place(LIGHT, HEPHAESTUS, "d2");
    Place(LIGHT, ATHENA,     "e2");
    Place(LIGHT, HERMES,     "f2");

    EXPECT_EQ(fx(LIGHT, ZEUS),        DAMAGE_BOOST | SPEED_BOOST | SHIELDED);
    EXPECT_EQ(fx(LIGHT, HEPHAESTUS),  SHIELDED);
    EXPECT_EQ(fx(LIGHT, ATHENA),      DAMAGE_BOOST | SPEED_BOOST);
    EXPECT_EQ(fx(LIGHT, HERMES),      SHIELDED);

    Remove(LIGHT, ATHENA);

    EXPECT_EQ(fx(LIGHT, ZEUS),       DAMAGE_BOOST | SPEED_BOOST);
    EXPECT_EQ(fx(LIGHT, HEPHAESTUS), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, ATHENA),     UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, HERMES),     UNAFFECTED);
}

TEST_F(MovesTest, Zeus_Moves) {
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

TEST_F(MovesTest, Zeus_Attacks) {
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
TEST_F(MovesTest, Zeus_Special) {
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

TEST_F(MovesTest, Hephaestus_Moves) {
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

TEST_F(MovesTest, Hephaestus_Attacks) {
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
TEST_F(MovesTest, Hephaestus_Special) {
    state = BoardTemplate(
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

    EXPECT_EQ(hp(DARK, ZEUS), 10);

    ExecuteTurn("N!e7");

    EXPECT_EQ(hp(DARK, ZEUS), 7);  // -3

    ExecuteTurn("Z>e8");
    ExecuteTurn("H>d5");
    ExecuteTurn("Z>e7");
    ExecuteTurn("N!e7");

    EXPECT_EQ(hp(DARK, ZEUS), 3);  // -4
}

TEST_F(MovesTest, Hera_Moves) {
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

TEST_F(MovesTest, Hera_Attacks) {
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
TEST_F(MovesTest, Hera_Special) {
    state = BoardTemplate(
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

    ExecuteTurn("E!e7");
    ExecuteTurn("E!c7");
    ExecuteTurn("E!a5");
    ExecuteTurn("E!g5");
    ExecuteTurn("E!e3");
    ExecuteTurn("E!c3");

    EXPECT_EQ(hp(DARK,  POSEIDON),   2);  // 7 hp - 5 dmg
    EXPECT_EQ(hp(LIGHT, POSEIDON),   0);
    EXPECT_EQ(hp(DARK,  ZEUS),       0);
    EXPECT_EQ(hp(LIGHT, ZEUS),       0);
    EXPECT_EQ(hp(DARK,  HEPHAESTUS), 0);
    EXPECT_EQ(hp(LIGHT, HEPHAESTUS), 4);  // 9 hp - 5 dmg
}

TEST_F(MovesTest, Poseidon_Moves) {
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

TEST_F(MovesTest, Poseidon_AttackLightWithKnockback) {
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

    state = old_state;
    ExecuteTurn("P!e5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

// This is just the reflected version of the above test.
TEST_F(MovesTest, Poseidon_AttackDarkWithKnockback) {
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

    state = old_state;
    ExecuteTurn("P!e5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

TEST_F(MovesTest, Poseidon_AttackPushBackBlocked) {
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

    state = old_state;
    ExecuteTurn("P!d5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

// This is debatable: when Poseidon knocks chained enemies back, do they
// become free? In the current implementation yes, even if they remain adjacent
// to Hades (as Zeus in the example below, but not Apollo).
TEST_F(MovesTest, Poseidon_AttackKnocksBackChainedEnemies) {
    state = BoardTemplate(
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

    Chain(DARK, APOLLO);
    Chain(DARK, ZEUS);
    EXPECT_EQ(fx(DARK, APOLLO), CHAINED);
    EXPECT_EQ(fx(DARK, ZEUS),   CHAINED);

    ExecuteTurn("P!e5");

    EXPECT_EQ(fx(DARK, APOLLO), UNAFFECTED);
    EXPECT_EQ(fx(DARK, ZEUS),   UNAFFECTED);
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
TEST_F(MovesTest, Poseidon_AttackKnocksBackHades) {
    state = BoardTemplate(
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

    Chain(LIGHT, APOLLO);
    Chain(LIGHT, ZEUS);
    EXPECT_EQ(fx(LIGHT, APOLLO), CHAINED);
    EXPECT_EQ(fx(LIGHT, ZEUS),   CHAINED);

    ExecuteTurn("P!d5");

    EXPECT_EQ(fx(LIGHT, APOLLO), UNAFFECTED);  // knocked out of range
    EXPECT_EQ(fx(LIGHT, ZEUS),   CHAINED);     // still in range!
}

// Note: because Athena has fewer HP (3) than Poseidon's base damage (4)
// she always dies if she's in range.
TEST_F(MovesTest, Poseidon_AthenaDies) {
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
            "   .znh.   "
            "  .......  "
            " ....P.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
    ).ToState(DARK);
    new_state.Kill(DARK, ATHENA);
    new_state.DecHpForTest(DARK, ZEUS,       4);
    new_state.DecHpForTest(DARK, HEPHAESTUS, 4);

    state = old_state;
    ExecuteTurn("P!e5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

TEST_F(MovesTest, Poseidon_AthenaPreventsDamageButNotKnockacb) {
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

    state = old_state;
    ExecuteTurn("P!d5");
    EXPECT_EQ(state, new_state)
        << "Expected:\n" << State::DebugPrint(new_state) << '\n'
        << "Received:\n" << State::DebugPrint(state) << '\n';
}

TEST_F(MovesTest, Apollo_Moves) {
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

TEST_F(MovesTest, Apollo_Attacks) {
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
TEST_F(MovesTest, Apollo_Special) {
    Place(LIGHT, APOLLO, "e5");
    Place(DARK,  ZEUS,   "c7");

    ExecuteTurn("O!c7");

    EXPECT_EQ(hp(DARK, ZEUS),  7);  // 10 - 3, direct diagonal attack

    ExecuteTurn("Z>c6");
    ExecuteTurn("O!c6");

    EXPECT_EQ(hp(DARK,  ZEUS), 5);  // 7 - 2, indirect attack

    ExecuteTurn("Z>c5");
    ExecuteTurn("O!c5");

    EXPECT_EQ(hp(DARK,  ZEUS), 2);  // 5 - 3, direct horizontal attack
}

// Aphrodite moves the same as Hermes
TEST_F(MovesTest, Aphrodite_Moves) {
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

TEST_F(MovesTest, Aphrodite_Attacks) {
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
TEST_F(MovesTest, Aphrodite_Special) {
    state = BoardTemplate(
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

    EXPECT_EQ(fx(LIGHT, HEPHAESTUS), SPEED_BOOST);
    EXPECT_EQ(fx(LIGHT, APHRODITE),  SHIELDED);
    EXPECT_EQ(fx(LIGHT, HERMES),     DAMAGE_BOOST);
    EXPECT_EQ(fx(LIGHT, ATHENA),     UNAFFECTED);

    ExecuteTurn("A+d8");

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
    EXPECT_EQ(fx(LIGHT, HEPHAESTUS), UNAFFECTED);
    EXPECT_EQ(fx(LIGHT, APHRODITE),  DAMAGE_BOOST);
    EXPECT_EQ(fx(LIGHT, HERMES),     SHIELDED);
    EXPECT_EQ(fx(LIGHT, ATHENA),     SPEED_BOOST);
}

// Special rule 1: Aphrodite can swap on the turn she is summoned
// (This is clarified under "special rules")
TEST_F(MovesTest, Aphrodite_Special1) {
    Place(LIGHT, POSEIDON, "d8");

    EXPECT_EQ(fi(LIGHT, POSEIDON), ParseField("d8"));

    ExecuteTurn("A@e1,A+d8");

    EXPECT_EQ(fi(LIGHT, POSEIDON),  ParseField("e1"));
    EXPECT_EQ(fi(LIGHT, APHRODITE), ParseField("d8"));
}

// Special rule 2: Aphrodite can use her special power on the turn she is summoned, even if
// another god already moved:
TEST_F(MovesTest, Aphrodite_Special2) {
    Place(LIGHT, POSEIDON, "d8");
    Place(LIGHT, ZEUS, "e1");

    EXPECT_EQ(fi(LIGHT, POSEIDON), ParseField("d8"));

    ExecuteTurn("Z>e2,A@e1,A+d8");

    EXPECT_EQ(fi(LIGHT, POSEIDON),  ParseField("e1"));
    EXPECT_EQ(fi(LIGHT, APHRODITE), ParseField("d8"));
}

// Special rule 3: extra move after killing an enemy at their gate, does not allow
// Aphrodite to swap!
TEST_F(MovesTest, Aphrodite_Special3) {
    Place(DARK,  ZEUS,      "e9");
    Place(LIGHT, ZEUS,      "e8");
    Place(LIGHT, APHRODITE, "e7");

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("A+e8"));            // can swap as only move
    EXPECT_THAT(turns, Contains("Z!e9,A>e9"));       // can move after kill
    EXPECT_THAT(turns, Not(Contains("Z!e9,A+e8")));  // cannot swap after kill
}

// Ares moves up to 3 spaces in any single direction (same as Hades).
TEST_F(MovesTest, Ares_Moves) {
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

TEST_F(MovesTest, Ares_Attacks) {
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

TEST_F(MovesTest, Ares_Special) {
    state = BoardTemplate(
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

    ExecuteTurn("R>e5");

    EXPECT_EQ(hp(LIGHT, APHRODITE), pantheon[APHRODITE].hit);  // ally
    EXPECT_EQ(hp(DARK,  DIONYSUS),  pantheon[DIONYSUS].hit);  // shielded
    EXPECT_EQ(hp(DARK,  APOLLO),    pantheon[APOLLO].hit - 1);
    EXPECT_EQ(hp(DARK,  POSEIDON),  pantheon[POSEIDON].hit - 1);
    EXPECT_EQ(hp(DARK,  ZEUS),      pantheon[ZEUS].hit);  // out of range
}

TEST_F(MovesTest, Ares_SpecialOnSummon) {
    Place(DARK, ZEUS, "e2");

    ExecuteTurn("R@e1");

    EXPECT_EQ(hp(DARK, ZEUS), pantheon[ZEUS].hit - 1);
}

// When Aphrodite swaps with Ares, he does damage when he lands on the new location.
TEST_F(MovesTest, Ares_SpecialAfterAphroditeSwaps) {
    state = BoardTemplate(
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

    ExecuteTurn("A+e6");

    EXPECT_EQ(hp(DARK, APOLLO),   pantheon[APOLLO].hit - 1);
    EXPECT_EQ(hp(DARK, POSEIDON), pantheon[POSEIDON].hit);
}

TEST_F(MovesTest, Ares_SpecialMoveAfterMove) {
    state = BoardTemplate(
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

    SetHp(DARK, ATHENA, 1);

    EXPECT_THAT(TurnStrings(), Contains("R>e8,R>e9"));
}

TEST_F(MovesTest, Ares_SpecialMoveAfterAphroditeSwap) {
    state = BoardTemplate(
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

    SetHp(DARK, ATHENA, 1);

    EXPECT_THAT(TurnStrings(), Contains("A+e1,R>e9"));
}

// Debatable: when Poseidon pushes Ares back, this does NOT count as Ares
// "landing" on a new field, so it does not trigger his damage ability.
TEST_F(MovesTest, Ares_SpecialNotAfterPoseidonPush) {
    state = BoardTemplate(
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

    ExecuteTurn("P!e5");

    ASSERT_THAT(hp(LIGHT, ARES), pantheon[ARES].hit - pantheon[POSEIDON].dmg);
    ASSERT_THAT(fi(LIGHT, ARES), ParseField("e3"));  // knocked back
    ASSERT_THAT(fi(DARK,  ZEUS), ParseField("e2"));  // adjacent
    ASSERT_THAT(hp(DARK,  ZEUS), pantheon[ZEUS].hit);  // undamaged
}

// Hermes moves the same as Aphrodite
TEST_F(MovesTest, Hermes_Moves) {
    TestMovement(LIGHT, HERMES,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  +++++++  "
            "   +++++   "
            "    +++    "
            "     M     "
    );

    TestMovement(LIGHT, HERMES,
            "     .     "
            "    +++    "
            "   +++++   "
            "  +++++++  "
            " .+++M+++. "
            "  +++++++  "
            "   +++++   "
            "    +++    "
            "     .     "
    );

    TestMovement(LIGHT, HERMES,
            "     .     "
            "    ...    "
            "   .....   "
            "  oZ+....  "
            " M+++..... "
            "  +a+....  "
            "   ++...   "
            "    +..    "
            "     .     "
    );
}

TEST_F(MovesTest, Hermes_Attacks) {
    TestAttack(LIGHT, HERMES, {DIONYSUS, APOLLO},
            "     .     "
            "    ...    "
            "   d....   "
            "  ...o...  "
            " a...M..p. "
            "  .......  "
            "   ...r.   "
            "    ...    "
            "     .     "
        , TestAttackOptions::DEDUPLICATE);
}

TEST_F(MovesTest, Hermes_AttackAthenaFirst) {
    if (!hermes_canonicalize_attacks) {
        GTEST_SKIP() << "This test requires hermes_canonicalize_attacks = true";
    }

    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ...e...  "
            " ...nM.z.. "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);
    SetHp(DARK, ATHENA, pantheon[HERMES].dmg);

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("M!e6"));
    EXPECT_THAT(turns, Contains("M!d5"));
    EXPECT_THAT(turns, Contains("M!g5"));
    EXPECT_THAT(turns, Contains("M!d5,M!e6"));
    EXPECT_THAT(turns, Contains("M!d5,M!g5"));
    EXPECT_THAT(turns, Contains("M!e6,M!g5"));
    EXPECT_THAT(turns, Not(Contains("M!e6,M!d5")));  // deduped
    EXPECT_THAT(turns, Not(Contains("M!g5,M!d5")));  // deduped
    EXPECT_THAT(turns, Not(Contains("M!g5,M!e6")));  // deduped
    EXPECT_THAT(turns, Not(Contains("M!d5,M!e6,M!g5")));  // no more than 2 attacks

    ExecuteTurn("M!d5,M!g5");

    EXPECT_TRUE(IsDead(DARK, ATHENA));
    EXPECT_EQ(hp(DARK, HERA), pantheon[HERA].hit);
    EXPECT_EQ(hp(DARK, ZEUS), pantheon[ZEUS].hit - pantheon[HERMES].dmg);
}

TEST_F(MovesTest, Hermes_AttackTwice) {
    if (hermes_canonicalize_attacks) {
        GTEST_SKIP() << "This test requires hermes_canonicalize_attacks == false";
    }

    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   .....   "
            "  ...e...  "
            " ...nM.z.. "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);
    SetHp(DARK, ATHENA, pantheon[HERMES].dmg);

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("M!d5"));
    EXPECT_THAT(turns, Contains("M!e6"));
    EXPECT_THAT(turns, Contains("M!g5"));
    EXPECT_THAT(turns, Contains("M!d5,M!e6"));
    EXPECT_THAT(turns, Contains("M!d5,M!g5"));
    EXPECT_THAT(turns, Contains("M!e6,M!g5"));
    EXPECT_THAT(turns, Contains("M!e6,M!d5"));
    EXPECT_THAT(turns, Contains("M!g5,M!d5"));
    EXPECT_THAT(turns, Contains("M!g5,M!e6"));
    EXPECT_THAT(turns, Not(Contains("M!d5,M!e6,M!g5")));  // no more than 2 attacks

    ExecuteTurn("M!e6,M!d5");
    EXPECT_EQ(hp(DARK, HERA), pantheon[HERA].hit);  // protected by Athena
    EXPECT_TRUE(IsDead(DARK, ATHENA));

    EndTurn();

    ExecuteTurn("M!g5,M!e6");
    EXPECT_EQ(hp(DARK, ATHENA), pantheon[ATHENA].hit - pantheon[HERMES].dmg);
    EXPECT_EQ(hp(DARK, ZEUS),   pantheon[ZEUS].hit   - pantheon[HERMES].dmg);
}

TEST_F(MovesTest, Hermes_Special) {
    state = BoardTemplate(
            "     z     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ....R.... "
            "  ...M...  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("R>a5"));
    EXPECT_THAT(turns, Contains("R>i5"));
    EXPECT_THAT(turns, Not(Contains("R!e9")));  // attack range not boosted
}

TEST_F(MovesTest, Dionysus_Moves) {
    TestMovement(LIGHT, DIONYSUS,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   .+.+.   "
            "    ...    "
            "     D     "
    );

    TestMovement(LIGHT, DIONYSUS,
            "     .     "
            "    ...    "
            "   .+.+.   "
            "  .+...+.  "
            " ....D.... "
            "  .+...+.  "
            "   .+.+.   "
            "    ...    "
            "     .     "
    );

    // Dionysus can jump over opponents and friendly pieces
    TestMovement(LIGHT, DIONYSUS,
            "     .     "
            "    ...    "
            "   .+.+.   "
            "  .+ZHE+.  "
            " ...mDP... "
            "  .+rao+.  "
            "   .+.+.   "
            "    ...    "
            "     .     "
    );

    // Dionysus can move twice when boosted by Hermes
    TestMovement(LIGHT, DIONYSUS,
            "     +     "
            "    +.+    "
            "   .+++.   "
            "  +++.+++  "
            " +.+MD.+.+ "
            "  +++.+++  "
            "   .+++.   "
            "    +.+    "
            "     +     "
    );

    // Dionysus still cannot move through friendly pieces
    TestMovement(LIGHT, DIONYSUS,
            "     .     "
            "    ...    "
            "   .A.Z.   "
            "  +O+.+P+  "
            " +.+MD.+.+ "
            "  +++.+++  "
            "   .+++.   "
            "    +.+    "
            "     +     "
    );
}

TEST_F(MovesTest, Dionysus_Attack) {
    TestAreaAttack(LIGHT, DIONYSUS, {APHRODITE, POSEIDON},
            "     .     "
            "    ...    "
            "   .....   "
            "  .z.....  "
            " a....M.p. "
            "  .Do....  "
            "   e....   "
            "    ...    "
            "     .     "
    );
    TestAreaAttack(DARK, DIONYSUS, {APHRODITE, POSEIDON},
            "     .     "
            "    ...    "
            "   .O...   "
            "  ..dE...  "
            " A....m.P. "
            "  ..Z....  "
            "   .....   "
            "    ...    "
            "     .     "
    );
}

TEST_F(MovesTest, Dionysus_Special) {
    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   no.Z.   "
            "  .......  "
            " ....D.... "
            "  .......  "
            "   .a...   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("D+d3"));
    EXPECT_THAT(turns, Not(Contains("D>d7")));  // cannot jump on shielded enemy
    EXPECT_THAT(turns, Not(Contains("D+d7")));  // cannot jump on shielded enemy
    EXPECT_THAT(turns, Not(Contains("D>f7")));  // cannot jump on ally
    EXPECT_THAT(turns, Not(Contains("D+f7")));  // cannot jump on ally
    EXPECT_FALSE(IsDead(DARK, APHRODITE));

    ExecuteTurn("D+d3");

    EXPECT_EQ(PlayerAt("d3"), LIGHT);
    EXPECT_EQ(GodAt("d3"), DIONYSUS);
    EXPECT_TRUE(IsDead(DARK, APHRODITE));
}

TEST_F(MovesTest, Dionysus_SpecialWithSpeedBoost) {
    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..o..   " // 7
            "  ....h..  " // 6
            " ...a.z... " // 5
            "  .......  " // 4
            "   .MD..   " // 3
            "    ...    "
            "     .     "
        //    abcdefghi
        ).ToState(LIGHT);

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("D>g4"));   // single move
    EXPECT_THAT(turns, Contains("D>e5"));   // double move
    EXPECT_THAT(turns, Contains("D+f5"));   // single kill
    EXPECT_THAT(turns, Not(Contains("D>g4,D>e5"))); // should be merged
    EXPECT_THAT(turns, Contains("D+f6"));           // move then kill
    EXPECT_THAT(turns, Contains("D+f5,D>d6"));      // kill then move
    EXPECT_THAT(turns, Contains("D+f5,D+e7"));      // double kill!
    EXPECT_THAT(turns, Contains("D+d5,D+e7"));      // double kill!
}

TEST_F(MovesTest, Dionysus_SpecialWithSpeedBoost_DeduplicateEqualTurns) {
    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..o..   "
            "  .......  "
            " ....DM... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("D>e3").Times(1));
    EXPECT_THAT(turns, Contains("D+e7").Times(1));
}

// Not included: Dionysus killing enemy at the gate allowing a double move.
// (In that case, just stay at the gate to win.)

TEST_F(MovesTest, Artemis_Moves) {
    TestMovement(LIGHT, ARTEMIS,
            "     .     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ......... "
            "  .......  "
            "   +++++   "
            "    +++    "
            "     T     "
    );

    TestMovement(LIGHT, ARTEMIS,
            "     .     "
            "    ...    "
            "   +++++   "
            "  .+++++.  "
            " ++++T++++ "
            "  .+++++.  "
            "   +++++   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, ARTEMIS,
            "     .     "
            "    ...    "
            "   ....+   "
            "  .....++  "
            " .+++++++T "
            "  .....++  "
            "   ....+   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, ARTEMIS,
            "     .     "
            "    ..+    "
            "   ...++   "
            "  ....+++  "
            " ++++++++T "
            "  ....++M  "
            "   ...++   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, ARTEMIS,
            "     .     "
            "    ...    "
            "   ....+   "
            "  .....++  "
            " ......+ZT "
            "  .....++  "
            "   ....+   "
            "    ...    "
            "     .     "
    );

    TestMovement(LIGHT, ARTEMIS,
            "     .     "
            "    ...    "
            "   ....+   "
            "  .....++  "
            " ...z++++T "
            "  .....++  "
            "   ....+   "
            "    ...    "
            "     .     "
    );
}

TEST_F(MovesTest, Artemis_Attacks) {
    TestAttack(LIGHT, ARTEMIS, {APOLLO, APHRODITE, POSEIDON},
            "     .     "
            "    ...    "
            "   ....z   "
            "  o......  "
            " ......... "
            "  ..Tm...  "
            "   a.p..   "
            "    ..e    "
            "     .     "
    );
}

// Withering Moon: deal 1 damage to any enemy and take 1 damage in return.
TEST_F(MovesTest, Artemis_Special) {
    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..z..   "
            "  .......  "
            " ....T..o. "
            "  ......n  "
            "   ..Z..   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);

    auto turns = TurnStrings();
    ASSERT_THAT(turns, Contains("T+e7"));
    ASSERT_THAT(turns, Not(Contains("T+e3")));  // ally
    ASSERT_THAT(turns, Not(Contains("T+h5")));  // protected by Athena

    ExecuteTurn("T+e7");

    EXPECT_EQ(hp(LIGHT, ARTEMIS), pantheon[ARTEMIS].hit - 1);
    EXPECT_EQ(hp(DARK, ZEUS), pantheon[ZEUS].hit - 1);
}

TEST_F(MovesTest, Artemis_SpecialAfterSummon) {
    Place(DARK, ATHENA, "c6");

    ExecuteTurn("T@e1,T+c6");

    EXPECT_EQ(hp(DARK, ATHENA), pantheon[ATHENA].hit - 1);
}

TEST_F(MovesTest, Artemis_SpecialDamageBoost) {
    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..z..   "
            "  .......  "
            " ...HT.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);
    EXPECT_EQ(fx(LIGHT, ARTEMIS), DAMAGE_BOOST);

    ExecuteTurn("T+e7");

    EXPECT_EQ(hp(LIGHT, ARTEMIS), pantheon[ARTEMIS].hit - 2);
    EXPECT_EQ(hp(DARK, ZEUS), pantheon[ZEUS].hit - 2);
}

// Edge case: when Artemis is at 1 HP herself and damage boosted
// by Hephaestus, Withering Moon still does 2 damage to the target.
TEST_F(MovesTest, Artemis_SpecialDamageBoostAt1HP) {
    state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..z..   "
            "  .......  "
            " ...HT.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);
    SetHp(LIGHT, ARTEMIS, 1);

    ExecuteTurn("T+e7");

    EXPECT_EQ(hp(DARK, ZEUS), pantheon[ZEUS].hit - 2);
    EXPECT_TRUE(IsDead(LIGHT, ARTEMIS));
}

// Killing an enemy at the opponent's gate with Artemis' special triggers
// an extra move, just like a regular attack would.
TEST_F(MovesTest, Artemis_SpecialExtraMove) {
    state = BoardTemplate(
            "     n     "
            "    ...    "
            "   .....   "
            "  .......  "
            " ....T.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);
    SetHp(DARK,ATHENA, 1);

    EXPECT_THAT(TurnStrings(), Contains("T+e9,T>e7"));
}

// Hades moves up to 3 spaces in any single direction (same as Ares).
TEST_F(MovesTest, Hades_Moves) {
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

TEST_F(MovesTest, Hades_Attacks) {
    state = BoardTemplate(
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

    ExecuteTurn("S!d5");

    // Note 4 damage instead of 3 because of Hephaestus damage boost
    EXPECT_EQ(hp(LIGHT, HEPHAESTUS),      9);  // friendly
    EXPECT_EQ(hp(DARK,  HERMES),          5);  // out of range
    EXPECT_EQ(hp(DARK,  POSEIDON),    7 - 4);  // hit
    EXPECT_EQ(hp(DARK,  APOLLO),      6 - 4);  // hit
    EXPECT_EQ(hp(DARK,  ATHENA),          0);  // killed
    EXPECT_EQ(hp(DARK,  ZEUS),       10 - 4);  // hit
    EXPECT_EQ(hp(DARK,  APHRODITE),       6);  // out of range
    EXPECT_EQ(hp(DARK,  DIONYSUS),        4);  // out of range
}

// Chains of Tartarus: bind 1 enemy per turn after landing next to or attacking
// an enemy so they cannot move or attack as Hades remains adjacent.
TEST_F(MovesTest, Hades_Special) {
    // This test case is essentially the example from the rulebook.
    state = BoardTemplate(
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

    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("S>d5"));
    EXPECT_THAT(turns, Contains("S>d5,S+c6"));
    EXPECT_THAT(turns, Contains("S>d5,S+d6"));
    EXPECT_THAT(turns, Not(Contains("S>d5,S+e6")));       // empty
    EXPECT_THAT(turns, Not(Contains("S>d5,S+f6")));       // not adjacent

    ExecuteTurn("S>d5,S+c6");  // chain Hera at c6

    EXPECT_EQ(fx(DARK, HERA), CHAINED);
    EXPECT_EQ(fx(DARK, ZEUS), UNAFFECTED);
    EXPECT_EQ(fx(DARK, HEPHAESTUS), UNAFFECTED);
    EXPECT_THAT(MoveDestinations(HERA), IsEmpty());
    EXPECT_THAT(MoveDestinations(ZEUS), Not(IsEmpty()));
    EXPECT_THAT(MoveDestinations(HEPHAESTUS), Not(IsEmpty()));
    EXPECT_THAT(AttackTargets(HERA), IsEmpty());
    EXPECT_THAT(AttackTargets(ZEUS), Contains(HADES));

    EndTurn();  // skip dark turn

    EXPECT_THAT(TurnStrings(), Not(Contains("S!d5,S+c6")));  // cannot chain Hera twice

    ExecuteTurn("S!d5,S+d6");  // attack + chain Zeus at d6

    EXPECT_EQ(hp(DARK, HERA),        8 - 3);
    EXPECT_EQ(hp(DARK, ZEUS),       10 - 3);
    EXPECT_EQ(hp(DARK, HEPHAESTUS),      9);
    EXPECT_EQ(fx(DARK, HERA), CHAINED);
    EXPECT_EQ(fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(fx(DARK, HEPHAESTUS), UNAFFECTED);
    EXPECT_THAT(MoveDestinations(HERA), IsEmpty());
    EXPECT_THAT(MoveDestinations(ZEUS), IsEmpty());
    EXPECT_THAT(MoveDestinations(HEPHAESTUS), Not(IsEmpty()));
    EXPECT_THAT(AttackTargets(HERA), IsEmpty());
    EXPECT_THAT(AttackTargets(ZEUS), IsEmpty());

    EndTurn();  // skip dark turn

    ExecuteTurn("S>c5");  // move does not change chains

    EXPECT_EQ(fx(DARK, HERA), CHAINED);
    EXPECT_EQ(fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(fx(DARK, HEPHAESTUS), UNAFFECTED);

    EndTurn();  // skip dark turn

    ExecuteTurn("S>e5,S+f6");  // move + chain Hephaestus at d6

    EXPECT_EQ(fx(DARK, HERA), UNAFFECTED);
    EXPECT_EQ(fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(fx(DARK, HEPHAESTUS), CHAINED);
    EXPECT_THAT(MoveDestinations(HERA), Not(IsEmpty()));
    EXPECT_THAT(MoveDestinations(ZEUS), IsEmpty());
    EXPECT_THAT(MoveDestinations(HEPHAESTUS), IsEmpty());
    EXPECT_THAT(AttackTargets(ZEUS), IsEmpty());
    EXPECT_THAT(AttackTargets(HEPHAESTUS), IsEmpty());
}

// Hades can chain an enemy when summoning, then move or attack and chain again,
// binding two enemies in one turn.
TEST_F(MovesTest, Hades_SpecialAfterSummon) {
    Place(DARK, ZEUS,       "d2");
    Place(DARK, HEPHAESTUS, "f2");

    auto turns = TurnStrings();
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

TEST_F(MovesTest, Hades_SpecialAfterMoveSummon) {
    Place(DARK,  ZEUS,       "d2");
    Place(DARK,  HEPHAESTUS, "f2");
    Place(LIGHT, ZEUS,       "e1");

    EXPECT_THAT(TurnStrings(), Contains("Z>e2,S@e1,S+d2,S!e1,S+f2"));
}

TEST_F(MovesTest, Hades_SpecialBeforeSummon) {
    Place(LIGHT, HADES, "e1");
    Place(DARK,  ZEUS,  "d2");

    ExecuteTurn("S>e2,S+d2,O@e1,O!d2");

    EXPECT_EQ(fx(DARK, ZEUS), CHAINED);
    EXPECT_EQ(hp(DARK, ZEUS), 10 - 3);  // 3 = 2 base + 1 Blazing Arrow bonus damage
}

// Special rule 3: if a player kills an enemy at their gate, then the player
// may move again; this is another way Hades can bind two enemies in one turn.
TEST_F(MovesTest, Hades_SpecialAfterKillingAtEnemyGate) {
    //     d e f
    // 9     n
    // 8   z S p
    // 7 . . . . .
    Place(LIGHT, HADES,    "e8");
    Place(DARK,  ZEUS,     "d8");
    Place(DARK,  POSEIDON, "f8");
    Place(DARK,  ATHENA,   "e9");

    ExecuteTurn("S!e8,S+d8,S>e7,S+f8");

    EXPECT_TRUE(IsDead(DARK, ATHENA));
    EXPECT_EQ(fx(DARK, ZEUS),     CHAINED);
    EXPECT_EQ(fx(DARK, POSEIDON), CHAINED);
    EXPECT_EQ(hp(DARK, ZEUS),      10 - 3);
    EXPECT_EQ(hp(DARK, POSEIDON),   7 - 3);

    ExecuteTurn("R@e9,R!e7");

    // If Hades gets killed, chained gods are released.
    EXPECT_TRUE(IsDead(LIGHT, HADES));
    EXPECT_EQ(fx(DARK, ZEUS),     UNAFFECTED);
    EXPECT_EQ(fx(DARK, POSEIDON), UNAFFECTED);
}

// Hades can chain Athena and her shielded allies
TEST_F(MovesTest, Hades_SpecialProtectedByAthena) {
    state = BoardTemplate(
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

    EXPECT_EQ(fx(DARK, ZEUS), SHIELDED);
    auto turns = TurnStrings();
    EXPECT_THAT(turns, Contains("S!e1,S+d2"));
    EXPECT_THAT(turns, Contains("S>e2,S+e3"));
}

// Aphrodite swapping with Hades allows him to chain another enemy,
// preserving chains to adjacent enemies (only).
TEST_F(MovesTest, Hades_SpecialAfterAphroditeSwaps) {
    state = BoardTemplate(
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

    Chain(DARK, APOLLO);
    Chain(DARK, POSEIDON);

    EXPECT_EQ(fx(DARK, ZEUS),     UNAFFECTED);
    EXPECT_EQ(fx(DARK, APOLLO),   CHAINED);
    EXPECT_EQ(fx(DARK, HERA),     UNAFFECTED);
    EXPECT_EQ(fx(DARK, POSEIDON), CHAINED);

    ExecuteTurn("A+e4,S+e6");

    EXPECT_EQ(fx(DARK, ZEUS),     CHAINED);
    EXPECT_EQ(fx(DARK, APOLLO),   CHAINED);
    EXPECT_EQ(fx(DARK, HERA),     UNAFFECTED);
    EXPECT_EQ(fx(DARK, POSEIDON), UNAFFECTED);
}

TEST_F(MovesTest, Athena_Moves) {
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

TEST_F(MovesTest, Athena_Attacks) {
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
TEST_F(MovesTest, Athena_Special) {
    state = BoardTemplate(
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

    EXPECT_TRUE (has_fx(LIGHT, ZEUS,    SHIELDED));
    EXPECT_TRUE (has_fx(LIGHT, ARTEMIS, SHIELDED));
    EXPECT_FALSE(has_fx(LIGHT, ATHENA,  SHIELDED));  // cannot shield self
    EXPECT_FALSE(has_fx(LIGHT, APOLLO,  SHIELDED));  // out of range

    ExecuteTurn("Z!e6");

    EXPECT_EQ(hp(LIGHT, ZEUS), 10);  // no damage taken
}

TEST_F(MovesTest, Moves_SixActions) {
    Place(LIGHT, HADES, "e1");
    Place(DARK, ZEUS, "d2");
    Place(DARK, HERA, "f2");
    Place(DARK, ATHENA, "e9");
    SetHp(DARK, ATHENA, 1);

    EXPECT_THAT(TurnStrings(), Contains("S>e2,S+d2,T@e1,T+e9,S>e3,S+f2"));
}
