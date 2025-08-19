import './GameComponent.css'

import { fieldRows, fieldCols, fieldCoords, isOnBoard } from './game/board.ts';
import { pantheon, type GodValue } from './game/gods.ts';
import { Player, playerNames, type PlayerValue } from './game/player.ts';
import { type AliveGodState, type GameState, type GodState } from './game/state.ts';
import { classNames } from './utils.ts';

const fieldSizePx = 60;

function FieldComponent({fieldIndex}: {fieldIndex: number}) {
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
            })}
            style={{
                gridRow: row + 1,
                gridColumn: col + 1,
            }}
        />
    );
}

function PieceComponent({player, god, state}: {player: PlayerValue, god: GodValue, state: AliveGodState}) {
    /* TODO: show chained status */
    return (
        <div
            className={`piece fade-in ${playerNames[player]}`}
            style={{
                left: fieldCols[state.field] * fieldSizePx,
                top:  fieldRows[state.field] * fieldSizePx,
            }}
        >
            <div className="emoji">{pantheon[god].emoji}</div>
            <div className="health">{state.health}</div>
            <div className="name">{pantheon[god].name}</div>
        </div>
    );
}

export function BoardComponent({state}: {state: GameState}) {
    return (
        <div className="board-container">
            <div className="board" id="board">
                { fieldCoords.map((_, i) => <FieldComponent key={i} fieldIndex={i} />) }
            </div>
            <div className="board-pieces" id="board-pieces">
                {
                    state.gods.flatMap((gods, player) =>
                        gods.flatMap((godState, god) =>
                            godState.state === 'alive'
                                ? [<PieceComponent
                                        key={String([player, god])}
                                        player={player as PlayerValue}
                                        god={god as GodValue}
                                        state={godState}
                                    />]
                                : []
                        )
                    )
                }
            </div>
        </div>
    );
}

export function GodRosterItem({player, god, state}: {player: PlayerValue, god: GodValue, state: GodState}) {
    return (
        <div className={`item ${state.state}`}>
            <div className={`piece ${playerNames[player]}`}>
                <div className="emoji">{pantheon[god].emoji}</div>
                <div className="health">{pantheon[god].hit}</div>
                <div className="name">{pantheon[god].name}</div>
            </div>
            <div className="eliminated">☠️</div>
        </div>
    );
}

export function GodRoster({player, gods}: {player: PlayerValue, gods: GodState[]}) {
    return (
        <div className="roster">
            {
                gods.map((state, god) =>
                    <GodRosterItem
                        key={god}
                        player={player as PlayerValue}
                        god={god as GodValue}
                        state={state}
                    />
                )
            }
        </div>
    );
}

export function GameComponent({state}: {state: GameState}) {
    return (
        <div className="game-area">
            <GodRoster player={Player.dark} gods={state.gods[Player.dark]}/>
            <BoardComponent state={state}/>
            <GodRoster player={Player.light} gods={state.gods[Player.light]}/>
        </div>
    );
}

export default GameComponent;
