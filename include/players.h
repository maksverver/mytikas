#ifndef PLAYERS_H_INCLUDED
#define PLAYERS_H_INCLUDED

#include "state.h"
#include "moves.h"

class GamePlayer {
public:
    virtual ~GamePlayer() {};

    virtual std::optional<Turn> SelectTurn(const State &state) = 0;
};

GamePlayer *CreateCliPlayer();
GamePlayer *CreateRandomPlayer();

#endif  // ndef PLAYERS_H_INCLUDED
