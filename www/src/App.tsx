import './App.css'

import { useEffect, useState } from 'react';
import GameComponent from './GameComponent.tsx';
import { HistoryComponent } from './HistoryComponent.tsx';
import { addTurnToAugmentedState, createAugmentedState } from './AugmentedState.ts';
import { Action, partialTurnToString } from './game/turn.ts';
import { GameState } from './game/state.ts';
import type { GodValue } from './game/gods.ts';

function matchPartialTurn(partialTurn: readonly Action[], turn: readonly Action[]) {
    if (partialTurn.length > turn.length) return false;
    for (let i = 0; i < partialTurn.length; ++i) {
        if (partialTurn[i].encodeInt() !== turn[i].encodeInt()) return false;
    }
    return true;
}

// Returns a list of next actions that are consistent with the given partial
// turn, and whether the current partial turn is itself a complete turn.
//
// For example, if turns = [[1,2],[1,2,3],[1,4,5],[6]] and partialTurn = [],
// then the result is [[1, 6], false], while if partialTurn = [1, 2], then the
// result is [[3], true].
function findNextActions(
    partialTurn: readonly Action[],
    turns: readonly Action[][]
): [Action[], boolean] {
    let isComplete = false;
    const res = new Map<number, Action>();
    for (const turn of turns) {
        if (matchPartialTurn(partialTurn, turn)) {
            const action = turn[partialTurn.length];
            if (action == null) {
                isComplete = true;
            } else {
                res.set(action.encodeInt(), action);
            }
        }
    }
    return [Array.from(res.values()), isComplete];
}

function executePartialTurn(state: GameState, partialTurn: Action[]): GameState {
    if (partialTurn.length === 0) return state;
    return state.executeActions(partialTurn);
}

type PartialTurnProps = {
    enabled: boolean;
    turnString: string;
    onRestart?: () => void,
    onFinish?: () => void,
};

function PartialTurnComponent({enabled, turnString, onRestart, onFinish} : PartialTurnProps) {
    return (
        <div className="partial-turn">
            <button disabled={onRestart == null} onClick={onRestart} title="Restart">⟲</button>
            <div className="turn-string">{enabled ? turnString : '\u00A0'}</div>
            <button disabled={onFinish == null} onClick={onFinish} title="Finish">✓</button>
        </div>
    )
}

export default function App() {
    // Logic: for a given augmented state, we can either have a currently
    // selected turn, OR a partial turn being constructed (when it's the user's
    // turn to move), but not both.
    const [augmentedState, setAugmentedState] = useState(() => createAugmentedState());
    const [selectedTurn, setSelectedTurn] = useState<number|undefined>();
    const [partialTurn, setPartialTurn] = useState<Action[]>([]);
    const [selectedGod, setSelectedGod] = useState<GodValue|undefined>();

    //const aiPlayer = ['rand', 'minimax,max_depth=2'];
    //const aiPlayer = ['rand', 'rand'];
    const aiPlayer = [null, 'rand'];
    //const aiPlayer = [null, null];
    const aiMoveDelayMs = 500;

    const lastGameState = augmentedState.gameStates.at(-1)!;
    const nextTurns = augmentedState.nextTurns;
    const playerDesc = aiPlayer[lastGameState.player];

    const userEnabled = selectedTurn == null && playerDesc == null;

    const gameState =
        userEnabled ? executePartialTurn(lastGameState, partialTurn) :
        selectedTurn == null ? lastGameState : augmentedState.gameStates[selectedTurn];

    // Find next possible actions that are consistent with the current partial turn.
    const [nextActions, partialTurnIsComplete] =
        userEnabled
            ? findNextActions(partialTurn, nextTurns)
            : [[], false];

    function addAction(action: Action) {
        setPartialTurn([...partialTurn, action]);
        setSelectedGod(undefined);
    }
    function restartTurn() {
        setPartialTurn([]);
    }
    function finishTurn() {
        setAugmentedState(addTurnToAugmentedState(augmentedState, partialTurn));
        setPartialTurn([]);
    }

    function handleSelect(god: GodValue | undefined) {
        return setSelectedGod(god === selectedGod ? undefined : god);
    }

    // Play AI moves:
    useEffect(() => {
        if (playerDesc == null || nextTurns.length === 0) return;
        const nextTurnString = lastGameState.chooseAiTurn(playerDesc);
        if (nextTurnString == null) {
            console.error('AI move failed! stateString:', lastGameState.toString(), 'playerDesc:', playerDesc);
            alert('AI failed!');
            return;
        }
        // Add a small delay before moving.
        const timeoutId = setTimeout(() => {
            setAugmentedState(addTurnToAugmentedState(augmentedState, nextTurnString));
        }, aiMoveDelayMs);
        return () => clearTimeout(timeoutId);
    }, [augmentedState]);

    return (
        <div className="mytikas-app">
            <GameComponent
                state={gameState}
                nextActions={nextActions}
                selectedGod={selectedGod}
                onSelect={userEnabled ? handleSelect : undefined}
                onAction={userEnabled ? addAction : undefined}
            />
            <div className="right-panel">
                <HistoryComponent
                    state={augmentedState}
                    selected={selectedTurn}
                    setSelected={partialTurn.length === 0 ? setSelectedTurn : undefined}
                />
                <PartialTurnComponent
                    enabled={userEnabled}
                    turnString={partialTurnToString(partialTurn)}
                    onRestart={userEnabled && partialTurn.length > 0 ? restartTurn : undefined}
                    onFinish={userEnabled && partialTurnIsComplete ? finishTurn : undefined}
                />
            </div>
        </div>
    );
}
