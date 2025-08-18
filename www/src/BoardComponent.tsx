import './BoardComponent.css'

import { fieldRows, fieldCols, fieldCoords, isOnBoard } from './game/board.ts';
import { pantheon, type GodValue } from './game/gods.ts';
import { playerNames, type PlayerValue } from './game/player.ts';
import { type AliveGodState, type GameState, type GodState } from './game/state.ts';

const fieldSizePx = 60;

function FieldComponent({fieldIndex}: {fieldIndex: number}) {
    const [row, col] = fieldCoords[fieldIndex];
    const classNames = ['field', ['dark', 'light'][(row + col) % 2]];
    if (!isOnBoard(row - 1, col)) classNames.push('border-top');
    if (!isOnBoard(row + 1, col)) classNames.push('border-bottom');
    if (!isOnBoard(row, col - 1)) classNames.push('border-left');
    if (!isOnBoard(row, col + 1)) classNames.push('border-right');
    return (
        <div
            className={classNames.join('  ')}
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
            className={`piece ${playerNames[player]}`}
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

export default function BoardComponent({state}: {state: GameState}) {
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
