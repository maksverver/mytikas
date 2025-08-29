import './App.css'

import { useEffect, useId, useState } from 'react';
import GameComponent from './GameComponent.tsx';
import { HistoryComponent } from './HistoryComponent.tsx';
import { AugmentedState } from './AugmentedState.ts';
import { Action, partialTurnToString, Turn } from './game/turn.ts';
import { GameState } from './game/state.ts';
import type { GodValue } from './game/gods.ts';

const playerOptions: Record<string, {title: string, playerDesc: string|null}> = {
    human:    { title: 'Human',             playerDesc: null },
    random:   { title: 'Random',            playerDesc: 'random' },
    minimax1: { title: 'Minimax (depth 1)', playerDesc: 'minimax,max_depth=1' },
    minimax2: { title: 'Minimax (depth 2)', playerDesc: 'minimax,max_depth=2' },
    minimax3: { title: 'Minimax (depth 3)', playerDesc: 'minimax,max_depth=3' },
    minimax4: { title: 'Minimax (depth 4)', playerDesc: 'minimax,max_depth=4' },
};

const defaultPlayerOption = 'human';

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
    turns: readonly Turn[],
): [Action[], boolean] {
    let isComplete = false;
    const res = new Map<number, Action>();
    for (const turn of turns) {
        if (matchPartialTurn(partialTurn, turn.actions)) {
            const action = turn.actions[partialTurn.length];
            if (action == null) {
                isComplete = true;
            } else {
                res.set(action.encodeInt(), action);
            }
        }
    }
    return [Array.from(res.values()), isComplete];
}

function executePartialTurn(state: GameState, partialTurn: readonly Action[]): GameState {
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

type PlayerSelectPros = {
    light: string;
    dark: string;
    setLight: (s: string) => void;
    setDark:  (s: string) => void;
}

function PlayerSelectComponent({light, dark, setLight, setDark}: PlayerSelectPros) {
    const lightSelectId = useId();
    const darkSelectId  = useId();
    return (
        <div className="player-select">
            <label htmlFor={lightSelectId}>Light player</label>
            <label htmlFor={darkSelectId}>Dark player</label>
            <select id={lightSelectId} value={light} onChange={e => setLight(e.target.value)}>
                {Object.entries(playerOptions).map(([key, {title}]) =>
                    (<option key={key} value={key}>{title}</option>))}
            </select>
            <select id={darkSelectId} value={dark} onChange={e => setDark(e.target.value)}>
                {Object.entries(playerOptions).map(([key, {title}]) =>
                    (<option key={key} value={key}>{title}</option>))}
            </select>
        </div>
    );
}

type StateButtonsProps = {
    onSave?: () => void,
    onLoad?: () => void,
    onUndo?: () => void,
    onRedo?: () => void,
};

function StateButtonsComponent({onSave, onLoad, onUndo, onRedo}: StateButtonsProps) {
    return (
        <div className="state-buttons">
            <button title="Save" disabled={onSave == null} onClick={onSave}>💾</button>
            <button title="Load" disabled={onLoad == null} onClick={onLoad}>📂</button>
            <button title="Undo" disabled={onUndo == null} onClick={onUndo}>↩️</button>
            <button title="Redo" disabled={onRedo == null} onClick={onRedo}>↪️</button>
        </div>
    );
}

export default function App() {
    // Logic: for a given augmented state, we can either have a currently
    // selected turn, OR a partial turn being constructed (when it's the user's
    // turn to move), but not both.
    const [augmentedState, setAugmentedState] = useState(() => AugmentedState.fromInitialState());
    const [selectedTurn, setSelectedTurn] = useState<number|undefined>();

    // These are used to construct a turn, by first selecting a god in play,
    // then clicking on a destination field to create a move action, etc.
    // The details are handled by the GameComponent.
    const [partialTurn, setPartialTurn] = useState<readonly Action[]>([]);
    const [selectedGod, setSelectedGod] = useState<GodValue|undefined>();

    // These set whether the light/dark players are controlled by the human user or an AI.
    const [lightPlayer, setLightPlayer] = useState<string>(defaultPlayerOption);
    const [darkPlayer,  setDarkPlayer]  = useState<string>(defaultPlayerOption);

    const aiPlayer = [
        playerOptions[lightPlayer].playerDesc,
        playerOptions[darkPlayer].playerDesc,
    ];
    const aiMoveDelayMs = 500;

    const nextTurns = augmentedState.nextTurns;
    const playerDesc = aiPlayer[augmentedState.lastGameState.player];

    const gameIsOver = nextTurns.length === 0;
    const userEnabled = selectedTurn == null && playerDesc == null && !gameIsOver;

    const gameState =
        userEnabled ? executePartialTurn(augmentedState.lastGameState, partialTurn) :
        selectedTurn == null ? augmentedState.lastGameState : augmentedState.gameStates[selectedTurn];

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
        setAugmentedState(augmentedState.addTurn(new Turn(partialTurn)));
        setPartialTurn([]);
    }

    function handleSelect(god: GodValue | undefined) {
        return setSelectedGod(god === selectedGod ? undefined : god);
    }

    // Note: this does not update the undo/redo stacks!
    function changeState(newState: AugmentedState) {
        setAugmentedState(newState);
        setPartialTurn([]);
        setSelectedGod(undefined);
        setSelectedTurn(undefined);
    }

    // State button handlers.
    const canUndo = augmentedState.history.length > 0;
    const canRedo = false;  // TODO
    function handleSave() {
    }
    function handleLoad() {
    }
    function handleUndo() {
    }
    function handleRedo() {
    }

    // Play AI moves:
    useEffect(() => {
        if (playerDesc == null || nextTurns.length === 0) return;
        const nextTurn = augmentedState.lastGameState.chooseAiTurn(playerDesc);
        if (nextTurn == null) {
            console.error('AI move failed! stateString:', augmentedState.lastGameState.toString(), 'playerDesc:', playerDesc);
            alert('AI failed!');
            return;
        }
        // Add a small delay before moving.
        const timeoutId = setTimeout(() => {
            setAugmentedState(augmentedState.addTurn(nextTurn));
        }, aiMoveDelayMs);
        return () => clearTimeout(timeoutId);
    }, [playerDesc, augmentedState]);

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
                <StateButtonsComponent
                    onSave={handleSave}
                    onLoad={handleLoad}
                    onUndo={canUndo ? handleUndo : undefined}
                    onRedo={canRedo ? handleRedo : undefined}
                />
                <PlayerSelectComponent
                    light={lightPlayer}
                    dark={darkPlayer}
                    setLight={setLightPlayer}
                    setDark={setDarkPlayer}
                />
                <PartialTurnComponent
                    enabled={userEnabled}
                    turnString={partialTurnToString(partialTurn)}
                    onRestart={userEnabled && partialTurn.length > 0 ? restartTurn : undefined}
                    onFinish={userEnabled && partialTurnIsComplete ? finishTurn : undefined}
                />
                <HistoryComponent
                    state={augmentedState}
                    selected={selectedTurn}
                    setSelected={partialTurn.length === 0 ? setSelectedTurn : undefined}
                />
            </div>
        </div>
    );
}
