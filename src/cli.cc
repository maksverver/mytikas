#include "cli.h"

#include "state.h"
#include "moves.h"

#include <iostream>
#include <iomanip>

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

void PrintDiff(const State &prev, const State &next) {
    for (int p = 0; p < 2; ++p) {
        for (int g = 0; g < GOD_COUNT; ++g) {
            Player pl = AsPlayer(p);
            God gd = AsGod(g);
            field_t src = prev.fi(pl, gd);
            field_t dst = next.fi(pl, gd);
            if (src != dst) {
                if (src == -1) {
                    std::cout << PlayerName(p) << ' ' << GodName(g) << " was summoned.\n";
                    src = gate_index[p];
                }
                if (dst != -1) {
                    std::cout << PlayerName(p) << ' ' << GodName(g) << " moved from " << FieldName(src) << " to " << FieldName(dst) << ".\n";
                }
            }
            int hp_lost = prev.hp(pl, gd) - next.hp(pl, gd);
            if (hp_lost != 0) {
                std::cout << PlayerName(p) << ' ' << GodName(g) << " took " << hp_lost << " damage\n";
            }
            if (src != -1 && dst == -1) {
                std::cout << PlayerName(p) << ' ' << GodName(g) << " died.\n";
            } else {
                int old_fx = prev.fx(pl, gd);
                int new_fx = next.fx(pl, gd);
                for (int i = 0; i < 4; ++i) {
                    const char *effect_names[4] = {"chain", "damage boost", "speed boost", "invincibility"};
                    bool before = old_fx & (1 << i);
                    bool after  = new_fx & (1 << i);
                    if (before != after) {
                        std::cout << PlayerName(p) << ' ' << GodName(g) << ' ' << (after ? "gained" : "lost") << ' ' << effect_names[i] << ".\n";
                    }
                }
            }
        }
    }
}

void PrintState(const State &state) {
    static const char *dir_chars[8] = {" ", "+", "×", "🞶", "L", "?", "?", "?"};

    std::cout <<
        "    God          Mov Atk Dmg Light side        Dark side\n"
        "--  ------------ --- --- --- ----------------  ----------------\n";
    for (int i = 0; i < GOD_COUNT; ++i) {
        const GodInfo &info = pantheon[i];

        std::cout
            << std::setw( 2) << std::right << i + 1 << "  "
            << std::setw(12) << std::left << info.name;

        std::cout
            << ' '
            << int{info.mov}
            << dir_chars[info.mov_dirs & 7]
            << ((info.mov_dirs & Dirs::DIRECT) ? "/" : " ")
            << ' '
            << int{info.rng}
            << dir_chars[info.atk_dirs & 7]
            << ((info.atk_dirs & Dirs::DIRECT) ? "/" : " ")
            << ' '
            << std::setw(2) << std::right << int{info.dmg};

        for (int p = 0; p < 2; ++p) {
            std::cout << "  " << static_cast<char>((p == 0 ? toupper : tolower)(info.ascii_id)) << " ";
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
        std::cout << ' ' << info.emoji_utf8 << '\n';
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
    std::cout << "\n   ";
    for (int c = 0; c < BOARD_SIZE; ++c) {
        std::cout << ' ' << static_cast<char>('a' + c) << ' ';
    }
    std::cout
        << "\n\n"
        <<  (state.NextPlayer() == LIGHT ? "Light" : "Dark")
        << " to move\n";
}
