import './HistoryComponent.css';

import type { AugmentedState } from "./AugmentedState";
import { playerNames } from './game/player';
import type { GameState } from './game/state';
import { classNames } from './utils';
import { useEffect, useRef } from 'react';

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
            className={
                classNames({
                    'turn': true,
                    [playerNames[index%2]]: true,
                    selectable: onClick != null,
                    selected,
                })
            }
            onClick={onClick}
        >
            {String(index + 1).padStart(3, ' ')}. {turnString}
        </div>
    );
}

export type HistoryProps = {
    state: AugmentedState,
    selected?: number,
    setSelected?: (i: number|undefined) => void,
}

// Displays the history of turns played so far, with a set of control buttons to
// move to the front or back of the list.
//
// If `setSelected` is undefined, selection is disabled.
export function HistoryComponent({state, selected, setSelected}: HistoryProps) {
    const turnCount = state.history.length;

    const hasPrev = (selected ?? turnCount) > 0;
    const hasNext = (selected ?? turnCount) < turnCount;

    const moveToFirst = () => setSelected?.(turnCount > 0 ? 0 : undefined);
    const moveToPrev  = () => setSelected?.(selected != null && selected > 0 ? selected - 1 : turnCount > 0 ? turnCount - 1 : undefined);
    const moveToNext  = () => setSelected?.(selected != null && selected + 1 < turnCount ? selected + 1 : undefined);
    const moveToLast  = () => setSelected?.(undefined);

    const turnListRef = useRef<HTMLDivElement>(null);
    useEffect(() => {
        turnListRef.current?.children[selected ?? turnCount - 1]?.scrollIntoView({
            block: 'nearest', inline: 'nearest'});
    }, [selected ?? turnCount]);

    useEffect(() => {
        if (setSelected == null) return;
        function handleKeyDown(e: KeyboardEvent) {
            switch (e.key) {
            case 'Home':       if (hasPrev) moveToFirst(); break;
            case 'ArrowLeft':  if (hasPrev) moveToPrev(); break;
            case 'ArrowRight': if (hasNext) moveToNext(); break;
            case 'End':        if (hasNext) moveToLast(); break;
            }
        }
        document.addEventListener('keydown', handleKeyDown);
        return () => document.removeEventListener('keydown', handleKeyDown);
    }, [selected, setSelected, turnCount]);

    return (
        <div className="history">
            <div className="buttons">
                <button
                    title="To game start"
                    disabled={setSelected == null || !hasPrev}
                    onClick={moveToFirst}
                >⏮️</button>
                <button
                    title="Previous turn"
                    disabled={setSelected == null || !hasPrev}
                    onClick={moveToPrev}
                >◀️</button>
                <button
                    title="Next turn"
                    disabled={setSelected == null || !hasNext}
                    onClick={moveToNext}
                >▶️</button>
                <button
                    title="To game end"
                    disabled={setSelected == null || !hasNext}
                    onClick={moveToLast}
                >⏭️</button>
            </div>
            <div className="turn-list" ref={turnListRef}>
                {
                    state.history.map((turn, i) =>
                        <TurnComponent
                            key={i}
                            index={i}
                            selected={selected === i}
                            turnString={String(turn)}
                            prevState={state.gameStates[i]}
                            nextState={state.gameStates[i + 1]}
                            onClick={setSelected == null ? undefined :
                                () => setSelected(i === selected ? undefined : i)}
                        />
                    )
                }
            </div>
        </div>
    );
}
