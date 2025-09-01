import './App.css'

import { useCallback, useEffect, useId, useReducer, useRef } from 'react';
import GameComponent from './GameComponent.tsx';
import { HistoryComponent } from './HistoryComponent.tsx';
import { AugmentedState, type RedoState } from './AugmentedState.ts';
import { Action, formatCompactTurnHistory, formatTurnHistory, parseCompactTurnHistory, parseTurnHistory, partialTurnToString, Turn } from './game/turn.ts';
import { decodeStateString, GameState } from './game/state.ts';
import type { GodValue } from './game/gods.ts';
import { classNames } from './utils.ts';
import { Player, type PlayerValue } from './game/player.ts';

const playerOptions = {
    human:    { title: 'Human',             playerDesc: null },
    random:   { title: 'Random',            playerDesc: 'random' },
    minimax1: { title: 'Minimax (depth 1)', playerDesc: 'minimax,max_depth=1' },
    minimax2: { title: 'Minimax (depth 2)', playerDesc: 'minimax,max_depth=2' },
    minimax3: { title: 'Minimax (depth 3)', playerDesc: 'minimax,max_depth=3' },
    minimax4: { title: 'Minimax (depth 4)', playerDesc: 'minimax,max_depth=4' },
};

type PlayerOption = keyof (typeof playerOptions);

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
    light: PlayerOption;
    dark: PlayerOption;
    setLight: (s: PlayerOption) => void;
    setDark:  (s: PlayerOption) => void;
}

