// Helper functions for the text-based command line interface.

#ifndef CLI_H_INCLUDED
#define CLI_H_INCLUDED

#include <state.h>
#include <moves.h>

#include <vector>

void PrintTurns(const std::vector<Turn> &turns);
void PrintDiff(const State &prev, const State &next);
void PrintState(const State &state);

#endif // ndef CLI_H_INCLUDED
