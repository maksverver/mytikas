#include "doctest.h"

#include "state.h"
#include "moves.h"

#include <iostream>
#include <cassert>
#include <cctype>
#include <sstream>
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

/*
State StateWithGod(God god, Player player=LIGHT) {
    bool gods[2][GOD_COUNT] = {};
    gods[player][god] = true;
    return State::InitialWithGods(gods);
}
*/

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

void RemoveSpaces(std::string &s) {
    size_t j = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!isspace(s[i])) s[j++] = s[i];
    }
    s.resize(j);
}

State StateFromTemplate(Player player, std::string s) {
    RemoveSpaces(s);
    assert(s.size() == FIELD_COUNT);

    struct Place {
        Player player;
        God god;
        field_t field;
    };
    std::vector<Place> places;
    for (field_t field = 0; field < FIELD_COUNT; ++field) {
        char ch = s[field];
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

void TestMovement(Player player, God god, std::string s) {
    RemoveSpaces(s);
    assert(s.size() == FIELD_COUNT);

    State state = StateFromTemplate(player, s);

    std::vector<field_t> expected;
    for (field_t field = 0; field < FIELD_COUNT; ++field) {
        if (s[field] == '+') expected.push_back(field);
    }

    std::vector<field_t> received = MoveDestinations(state, god);
    std::ranges::sort(received);

    CHECK_EQ(expected, received);
}

void TestAttack(Player player, God god, std::vector<God> expected, std::string s) {
    State state = StateFromTemplate(player, std::move(s));
    std::vector<God> received = AttackTargets(state, god);
    std::ranges::sort(expected);
    std::ranges::sort(received);
    CHECK_EQ(expected, received);
}

}  // namespace

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

// TODO: hephaestus special!

// TODO: special rule 1
// TODO: special rule 2
// TODO: special rule 3

// TODO: hera, poseidon, apollo, aphrodite, ares, hermes, dionysus, artemis, hades, athena
