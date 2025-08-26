export const Player = Object.freeze({
    light: 0,
    dark:  1,
});

export const playerCount = 2;

export type PlayerValue = typeof Player[keyof typeof Player];

export const playerNames = Object.freeze(['light', 'dark']);

export const playerByName = Object.freeze({
    'light': Player.light,
    'dark':  Player.dark,
});

export function other(player: PlayerValue): PlayerValue {
    switch (player) {
        case Player.light: return Player.dark;
        case Player.dark:  return Player.light;
    }
    throw new Error('Invalid player value: ' + player);
}
