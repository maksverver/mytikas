#include "state.h"
#include "moves.h"
#include "random.h"

#include <cassert>
#include <cctype>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <string_view>

namespace {

constexpr bool debug_encoding = true;

constexpr int wrap_columns = 80;

void PrintTurns(const std::vector<Turn> &turns) {
    if (debug_encoding) {
        for (Turn turn : turns) {
            std::string encoded = turn.ToString();
            std::optional<Turn> decoded = Turn::FromString(encoded);
            if (decoded != turn) {
                std::cerr << "encoded: " << encoded << "\ndecoded: ";
                if (decoded) {
                    std::cerr << decoded->ToString();
                } else {
                    std::cerr << "FAILED";
                }
                assert(decoded == turn); // will fail
            }
        }
    }

    std::vector<std::string> strings;
    int max_len = 0;
    for (size_t i = 0; i < turns.size(); ++i) {
        std::ostringstream oss;
        oss << std::setw(4) << i + 1 << ". " << turns[i];
        strings.push_back(oss.str());
        max_len = std::max(max_len, (int) strings.back().size());
    }
    const size_t space = 2;
    const size_t columns = std::max(size_t{1}, size_t{wrap_columns} / (max_len + space));
    const size_t rows = (strings.size() + (columns - 1)) / columns;
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < columns; ++c) {
            size_t i = rows*c + r;
            if (i >= strings.size()) break;
            if (c > 0) std::cout << std::setw(space) << ' ';
            std::cout << std::left << std::setw(max_len) << strings[i];
        }
        std::cout << '\n';
    }
}

void PrintState(const State &state) {
    std::cout <<
        "    God           Light side        Dark side\n"
        "--  ------------  ----------------  ----------------\n";
    for (int i = 0; i < GOD_COUNT; ++i) {
        std::cout
            << std::setw( 2) << std::right << i + 1 << "  "
            << std::setw(12) << std::left << pantheon[i].name;
        for (int p = 0; p < 2; ++p) {
            std::cout << "  " << static_cast<char>((p == 0 ? toupper : tolower)(pantheon[i].ascii_id)) << " ";
            int      hp = state.hp(AsPlayer(p), AsGod(i));
            field_t  fi = state.fi(AsPlayer(p), AsGod(i));
            StatusFx fx = state.fx(AsPlayer(p), AsGod(i));
            if (hp == 0) {
                std::cout << "--------------";
                assert(fi == -1 && fx == UNAFFECTED);
            } else {
                std::cout
                    << std::setw(2) << std::right << hp << " HP "
                    << std::setw(2) << FieldName(fi) << ' '
                    << static_cast<char>((fx & SHIELDED)     ? (p ? tolower : toupper)('N') : ' ')
                    << static_cast<char>((fx & SPEED_BOOST)  ? (p ? tolower : toupper)('M') : ' ')
                    << static_cast<char>((fx & DAMAGE_BOOST) ? (p ? tolower : toupper)('H') : ' ')
                    << static_cast<char>((fx & CHAINED)      ? (p ? toupper : tolower)('S') : ' ')
                    << ' ';
            }
        }
        std::cout << ' ' << pantheon[i].emoji_utf8 << '\n';
    }
    std::string encoded = state.Encode();
    std::cout << encoded << '\n';
    if (debug_encoding) {
        std::optional<State> decoded = State::Decode(encoded);
        if (decoded != state) {
            std::cerr << "encoded: " << encoded << "\ndecoded: ";
            if (decoded) {
                std::cerr << decoded->Encode();
            } else {
                std::cerr << "FAILED";
            }
            std::cerr << std::endl;
            abort();
        }
    }
    std::cout << "\n   ";
    for (int c = 0; c < BOARD_SIZE; ++c) {
        std::cout << ' ' << static_cast<char>('a' + c) << ' ';
    }
    std::cout << "\n\n";
    for (int r = BOARD_SIZE - 1; r >= 0; --r) {
        std::cout << ' ' << static_cast<char>('1' + r) << ' ';
        for (int c = 0; c < BOARD_SIZE; ++c) {
            int i = FieldIndex(r, c);
            if (i == -1) {
                std::cout <<  "   ";
            } else if (state.IsEmpty(i)) {
                std::cout <<  " . ";
            } else {
                std::cout
                    << ' '
                    << static_cast<char>(state.PlayerAt(i) == 0 ?
                            toupper(pantheon[state.GodAt(i)].ascii_id) :
                            tolower(pantheon[state.GodAt(i)].ascii_id))
                    << ' ';
            }
        }
        std::cout << ' ' << static_cast<char>('1' + r) << '\n';
    }
    std::cout << "\n    ";
    for (int c = 0; c < BOARD_SIZE; ++c) {
        std::cout << ' ' << static_cast<char>('a' + c) << ' ';
    }
    std::cout
        << "\n\n"
        <<  (state.NextPlayer() == LIGHT ? "Light" : "Dark")
        << " to move\n";
}

