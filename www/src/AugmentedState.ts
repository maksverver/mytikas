// AugmentedState combines a game state with full move history including
// intermediate states and parsed turns.

import { GameState } from "./game/state";
import { type Turn } from "./game/turn";

export class AugmentedState {
    // All game states in order; contains at least 1 element (the initial state).
    readonly gameStates: readonly GameState[];

    // Turns that were played; history.length === gameStates.length - 1.
    // The i-th turn was played in state gameStates[i] resulting in gameStates[i + 1].
    readonly history: readonly Turn[];

    // List of turns that are possible in the latest gameState, generated with
    // gameStates.at(-1).generateTurns().
    readonly nextTurns: readonly Turn[];

    get lastGameState(): GameState {
        return this.gameStates.at(-1)!;
    }

    // Don't call this directly; use one of the static factory methods.
    constructor(
        gameStates: readonly GameState[],
        history: readonly Turn[],
        nextTurns: readonly Turn[],
    ) {
        this.gameStates = gameStates;
        this.history = history;
        this.nextTurns = nextTurns;;
    }

    static fromGameState(state: GameState) {
        return new AugmentedState([state], [], state.generateTurns());
    }

    static fromInitialState() {
        return this.fromGameState(GameState.initial());
    }

    addTurn(turn: Turn): AugmentedState {
        const nextGameState = this.lastGameState.executeTurn(turn);
        return new AugmentedState(
            [...this.gameStates, nextGameState],
            [...this.history, turn],
            nextGameState.generateTurns());
    }
};
