#include "cli.h"
#include "moves.h"
#include "players.h"
#include "state.h"

#include <cassert>
#include <iostream>
#include <sstream>

class CliPlayer : public GamePlayer {
public:
    CliPlayer() {}
    std::optional<Turn> SelectTurn(const State &state) override;
};

std::optional<Turn> CliPlayer::SelectTurn(const State &state) {
    std::vector<Turn> turns = GenerateTurns(state);
    assert(!turns.empty());
    // Note this sorts by internal structure, not string presentation.
    // This keeps Zeus as the first God, moves before attacks, etc.
    std::ranges::sort(turns);
    PrintTurns(turns);
    std::cout << '\n';

    std::string line;
    std::optional<Turn> res;
    while (!res) {
        std::cout << "Move: " << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cerr << "End of input!\n";
            break;
        }
        for (size_t i = 0; i < turns.size(); ++i) {
            std::ostringstream is;
            is << i + 1;
            std::ostringstream ts;
            ts << turns[i];
            if (is.str() == line || ts.str() == line) {
                res = turns[i];
                break;
            }
        }
        if (!res) {
            std::cerr << "Move not recognized!\n";
        }
    }
    return res;
};

GamePlayer *CreateCliPlayer() {
    return new CliPlayer();
}
