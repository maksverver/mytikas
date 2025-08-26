// AugmentedState combines a game state with full move history including
// intermediate states and parsed turns.

import { decodeStateString, type GameState } from "./game/state";
import { parseTurnString, parseTurnStrings, type Turn } from "./game/turn";
import { executeTurn, generateTurns, initialStateString } from "./wasm-api";

export type AugmentedState = {
    gameStates: GameState[],

    history: Turn[],

    // Possible turns in the *last* gamesate.
    nextTurns: Turn[],
};

function calculateNextTurns(state: AugmentedState): AugmentedState {
    const stateString = state.gameStates.at(-1)!.toString();
    const nextTurnStrings = generateTurns(stateString);
    if (nextTurnStrings == null) {
        console.error('Invalid state string:', stateString);
        throw new Error('Invalid state string!');
    }
    return {...state, nextTurns: parseTurnStrings(nextTurnStrings)};
}

export function createAugmentedState(stateString: string = initialStateString): AugmentedState {
    const state = decodeStateString(stateString);
    if (state == null) {
        console.error('Invalid state string:', stateString);
        throw new Error('Invalid state string!');
    }
    return calculateNextTurns({
        gameStates: [state],
        history: [],
        nextTurns: [],
    });
}

export function addTurnToAugmentedState(augmentedState: AugmentedState, turnString: string): AugmentedState {
    const oldStateString = augmentedState.gameStates.at(-1)!.toString();
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
        gameStates: [...augmentedState.gameStates, newGameState],
        history: [...augmentedState.history, turn],
        nextTurns: [],
    });
}
