import './App.css'

import { useEffect, useState } from 'react';
import GameComponent from './GameComponent.tsx';
import { HistoryComponent } from './HistoryComponent.tsx';
import { chooseRandomly } from './utils.ts';
import { addTurnToAugmentedState, createAugmentedState } from './AugmentedState.ts';
import { chooseAiTurn } from './wasm-api.ts';

export default function App() {
    const [augmentedState, setAugmentedState] = useState(() => createAugmentedState());

    const aiPlayer = ['rand', 'minimax,max_depth=2'];

    // Play random moves automatically:
    useEffect(() => {
        if (augmentedState.nextTurns.length === 0) return;
        const [stateString, state] = augmentedState.gameStates.at(-1)!;
        const nextPlayer = state.player;
        const playerDesc = aiPlayer[nextPlayer];
        if (playerDesc == null) return;
        const nextTurnString = chooseAiTurn(stateString, playerDesc);
        if (nextTurnString == null) {
            console.error('AI move failed! stateString:', stateString, 'playerDesc:', playerDesc);
            alert('AI failed!');
            return;
        }
        // Add a small delay before moving.
        const timeoutId = setTimeout(() => {
            setAugmentedState(addTurnToAugmentedState(augmentedState, nextTurnString));
        }, 1000);
        return () => clearTimeout(timeoutId);

    }, [augmentedState]);

    const [selectedTurn, setSelectedTurn] = useState<number|undefined>();

    return (
        <div className="mytikas-app">
            <GameComponent
                state={augmentedState.gameStates.at(selectedTurn ?? -1)![1]}
            />
            <HistoryComponent
                state={augmentedState}
                selected={selectedTurn}
                onSelect={(i) => {
                    setSelectedTurn(i === selectedTurn ? undefined : i);
                }}
            />
        </div>
    );
}
