import './GameComponent.css'

import { fieldRows, fieldCols, fieldCoords, isOnBoard, gateIndex, fieldNames } from './game/board.ts';
import { pantheon, StatusEffects, type GodValue } from './game/gods.ts';
import { Player, playerNames, type PlayerValue } from './game/player.ts';
import { GameState, type AliveGodState, type GodState } from './game/state.ts';
import { ActionType, Action, type ActionTypeValue } from './game/turn.ts';
import { classNames } from './utils.ts';

const fieldSizePx = 60;

type FieldProps = {
    fieldIndex: number;
    selected: boolean,
    actionType?: ActionTypeValue;
    onClick?: () => void;
};

function FieldComponent({fieldIndex, actionType, selected, onClick}: FieldProps) {
    const [row, col] = fieldCoords[fieldIndex];
    return (
        <div
            className={classNames({
                'field': true,
                [['dark', 'light'][(row + col) % 2]]: true,
                'border-top':    !isOnBoard(row - 1, col),
                'border-bottom': !isOnBoard(row + 1, col),
                'border-left':   !isOnBoard(row, col - 1),
                'border-right':  !isOnBoard(row, col + 1),
                'selected':      selected,
                'clickable':     onClick != null,
                'move':          actionType === ActionType.move,
                'attack':        actionType === ActionType.attack,
                'special':       actionType === ActionType.special,
            })}
            style={{
                gridRow: row + 1,
                gridColumn: col + 1,
            }}
            onClick={onClick}
        >
            <div className="coords">{fieldNames[fieldIndex]}</div>
        </div>
    );
}

type PieceProps = {
    player: PlayerValue;
    god: GodValue;
    state: AliveGodState;
    onClick?: () => void;
};

function PieceComponent({player, god, state, onClick}: PieceProps) {
    return (
        <div
            className={classNames({
                piece: true,
                'fade-in': true,
                [playerNames[player]]: true,
                clickable: onClick != null,
            })}
            style={{
                left: fieldCols[state.field] * fieldSizePx,
                top:  fieldRows[state.field] * fieldSizePx,
            }}
            onClick={onClick}
        >
            <div className="emoji">{pantheon[god].emoji}</div>
            <div className="health">{state.health}</div>
            <div className="name">{pantheon[god].name}</div>
            {(state.effects & StatusEffects.chained)
                ? <div className="chained">🔗</div>
                : undefined}
        </div>
    );
}

type BoardProps = {
    state: GameState;
    selectableGods: Map<number, GodValue>;
    fieldActions: Map<number, ActionTypeValue>;
    selectedGod: GodValue|undefined;
    onSelect?: (god: GodValue|undefined) => void;
    onAction?: (action: Action) => void;
};

export function BoardComponent({state, selectableGods, fieldActions, selectedGod, onSelect, onAction}: BoardProps) {
    const selectedGodState = selectedGod == null ? undefined : state.gods[state.player][selectedGod];
    const selectedField = selectedGodState?.state === 'alive' ? selectedGodState.field : undefined;

    return (
        <div className="board-container">
            <div className="board" id="board">
                {
                    fieldCoords.map((_, i) => {
                        let handleFieldClick: (() => void) | undefined = undefined;
                        let actionType: ActionTypeValue | undefined = undefined;
                        if (onAction != null && selectedGod != null && fieldActions.has(i)) {
                            actionType = fieldActions.get(i)!;
                            handleFieldClick = () => onAction!(new Action(actionType!, selectedGod, i));
                        } else if (onSelect != null && selectableGods.has(i)) {
                            handleFieldClick = () => onSelect!(selectableGods.get(i));
                        }
                        return (
                            <FieldComponent
                                key={i}
                                fieldIndex={i}
                                onClick={handleFieldClick}
                                actionType={actionType}
                                selected={selectedField===i}
                            />
                        );
                    })
                }
            </div>
            <div className="board-pieces" id="board-pieces">
                {
                    state.gods.flatMap((gods, p) =>
                        gods.flatMap((godState, g) => {
                            const player = p as PlayerValue;
                            const god = g as GodValue;
                            if (godState.state !== 'alive') return;
                            return (
                                <PieceComponent
                                    key={String([player, god])}
                                    player={player}
                                    god={god}
                                    state={godState}
                                />
                            );
                        })
                    )
                }
            </div>
        </div>
    );
}

type GodRosterItemProps = {
    player: PlayerValue;
    god: GodValue;
    state: GodState;
    onClick?: () => (void);
};

export function GodRosterItem({player, god, state, onClick}: GodRosterItemProps) {
    return (
        <div
            className={classNames({
                item: true,
                [state.state]: true,
                'clickable': onClick != null,
            })}
            onClick={onClick}
        >
            <div className={`piece ${playerNames[player]}`}>
                <div className="emoji">{pantheon[god].emoji}</div>
                <div className="health">{pantheon[god].hit}</div>
                <div className="name">{pantheon[god].name}</div>
            </div>
            <div className="eliminated">☠️</div>
        </div>
    );
}

type GodRosterProps = {
    player: PlayerValue;
    gods: readonly GodState[];
    summonable: Set<GodValue>;
    onSummon: (god: GodValue) => (void);
};

export function GodRoster({player, gods, summonable, onSummon}: GodRosterProps) {
    return (
        <div className="roster">
            {
                gods.map((state, i) => {
                    const god = i as GodValue;
                    return (
                        <GodRosterItem key={i} player={player} god={god} state={state}
                            onClick={
                                summonable.has(god)
                                    ? () => onSummon(god)
                                    : undefined
                            }
                        />
                    );
                })
            }
        </div>
    );
}

type GameProps = {
    state: GameState;
    nextActions: Action[];
    selectedGod: GodValue|undefined;
    onSelect?: (god: GodValue|undefined) => void;
    onAction?: (action: Action) => void;
}

export function GameComponent({state, nextActions, selectedGod, onSelect, onAction}: GameProps) {
    const summonable = new Set<GodValue>();
    const selectableGods = new Map<number, GodValue>();  // field -> god
    const fieldActions = new Map<number, ActionTypeValue>();
    for (const action of nextActions) {
        if (action.type === ActionType.summon) {
            summonable.add(action.god);
        } else {
            const gs = state.gods[state.player][action.god];
            if (gs.state === 'alive') {
                selectableGods.set(gs.field, action.god);
            }
            if (selectedGod != null && action.god === selectedGod) {
                // When multiple actions apply to the same field, keep the
                // minimum type. This prefers attack over special moves, which
                // only matters for Artemis when her Withering Moon special
                // targets an enemy that she can also attack directly.
                fieldActions.set(action.field,
                    Math.min(fieldActions.get(action.field) ?? action.type, action.type));
            }
        }
    }

    function onSummonAction(god: GodValue) {
        onAction?.(new Action(ActionType.summon, god, gateIndex[state.player]));
    }

    return (
        <div className="game-area">
            <GodRoster
                player={Player.dark}
                gods={state.gods[Player.dark]}
                summonable={state.player == Player.dark ? summonable : new Set()}
                onSummon={onSummonAction}
            />
            <BoardComponent
                state={state}
                selectableGods={selectableGods}
                fieldActions={fieldActions}
                selectedGod={selectedGod}
                onSelect={onSelect}
                onAction={onAction}
            />
            <GodRoster
                player={Player.light}
                gods={state.gods[Player.light]}
                summonable={state.player == Player.light ? summonable : new Set()}
                onSummon={onSummonAction}
            />
        </div>
    );
}

export default GameComponent;
