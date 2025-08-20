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
    setSelected?: (i: number|undefined) => void,
}

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
                    disabled={!hasPrev}
                    onClick={moveToFirst}
                >⏮️</button>
                <button
                    title="Previous turn"
                    disabled={!hasPrev}
                    onClick={moveToPrev}
                >◀️</button>
                <button
                    title="Next turn"
                    disabled={!hasNext}
                    onClick={moveToNext}
                >▶️</button>
                <button
                    title="To game end"
                    disabled={!hasNext}
                    onClick={moveToLast}
                >⏭️</button>
            </div>
            <div className="turn-list" ref={turnListRef}>
                {
                    state.history.map(([turnString, _], i) =>
                        <TurnComponent
                            key={i}
                            index={i}
                            selected={selected === i}
                            turnString={turnString}
                            prevState={state.gameStates[i][1]}
                            nextState={state.gameStates[i + 1][1]}
                            onClick={() => setSelected?.(i === selected ? undefined : i)}
                        />
                    )
                }
            </div>
        </div>
    );
}
