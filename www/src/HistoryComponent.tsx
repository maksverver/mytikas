import './HistoryComponent.css';

import type { AugmentedState } from "./AugmentedState";
import { playerNames } from './game/player';
import type { GameState } from './game/state';
import { classNames } from './utils';

export type TurnProps = {
    index: number,
    selected: boolean,
    turnString: string
    prevState: GameState,
    nextState: GameState,
    onClick?: () => void,
}

function TurnComponent({index, selected, turnString, prevState, nextState, onClick}: TurnProps) {
    return (
        <div
            className={classNames({'turn': true, [playerNames[index%2]]: true, selected})}
            onClick={onClick}
        >
            {String(index + 1).padStart(3, ' ')}. {turnString}
        </div>
    );
}

export type HistoryProps = {
    state: AugmentedState,
    selected?: number,
    onSelect?: (i: number|undefined) => void,
}

export function HistoryComponent({state, selected, onSelect}: HistoryProps) {
    return (
        <div className="history">
            {
                state.history.map(([turnString, _], i) =>
                    <TurnComponent
                        key={i}
                        index={i}
                        selected={selected === i}
                        turnString={turnString}
                        prevState={state.gameStates[i][1]}
                        nextState={state.gameStates[i + 1][1]}
                        onClick={onSelect? () => onSelect(i) : undefined}
                    />
                )
            }
        </div>
    );
}