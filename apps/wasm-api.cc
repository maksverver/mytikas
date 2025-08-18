// Provides an API to be used by the web frontend implemented in Javascript.

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "state.h"
#include "players.h"

#include <cstring>
#include <string_view>
#include <memory>

static std::string initial_state_cpp_string = State::InitialAllSummonable().Encode();

static char *copy_to_c_string(std::string_view sv) {
    size_t n = sv.size();
    char *res = (char*) malloc(n + 1);
    if (res) {
        memcpy(res, sv.data(), n);
        res[n] = '\0';
    }
    return res;
}

extern "C" {

// On the Javascript side, this can be retrieved with something like:
// UTF8ToString(Module.getValue(Module._initial_state_string, 'i8*'))
EMSCRIPTEN_KEEPALIVE const char *initial_state_string = initial_state_cpp_string.c_str();

EMSCRIPTEN_KEEPALIVE void *mytikas_alloc(size_t size) {
    return malloc(size);
}

EMSCRIPTEN_KEEPALIVE void mytikas_free(void *p) {
    free(p);
}

// Calculates the new state after executing the given turn.
//
// Note that the turn MUST be valid in the given state, which is currently not
// validated.
//
// The result must be freed with mytikas_free().
EMSCRIPTEN_KEEPALIVE char *mytikas_execute_turn(
    const char *state_string,
    const char *turn_string
) {
    auto state = State::Decode(state_string);
    if (!state) return nullptr;
    auto turn = Turn::FromString(turn_string);
    if (!turn) return nullptr;
    ExecuteTurn(*state, *turn);
    return copy_to_c_string(state->Encode());
}

// Returns a list of possible turns in the given valid state, as a
// zero-terminated list of zero-separated strings.
//
// For example, a list of three strings ("foo", "bar", and "baz") would be
// encoded as "foo\0bar\0baz\0\0", where the final empty string marks the end
// of the list.
//
// The result must be freed with mytikas_free().
EMSCRIPTEN_KEEPALIVE char *mytikas_generate_turns(
    const char *state_string
) {
    auto state = State::Decode(state_string);
    if (!state) return nullptr;
    std::string turn_strings;
    if (!state->IsOver()) {
        for (const Turn &turn : GenerateTurns(*state)) {
            turn_strings += turn.ToString();
            turn_strings += '\0';
        }
    }
    return copy_to_c_string(turn_strings);
}

// Returns an optimal turn in the given state, selected by an AI player.
//
// The result must be freed with mytikas_free().
EMSCRIPTEN_KEEPALIVE char *mytikas_choose_ai_turn(
    const char *state_string,
    const char *player_desc_string
) {
    auto state = State::Decode(state_string);
    if (!state) return nullptr;

    auto player_desc = ParsePlayerDesc(player_desc_string);
    if (!player_desc) return nullptr;

    std::unique_ptr<GamePlayer> player(CreatePlayerFromDesc(*player_desc));
    auto turn = player->SelectTurn(*state);
    if (!turn) return nullptr;

    return copy_to_c_string(turn->ToString());
}

}  // extern "C"

int main(int, char *[]) {}
