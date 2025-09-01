import './App.css'

import { useCallback, useEffect, useId, useRef, useState } from 'react';
import GameComponent from './GameComponent.tsx';
import { HistoryComponent } from './HistoryComponent.tsx';
import { AugmentedState, type RedoState } from './AugmentedState.ts';
import { Action, formatCompactTurnHistory, formatTurnHistory, parseCompactTurnHistory, parseTurnHistory, partialTurnToString, Turn } from './game/turn.ts';
import { decodeStateString, GameState } from './game/state.ts';
import type { GodValue } from './game/gods.ts';
import { classNames } from './utils.ts';

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

// Attempts to parse the given string as either a game state string, or
// a full turn history string, as shown in the save dialog.
function parseAugmentedState(s: string): AugmentedState|undefined {
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
        const gameState: GameState = decodeStateString(s);
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
    onHistory?: () => void,
    historyFlash: boolean,
};

function StateButtonsComponent({onSave, onLoad, onUndo, onRedo, onHistory, historyFlash}: StateButtonsProps) {
    return (
        <div className="state-buttons">
            <button title="Save" disabled={onSave == null} onClick={onSave}>💾</button>
            <button title="Load" disabled={onLoad == null} onClick={onLoad}>📂</button>
            <button title="Undo" disabled={onUndo == null} onClick={onUndo}>↩️</button>
            <button title="Redo" disabled={onRedo == null} onClick={onRedo}>↪️</button>
            <button title="History"
                className={classNames({flash: historyFlash})}
                disabled={onHistory == null} onClick={onHistory}
            >📜</button>
        </div>
    );
}

function CopyableStringComponent({desc, text}: {desc: string, text: string}) {
    const handleClick = () => {
        if (navigator?.clipboard?.writeText == null) {
            alert(`❌ Error: could not copy ${desc} to clipboard!

navigator.clipboard.writeText() is not defined; this may happen when running outside a secure context.`);
        }
        navigator.clipboard.writeText(text)
            .then(() => {
                alert(`✅ Copied ${desc} to clipboard.`);
            })
            .catch((e) => {
                alert(`❌ Error: could not copy ${desc} to clipboard!\n\n${e}`);
            });
    }
    return (
        <div className="copyable-string">
            <button className="copy" title="Copy to clipboard" onClick={handleClick}>📋</button>
            <span className="code">{text}</span>
        </div>
    );
}

type SaveStateProps = {
    visible: boolean,
    augmentedState: AugmentedState,
    onClose?: () => void,
};

function SaveStateComponent({visible, augmentedState, onClose}: SaveStateProps) {
    const dialogRef = useRef<HTMLDialogElement>(null);

    // Open/close save dialog
    useEffect(() => {
        const dialog = dialogRef.current;
        if (dialog == null) return;
        if (visible) {
            if (!dialog.open) dialog.showModal();
        } else {
            if (dialog.open) dialog.close();
        }
    }, [visible]);

    const gameStateString = augmentedState.lastGameState.toString();
    const turnHistoryString = formatTurnHistory(augmentedState.history);
    const compactTurnHistoryString = formatCompactTurnHistory(augmentedState.history);

    return (
        <dialog
            className="save-dialog"
            ref={dialogRef}
            onClick={e => e.currentTarget.close()}
            onClose={onClose}
        >
            <div className="content" onClick={e => e.stopPropagation()}>
                <form method="dialog">
                    <button className="close">×</button>
                </form>
                <h1>Save state</h1>
                <h2>Final game state</h2>
                <CopyableStringComponent desc="final game state" text={gameStateString}/>
                <h2>Compact turn history</h2>
                <CopyableStringComponent desc="compact turn history" text={compactTurnHistoryString}/>
                <h2>Verbose turn history</h2>
                <CopyableStringComponent desc="verbose turn history" text={turnHistoryString}/>
            </div>
        </dialog>
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

    const [saveDialogVisible, setSaveDialogVisible] = useState<boolean>(false);
    const showSaveDialog = useCallback(() => setSaveDialogVisible(true), []);
    const hideSaveDialog = useCallback(() => setSaveDialogVisible(false), []);

    const [historyVisible, setHistoryVisible] = useState<boolean>(false);
    const showHistory = useCallback(() => setHistoryVisible(true), []);
    const hideHistory = useCallback(() => setHistoryVisible(false), []);

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
    const canUndo = augmentedState.gameStates.findIndex(s => aiPlayer[s.player] == null) < augmentedState.gameStates.length - 1;
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

    function handleLoad() {
        let s = prompt("Enter game state or move history");
        if (s == null) return;  // dialog closed
        s = s.replace(/\s+/g,'');  // strip whitespace
        if (s == '') return;  // ignore empty string, instead of parsing as empty turn list
        const newAugmentedState = parseAugmentedState(s);
        if (newAugmentedState == null) {
            alert('Failed to parse game state string! (See Javascript log for details.)');
            return;
        }
        changeState(newAugmentedState);
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
            executeTurn(nextTurn);
        }, aiMoveDelayMs);
        return () => clearTimeout(timeoutId);
    }, [playerDesc, augmentedState]);

    return (
        <div className="mytikas-app">
            <div className="main-column">
                <StateButtonsComponent
                    onSave={showSaveDialog}
                    onLoad={handleLoad}
                    onUndo={canUndo ? handleUndo : undefined}
                    onRedo={canRedo ? handleRedo : undefined}
                    onHistory={historyVisible ? undefined : showHistory}
                    historyFlash={!historyVisible && selectedTurn != null}
                />
                <div className="turn-row">
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
                </div>
                <GameComponent
                    state={gameState}
                    nextActions={nextActions}
                    selectedGod={selectedGod}
                    onSelect={userEnabled ? handleSelect : undefined}
                    onAction={userEnabled ? addAction : undefined}
                />
                <p className="bottom-links">
                    <a href="https://github.com/maksverver/mytikas/">Source code</a>
                    &nbsp;&nbsp;&nbsp;●&nbsp;&nbsp;&nbsp;
                    <a href="https://github.com/maksverver/mytikas/blob/master/RULES.md">Rules summary</a>
                </p>
            </div>
            {historyVisible ?
                <HistoryComponent
                    state={augmentedState}
                    selected={selectedTurn}
                    setSelected={partialTurn.length === 0 ? setSelectedTurn : undefined}
                    onClose={hideHistory}
                />
                : undefined}
            <SaveStateComponent
                augmentedState={augmentedState}
                visible={saveDialogVisible}
                onClose={hideSaveDialog}
            />
        </div>
    );
}
