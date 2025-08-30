import './App.css'

import { useEffect, useId, useRef, useState } from 'react';
import GameComponent from './GameComponent.tsx';
import { HistoryComponent } from './HistoryComponent.tsx';
import { AugmentedState, type RedoState } from './AugmentedState.ts';
import { Action, formatCompactTurnHistory, formatTurnHistory, parseCompactTurnHistory, parseTurnHistory, partialTurnToString, Turn } from './game/turn.ts';
import { decodeStateString, GameState } from './game/state.ts';
import type { GodValue } from './game/gods.ts';
import { classNames } from './utils.ts';

// Attempts to parse the given string as either a game state string, or
// a full turn history string, as shown in the save dialog.
function parseAgumentedState(s: string): AugmentedState|undefined {
    let e1, e2, e3;

    try {
        const turns = parseTurnHistory(s);
        if (turns != null) {
            return AugmentedState.fromTurnHistory(turns);
        }
    } catch (e) {
        e1 = e;
    }

    try {
        const turns = parseCompactTurnHistory(s);
        if (turns != null) {
            return AugmentedState.fromTurnHistory(turns);
        }
    } catch (e) {
        e2 = e;
    }

    try {
        let gameState: GameState = decodeStateString(s);
        if (gameState != null) {
            return AugmentedState.fromGameState(gameState);
        }
    } catch (e) {
        e3 = e;
    }

    // Wasn't able to parse the string in any way.
    console.info('Tried to parse game state from string', s);
    console.info('Could not parse string as verbose turn history', e1);
    console.info('Could not parse string as compact turn history', e2);
    console.info('Could not parse string as game state', e3);
    return undefined;
}

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
    finishHint: boolean;
};

function PartialTurnComponent({enabled, turnString, onRestart, onFinish, finishHint} : PartialTurnProps) {
    return (
        <div className="partial-turn">
            <button
                disabled={onRestart == null}
                onClick={onRestart}
                title="Restart"
            >⟲</button>
            <div
                className="turn-string"
            >{enabled ? turnString : '\u00A0'}</div>
            <button
                className={classNames({flash: finishHint})}
                disabled={onFinish == null}
                onClick={onFinish}
                title="Finish"
            >✓</button>
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

    const [redoStack, setRedoStack] = useState<RedoState[]>([]);

    const [showSaveDialog, setShowSaveDialog] = useState<boolean>(false);
    const saveDialogRef = useRef<HTMLDialogElement>(null);

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


    function changeState(newAugmentedState: AugmentedState, newRedoStack: RedoState[] = []) {
        setAugmentedState(newAugmentedState);
        setPartialTurn([]);
        setSelectedGod(undefined);
        setSelectedTurn(undefined);
        setRedoStack(newRedoStack);
    }

    function executeTurn(turn: Turn) {
        changeState(augmentedState.addTurn(turn));
    }

    function handleSelect(god: GodValue | undefined) {
        return setSelectedGod(god === selectedGod ? undefined : god);
    }

    // State button handlers.
    function addAction(action: Action) {
        setPartialTurn([...partialTurn, action]);
        setSelectedGod(undefined);
    }
    function restartTurn() {
        setPartialTurn([]);
    }
    function finishTurn() {
        executeTurn(new Turn(partialTurn));
        setPartialTurn([]);
    }

    // For undoing/redoing, we move to the previous/next state where it was the
    // user's turn to move, otherwise the AI would just immediately move again.
    const canUndo = augmentedState.gameStates.some(s => aiPlayer[s.player] == null);
    const canRedo = redoStack.some(s => aiPlayer[s.gameState.player] == null);

    function handleUndo() {
        let newAugmentedState = augmentedState;
        let newRedoStack = Array.from(redoStack);
        let redoState: RedoState;
        while (newAugmentedState.canUndo) {
            [newAugmentedState, redoState] = newAugmentedState.undoTurn();
            newRedoStack.push(redoState);
            if (aiPlayer[newAugmentedState.lastGameState.player] == null) break;
        }
        changeState(newAugmentedState);
        setRedoStack(newRedoStack);
    }

    function handleRedo() {
        let newAugmentedState = augmentedState;
        let newRedoStack = Array.from(redoStack);
        while (newRedoStack.length > 0) {
            newAugmentedState = newAugmentedState.redoTurn(newRedoStack.pop()!);
            if (aiPlayer[newAugmentedState.lastGameState.player] == null) break;
        }
        changeState(newAugmentedState);
        setRedoStack(newRedoStack);
    }

    function handleSave() {
        setShowSaveDialog(true);
    }

    function hideSaveDialog() {
        setShowSaveDialog(false);
    }

    function handleLoad() {
        let s = prompt("Enter game state or move history");
        if (s == null) return;  // dialog closed
        s = s.replace(/\s+/g,'');  // strip whitespace
        const newAugmentedState = parseAgumentedState(s);
        if (newAugmentedState == null) {
            alert('Failed to parse game state string! (See Javascript log for details.)');
            return;
        }
        changeState(newAugmentedState);
    }

    // Open/close save dialog
    useEffect(() => {
        if (!saveDialogRef.current?.open && showSaveDialog) {
            saveDialogRef.current!.showModal()
        } else if (saveDialogRef.current?.open && !showSaveDialog) {
            saveDialogRef.current!.close();
        }
    }, [showSaveDialog]);

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
            executeTurn(nextTurn);
        }, aiMoveDelayMs);
        return () => clearTimeout(timeoutId);
    }, [playerDesc, augmentedState]);

    const gameStateString = augmentedState.lastGameState.toString();
    const turnHistoryString = formatTurnHistory(augmentedState.history);
    const compactTurnHistoryString = formatCompactTurnHistory(augmentedState.history);

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
                    finishHint={userEnabled && nextActions.length === 0}
                />
                <HistoryComponent
                    state={augmentedState}
                    selected={selectedTurn}
                    setSelected={partialTurn.length === 0 ? setSelectedTurn : undefined}
                />
                <dialog className="save-dialog" ref={saveDialogRef} onClick={hideSaveDialog}>
                    <div className="content" onClick={e => e.stopPropagation()}>
                        <div className="close" onClick={hideSaveDialog}>✖</div>
                        <h1>Save state</h1>
                        <h2>Final game state</h2>
                        <div>
                            <span className="code">{gameStateString}</span>
                        </div>
                        <h2>Compact turn history</h2>
                        <div>
                            <span className="code">{compactTurnHistoryString}</span>
                        </div>
                        <h2>Verbose turn history</h2>
                        <div>
                            <span className="code">{turnHistoryString}</span>
                        </div>
                    </div>
                </dialog>
            </div>
        </div>
    );
}