void PrintUsage() {
    std::cout <<
        "Usage: play <light> <dark> [<state>]\n"
        "Where light/dark is either 'user' or 'rand'\n";
}

enum PlayerType {
    PLAY_RAND,
    PLAY_USER
};

std::optional<PlayerType> ParsePlayerType(std::string_view sv) {
    if (sv == "r" || sv == "rand") return PLAY_RAND;
    if (sv == "u" || sv == "user") return PLAY_USER;
    return {};
}

std::mt19937_64 rng = InitializeRng();

}  // namespace

int main(int argc, char *argv[]) {
    // Parse command line arugments
    if (argc < 3 || argc > 4) {
        PrintUsage();
        return 1;
    }
    PlayerType player_type[2];
    State state;
    {
        int argi = 1;
        for (int p = 0; p < 2; ++p) {
            if (auto pt = ParsePlayerType(argv[argi])) {
                player_type[p] = *pt;
                ++argi;
            } else {
                std::cerr << "Failed to parse player type: " << argv[argi] << '\n';
                return 1;
            }
        }
        if (argi < argc) {
            if (auto s = State::Decode(argv[argi]); s) {
                state = *s;
            } else {
                std::cerr << "Failed to decode state: " << argv[argi] << '\n';
                return 1;
            }
            ++argi;
        } else {
            state = State::Initial();
        }
        assert(argi == argc);
    }

    // Play game
    while (!state.IsOver()) {
        PrintState(state);

        std::vector<Turn> turns = GenerateTurns(state);
        std::cout << '\n';
        PrintTurns(turns);
        std::cout << '\n';

        assert(!turns.empty());

        auto random_turn = [&]() -> const Turn& {
            std::uniform_int_distribution<> dist(0, turns.size() - 1);
            size_t i = dist(rng);
            std::cout << "Randomly selected: " << (i + 1) << ". " << turns[i] << "\n\n";
            return turns[i];
        };

        std::optional<Turn> turn;
        if (player_type[state.NextPlayer()] == PLAY_RAND) {
            turn = random_turn();
        } else {
            std::string line;
            while (!turn) {
                std::cout << "Move: " << std::flush;
                if (!std::getline(std::cin, line)) {
                    std::cerr << "End of input!\n";
                    break;
                }
                if (line == "?") {
                    turn = random_turn();
                    break;
                }
                for (size_t i = 0; i < turns.size(); ++i) {
                    std::ostringstream is;
                    is << i + 1;
                    std::ostringstream ts;
                    ts << turns[i];
                    if (is.str() == line || ts.str() == line) {
                        turn = turns[i];
                        break;
                    }
                }
                if (!turn) {
                    std::cerr << "Move not recognized!\n";
                }
            }
        }
        if (!turn) {
            std::cerr << "No move selected! Quitting.\n";
            break;
        }
        ExecuteTurn(state, *turn);
    }
    if (state.IsOver()) {
        PrintState(state);
        std::cout
            << "Winner: "
            << (state.NextPlayer() ? "light" : "dark")
            << '\n';
    }
}
