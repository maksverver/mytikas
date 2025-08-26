// AugmentedState combines a game state with full move history including
// intermediate states and parsed turns.

import { GameState } from "./game/state";
import { type Turn } from "./game/turn";

export type AugmentedState = {
    gameStates: GameState[],

    history: Turn[],

    // Possible turns in the *last* gamesate.
    nextTurns: Turn[],
};

function calculateNextTurns(state: AugmentedState): AugmentedState {
    const lastGameState = state.gameStates.at(-1)!;
    return {...state, nextTurns: lastGameState.generateTurns()};
}

export function createAugmentedState(gameState = GameState.initial()): AugmentedState {
    return calculateNextTurns({
        gameStates: [gameState],
        history: [],
        nextTurns: [],
    });
}

export function addTurnToAugmentedState(augmentedState: AugmentedState, turn: Turn): AugmentedState {
    const lastGameState = augmentedState.gameStates.at(-1)!;
    const nextGameState = lastGameState.executeTurn(turn);
    return calculateNextTurns({
        gameStates: [...augmentedState.gameStates, nextGameState],
        history: [...augmentedState.history, turn],
        nextTurns: [],
    });
}
