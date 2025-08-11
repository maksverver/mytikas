#ifndef PLAYERS_H_INCLUDED
#define PLAYERS_H_INCLUDED

#include "state.h"
#include "moves.h"

class GamePlayer {
public:
    virtual ~GamePlayer() {};

    virtual std::optional<Turn> SelectTurn(const State &state) = 0;
};

enum PlayerType {
    PLAY_RAND,
    PLAY_CLI,
    PLAY_MINIMAX,
    PLAY_MCTS
};

struct RandomPlayerOpts {
};

struct CliPlayerOpts {
};

struct MinimaxPlayerOpts {
    int max_depth = 0;  // use default
    bool experiment = false;
};

struct MctsPlayerOpts {
};

struct PlayerDesc {
    PlayerType type;
    union {
        RandomPlayerOpts   random;
        CliPlayerOpts      cli;
        MinimaxPlayerOpts  minimax;
        MctsPlayerOpts     mcts;
    } opts;
};

std::optional<PlayerDesc> ParsePlayerDesc(std::string_view sv);

GamePlayer *CreatePlayerFromDesc(const PlayerDesc &desc);

GamePlayer *CreateRandomPlayer(const RandomPlayerOpts &opts);
GamePlayer *CreateCliPlayer(const CliPlayerOpts &opts);
GamePlayer *CreateMinimaxPlayer(const MinimaxPlayerOpts &opts);
GamePlayer *CreateMctsPlayer(const MctsPlayerOpts &opts);

#endif  // ndef PLAYERS_H_INCLUDED
