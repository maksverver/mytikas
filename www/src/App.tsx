import { useEffect, useState } from 'react';
import GameComponent from './GameComponent.tsx';
import { decodeStateString, type GameState } from './game/state.ts';
import { executeTurn, generateTurns, initialStateString } from './wasm-api.ts';

const initialState = decodeStateString(initialStateString);
if (initialState == null) throw new Error('Invalid initial state');

type AugmentedState = {
    stateString: string,
    gameState: GameState,
    turnStrings: string[],
};

function augmentState(stateString: string): AugmentedState {
    const gameState = decodeStateString(stateString);
    const turnStrings = generateTurns(stateString);
    if (gameState == null || turnStrings == null) {
        console.error('Invalid state string:', stateString);
        throw new Error('Invalid state string!');
    }
    return {stateString, gameState, turnStrings};
}

function chooseRandomly<T>(values: T[]): T {
    if (values.length === 0) {
        throw new Error('Cannot choose from an empty list!');
    }
    return values[Math.floor(Math.random() * values.length)];
}

export default function App() {
    const [augmentedState, setAugmentedState] = useState(() => augmentState(initialStateString));

    // Play random moves automatically:
    useEffect(() => {
        if (augmentedState.turnStrings.length > 0) {
            const timeoutId = setTimeout(() => {
                setAugmentedState(
                    augmentState(
                        executeTurn(
                            augmentedState.stateString,
                            chooseRandomly(augmentedState.turnStrings))!));
            }, 1000);
            return () => clearTimeout(timeoutId);
        }
    }, [augmentedState]);

    return (
        <GameComponent state={augmentedState.gameState} />
    );
}
