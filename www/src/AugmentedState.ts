// AugmentedState combines a game state with full move history including
// intermediate states and parsed turns.

import { decodeStateString, type GameState } from "./game/state";
import { parseTurnString, parseTurnStrings, type Turn } from "./game/turn";
import { zip2 } from "./utils";
import { executeTurn, generateTurns, initialStateString } from "./wasm-api";

export type AugmentedState = {
    gameStates: [string, GameState][],

    history: [string, Turn][],

    // Possible turns in the *last* gamesate.
    nextTurns: [string, Turn][],
};

function calculateNextTurns(state: AugmentedState): AugmentedState {
    const stateString = state.gameStates.at(-1)![0];
    const nextTurnStrings = generateTurns(stateString);
    if (nextTurnStrings == null) {
        console.error('Invalid state string:', stateString);
        throw new Error('Invalid state string!');
    }
    const nextTurns = parseTurnStrings(nextTurnStrings);
    return {...state, nextTurns: zip2(nextTurnStrings, nextTurns)};
}

export function createAugmentedState(stateString: string = initialStateString): AugmentedState {
    const state = decodeStateString(stateString);
    if (state == null) {
        console.error('Invalid state string:', stateString);
        throw new Error('Invalid state string!');
    }
    return calculateNextTurns({
        gameStates: [[stateString, state]],
        history: [],
        nextTurns: [],
    });
}

export function addTurnToAugmentedState(augmentedState: AugmentedState, turnString: string): AugmentedState {
    const oldStateString = augmentedState.gameStates.at(-1)![0];
    const newStateString = executeTurn(oldStateString, turnString);
    const turn = parseTurnString(turnString);
    if (newStateString == null || turn == null) {
        console.error('Invalid turn string:', turnString);
        throw new Error('Invalid turn string!');
    }
    const newGameState = decodeStateString(newStateString);
    if (newGameState == null) {
        console.error('Invalid state string:', newStateString);
        throw new Error('Invalid state string!');
    }
    return calculateNextTurns({
        gameStates: [...augmentedState.gameStates, [newStateString, newGameState]],
        history: [...augmentedState.history, [turnString, turn]],
        nextTurns: [],
    });
}
