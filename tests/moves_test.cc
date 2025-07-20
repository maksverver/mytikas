#include "doctest.h"

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

namespace doctest {
template<> struct StringMaker<std::vector<field_t>> {
    static String convert(const std::vector<field_t>& v) {
        using ::operator<<;
        std::ostringstream oss;
        oss << v;
        return oss.str().c_str();
    }
};
}  // namespace doctest

namespace {

std::optional<Turn> FindTurn(const State &state, std::string_view sv) {
    for (const Turn &turn : GenerateTurns(state)) {
        std::cerr << "? " << turn.ToString() << " <> " << sv <<'\n';
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
    return FieldIndex(8 - r, c);
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
        assert(init_gods[player][god] == false);
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

    CHECK_EQ(expected, received);
}

void TestAttack(Player player, God god, std::vector<God> expected, const BoardTemplate &t) {
    State state = t.ToState(player);
    std::vector<God> received = AttackTargets(state, god);
    std::ranges::sort(expected);
    std::ranges::sort(received);
    CHECK_EQ(expected, received);
}

}  // namespace

TEST_CASE("status effects") {
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
    CHECK(state.fx(LIGHT, N) == UNAFFECTED);
    CHECK(state.fx(LIGHT, Z) == SHIELDED);
    CHECK(state.fx(LIGHT, P) == UNAFFECTED);
    CHECK(state.fx(DARK,  Z) == UNAFFECTED);

    // Ally moving away removes status:
    state.Move(LIGHT, Z, ParseField("c3"));
    CHECK(state.fx(LIGHT, N) == UNAFFECTED);
    CHECK(state.fx(LIGHT, Z) == UNAFFECTED);
    CHECK(state.fx(LIGHT, P) == UNAFFECTED);
    CHECK(state.fx(DARK,  Z) == UNAFFECTED);

    // Ally moving towards adds status:
    state.Move(LIGHT, P, ParseField("f2"));
    CHECK((int)state.fx(LIGHT, N) == (int)UNAFFECTED);
    CHECK(state.fx(LIGHT, Z) == UNAFFECTED);
    CHECK(state.fx(LIGHT, P) == SHIELDED);
    CHECK(state.fx(DARK,  Z) == UNAFFECTED);

    // Moving towards ally adds status, way from ally removes status:
    state.Move(LIGHT, N, ParseField("d2"));
    CHECK(state.fx(LIGHT, N) == UNAFFECTED);
    CHECK(state.fx(LIGHT, Z) == SHIELDED);
    CHECK(state.fx(LIGHT, P) == UNAFFECTED);
    CHECK(state.fx(DARK,  Z) == UNAFFECTED);

    // Removing god removes status.
    state.Remove(LIGHT, N);
    CHECK(state.fx(LIGHT, N) == UNAFFECTED);
    CHECK(state.fx(LIGHT, Z) == UNAFFECTED);
    CHECK(state.fx(LIGHT, P) == UNAFFECTED);
    CHECK(state.fx(DARK,  Z) == UNAFFECTED);
}

TEST_CASE("overlapping status effects") {
    State state = State::Initial();
    state.Place(LIGHT, ZEUS,       ParseField("e1"));
    state.Place(LIGHT, HEPHAESTUS, ParseField("d2"));
    state.Place(LIGHT, ATHENA,     ParseField("e2"));
    state.Place(LIGHT, HERMES,     ParseField("f2"));

    CHECK(state.fx(LIGHT, ZEUS)       == (DAMAGE_BOOST | SPEED_BOOST | SHIELDED));
    CHECK(state.fx(LIGHT, HEPHAESTUS) == SHIELDED);
    CHECK(state.fx(LIGHT, ATHENA)     == (DAMAGE_BOOST | SPEED_BOOST));
    CHECK(state.fx(LIGHT, HERMES)     == SHIELDED);

    state.Remove(LIGHT, ATHENA);

    CHECK(state.fx(LIGHT, ZEUS)       == (DAMAGE_BOOST | SPEED_BOOST));
    CHECK(state.fx(LIGHT, HEPHAESTUS) == UNAFFECTED);
    CHECK(state.fx(LIGHT, ATHENA)     == UNAFFECTED);
    CHECK(state.fx(LIGHT, HERMES)     == UNAFFECTED);
}

TEST_CASE("zeus moves") {
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

TEST_CASE("zeus attacks") {
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

TEST_CASE("zeus special") {
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

TEST_CASE("hephaestus moves") {
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

TEST_CASE("hephaestus attacks") {
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

TEST_CASE("hephaestus special") {
    State state = BoardTemplate(
            "     .     "
            "    ...    "
            "   ..a..   "
            "  .......  "
            " ...HR.... "
            "  .......  "
            "   .....   "
            "    ...    "
            "     .     "
        ).ToState(LIGHT);
    std::optional<Turn> turn = FindTurn(state, "R!e7");
    REQUIRE(turn);
    ExecuteTurn(state, *turn);
    CHECK(state.hp(DARK, APHRODITE) == 0);
}

// TODO: special rule 1
// TODO: special rule 2
// TODO: special rule 3

// TODO: hera, poseidon, apollo, aphrodite, ares, hermes, dionysus, artemis, hades, athena
