// AugmentedState combines a game state with full move history including
// intermediate states and parsed turns.

import { GameState } from "./game/state";
import { type Turn } from "./game/turn";

export type RedoState = {
    readonly turn: Turn;
    readonly gameState: GameState;
    // nextTurns will be recalculated
};

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
        this.nextTurns = nextTurns;
    }

    // Creates an augmented game state from the default initial state.
    static fromInitialState() {
        return this.fromGameState(GameState.initial());
    }

    // Creates an augmented game state from the given state with no history.
    // The given state MUST be valid; this function does not validate it.
    static fromGameState(state: GameState) {
        return new AugmentedState([state], [], state.generateTurns());
    }

    // Reconstructs the game state history from the given list of turns.
    // The initial state (if given) MUST be valid. Turns are validated one
    // by one and an exception is thrown if any turn is invalid.
    static fromTurnHistory(turns: readonly Turn[], gameState: GameState = GameState.initial()): AugmentedState {
        const gameStates = [gameState];
        const history = [];
        for (const turn of turns) {
            const validTurns = gameState.generateTurns();
            const validTurn = validTurns.find(validTurn => validTurn.equals(turn));
            if (validTurn == null) {
                throw new Error(`Given turn (${turn}) does not match any valid turn (${validTurns})`);
            }
            gameState = gameState.executeTurn(validTurn);
            gameStates.push(gameState);
            history.push(validTurn);
        }
        return new AugmentedState(gameStates, history, gameState.generateTurns());
    }

    addTurn(turn: Turn): AugmentedState {
        const nextGameState = this.lastGameState.executeTurn(turn);
        return new AugmentedState(
            [...this.gameStates, nextGameState],
            [...this.history, turn],
            nextGameState.generateTurns());
    }

    canUndo(): boolean {
        return this.gameStates.length > 1;
    }

    undoTurn(): [AugmentedState, RedoState] {
        if (!this.canUndo()) {
            throw new Error('Cannot undo from initial state!');
        }
        return [
            new AugmentedState(
                this.gameStates.slice(0, -1),
                this.history.slice(0, -1),
                this.gameStates.at(-2)!.generateTurns()),
            {
                turn: this.history.at(-1)!,
                gameState: this.gameStates.at(-1)!
            }
        ];
    }

    redoTurn(undoState: RedoState): AugmentedState {
        return new AugmentedState(
            [...this.gameStates, undoState.gameState],
            [...this.history, undoState.turn],
            undoState.gameState.generateTurns());
    }
};