function PlayerSelectComponent({light, dark, setLight, setDark}: PlayerSelectPros) {
    const lightSelectId = useId();
    const darkSelectId  = useId();
    return (
        <div className="player-select">
            <label htmlFor={lightSelectId}>Light player</label>
            <label htmlFor={darkSelectId}>Dark player</label>
            <select id={lightSelectId} value={light} onChange={e => setLight(e.target.value as PlayerOption)}>
                {Object.entries(playerOptions).map(([key, {title}]) =>
                    (<option key={key} value={key}>{title}</option>))}
            </select>
            <select id={darkSelectId} value={dark} onChange={e => setDark(e.target.value as PlayerOption)}>
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

type AppState = {
    augmentedState: AugmentedState,

    // Logic: for a given augmented state, we can either have a turn selected
    // (in the history panel) OR a partial turn being constructed (when it's the
    // user's turn to move), but not both.
    selectedTurn: number|undefined,

    // These are used to construct a turn, by first selecting a god in play,
    // then clicking on a destination field to create a move action, etc.
    // The details are handled by the GameComponent.
    partialTurn: readonly Action[],
    selectedGod: GodValue|undefined,

    // These control which players are controlled by the human user or an AI.
    lightPlayer: PlayerOption,
    darkPlayer: PlayerOption,

    redoStack: RedoState[],

    saveDialogVisible: boolean,

    historyVisible: boolean,
};

function initAppState(): AppState {
    return {
        augmentedState: AugmentedState.fromInitialState(),
        selectedTurn: undefined,
        partialTurn: [],
        selectedGod: undefined,
        lightPlayer: defaultPlayerOption,
        darkPlayer: defaultPlayerOption,
        redoStack: [],
        saveDialogVisible: false,
        historyVisible: false,
    };
}

type AppAction = {
    type: 'select-turn',
    turn: number|undefined,
} | {
    type: 'toggle-selected-god',
    god: GodValue|undefined,
} | {
    type: 'partial-turn-reset' | 'partial-turn-finish',
} | {
    type: 'partial-turn-add-action',
    action: Action,
} | {
    type: 'set-light-player' | 'set-dark-player',
    player: PlayerOption,
} | {
    type: 'undo' | 'redo',
} | {
    type: 'execute-turn',
    turn: Turn,
} | {
    type: 'load-state',
    state: AugmentedState,
} | {
    type: 'show-save-dialog' | 'hide-save-dialog',
} | {
    type: 'show-history' | 'hide-history',
};

function getPlayerOptions(appState: AppState, player: PlayerValue): (typeof playerOptions)[PlayerOption] {
    if (player === Player.light) return playerOptions[appState.lightPlayer];
    if (player === Player.dark)  return playerOptions[appState.darkPlayer];
    return undefined as never;
}

function isUserControlled(appState: AppState, player: PlayerValue) {
    return getPlayerOptions(appState, player).playerDesc == null;
}

function reduceAppState(state: AppState, action: AppAction): AppState {
    function changeState(augmentedState: AugmentedState, redoStack: RedoState[]): AppState {
        return {
            ...state,
            augmentedState,
            partialTurn: [],
            selectedGod: undefined,
            redoStack,
        };
    }

    switch (action.type) {
        case 'select-turn':
            return {...state, selectedTurn: action.turn};

        case 'toggle-selected-god':
            return {
                ...state,
                selectedGod: state.selectedGod === action.god ? undefined : action.god
            };

        case 'partial-turn-reset':
            return {...state, partialTurn: []};

        case 'partial-turn-add-action':
            return {...state, partialTurn: [...state.partialTurn, action.action]};

        case 'partial-turn-finish':
            return changeState(state.augmentedState.addTurn(new Turn(state.partialTurn)), []);

        case 'set-light-player':
            return {...state, lightPlayer: action.player};

        case 'set-dark-player':
            return {...state, darkPlayer: action.player};

        case 'undo':
            {
                let augmentedState = state.augmentedState;
                let redoStack = Array.from(state.redoStack);
                let redoState: RedoState;
                while (augmentedState.canUndo) {
                    [augmentedState, redoState] = augmentedState.undoTurn();
                    redoStack.push(redoState);
                    if (isUserControlled(state, augmentedState.lastGameState.player)) break;
                }
                return changeState(augmentedState, redoStack);
            }

        case 'redo':
            {
                let augmentedState = state.augmentedState;
                let redoStack = Array.from(state.redoStack);
                while (redoStack.length > 0) {
                    augmentedState = augmentedState.redoTurn(redoStack.pop()!);
                    if (isUserControlled(state, augmentedState.lastGameState.player)) break;
                }
                return changeState(augmentedState, redoStack);
            }

        case 'execute-turn':
            return changeState(state.augmentedState.addTurn(action.turn), []);

        case 'load-state':
            return changeState(action.state, []);

        case 'show-save-dialog':
            return {...state, saveDialogVisible: true};

        case 'hide-save-dialog':
            return {...state, saveDialogVisible: false};

        case 'show-history':
            return {...state, historyVisible: true};

        case 'hide-history':
            return {...state, historyVisible: false};
    }
}

export default function App() {
    const [appState, dispatch] = useReducer(reduceAppState, undefined, initAppState);

    const {
        augmentedState,
        selectedTurn,
        partialTurn,
        selectedGod,
        lightPlayer,
        darkPlayer,
        redoStack,
        saveDialogVisible,
        historyVisible,
    } = appState;

    const selectTurn = useCallback((turn: number|undefined) => dispatch({type: 'select-turn', turn}), []);
    const toggleSelectedGod = useCallback((god: GodValue|undefined) => dispatch({type: 'toggle-selected-god', god}), []);
    const partialTurnReset = useCallback(() => dispatch({type: 'partial-turn-reset'}), []);
    const partialTurnAddAction = useCallback((action: Action) => dispatch({type: 'partial-turn-add-action', action}), []);
    const partialTurnFinish = useCallback(() => dispatch({type: 'partial-turn-finish'}), []);
    const setLightPlayer = useCallback((player: PlayerOption) => dispatch({type: 'set-light-player', player}), []);
    const setDarkPlayer = useCallback((player: PlayerOption) => dispatch({type: 'set-dark-player', player}), []);
    const showSaveDialog = useCallback(() => dispatch({type: 'show-save-dialog'}), []);
    const hideSaveDialog = useCallback(() => dispatch({type: 'hide-save-dialog'}), []);
    const showHistory = useCallback(() => dispatch({type: 'show-history'}), []);
    const hideHistory = useCallback(() => dispatch({type: 'hide-history'}), []);
    const undo = useCallback(() => dispatch({type: 'undo'}), []);
    const redo = useCallback(() => dispatch({type: 'redo'}), []);
    const executeTurn = useCallback((turn: Turn) => dispatch({type: 'execute-turn', turn}), []);

    const aiPlayer = [
        playerOptions[lightPlayer].playerDesc,
        playerOptions[darkPlayer].playerDesc,
    ];
    const aiMoveDelayMs = 500;

    const nextTurns = augmentedState.nextTurns;
    const gameIsOver = nextTurns.length === 0;

    const nextPlayer = augmentedState.lastGameState.player;
    const playerDesc = getPlayerOptions(appState, nextPlayer).playerDesc;
    const userEnabled = selectedTurn == null && playerDesc == null && !gameIsOver;

    const gameState =
        userEnabled ? executePartialTurn(augmentedState.lastGameState, partialTurn) :
        selectedTurn == null ? augmentedState.lastGameState : augmentedState.gameStates[selectedTurn];

    // Find next possible actions that are consistent with the current partial turn.
    const [nextActions, partialTurnIsComplete] =
        userEnabled
            ? findNextActions(partialTurn, nextTurns)
            : [[], false];

    // For undoing/redoing, we move to the previous/next state where it was the
    // user's turn to move, otherwise the AI would just immediately move again.
    const canUndo = augmentedState.gameStates.findIndex(s => aiPlayer[s.player] == null) < augmentedState.gameStates.length - 1;
    const canRedo = redoStack.some(s => aiPlayer[s.gameState.player] == null);

    function handleLoad() {
        let s = prompt("Enter game state or move history");
        if (s == null) return;  // dialog closed
        s = s.replace(/\s+/g,'');  // strip whitespace
        if (s == '') return;  // ignore empty string, instead of parsing as empty turn list
        const state = parseAugmentedState(s);
        if (state == null) {
            alert('Failed to parse game state string! (See Javascript log for details.)');
            return;
        }
        dispatch({type: 'load-state', state});
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
        const timeoutId = setTimeout(() =>  executeTurn(nextTurn), aiMoveDelayMs);
        return () => clearTimeout(timeoutId);
    }, [playerDesc, augmentedState]);

    return (
        <div className="mytikas-app">
            <div className="main-column">
                <StateButtonsComponent
                    onSave={showSaveDialog}
                    onLoad={handleLoad}
                    onUndo={canUndo ? undo : undefined}
                    onRedo={canRedo ? redo : undefined}
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
                        onRestart={userEnabled && partialTurn.length > 0 ? partialTurnReset : undefined}
                        onFinish={userEnabled && partialTurnIsComplete ? partialTurnFinish : undefined}
                        finishHint={userEnabled && nextActions.length === 0}
                    />
                </div>
                <GameComponent
                    state={gameState}
                    nextActions={nextActions}
                    selectedGod={selectedGod}
                    onSelect={userEnabled ? toggleSelectedGod : undefined}
                    onAction={userEnabled ? partialTurnAddAction : undefined}
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
                    setSelected={partialTurn.length === 0 ? selectTurn : undefined}
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
