export const Player = Object.freeze({
    light: 0,
    dark:  1,
    count: 2,
});

export type PlayerValue = typeof Player[keyof typeof Player];

export const playerNames = Object.freeze(['light', 'dark']);

export const playerByName = Object.freeze({
    'light': Player.light,
    'dark':  Player.dark,
});

export function other(player: PlayerValue): PlayerValue {
    return (
        player === Player.light ? Player.dark :
        player === Player.dark  ? Player.light :
        Player.count);
}
