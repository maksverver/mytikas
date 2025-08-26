export const boardSize = 9;

const _u = undefined;

const fieldIndex = [
    [ _u, _u, _u, _u, 40, _u, _u, _u, _u ],
    [ _u, _u, _u, 37, 38, 39, _u, _u, _u ],
    [ _u, _u, 32, 33, 34, 35, 36, _u, _u ],
    [ _u, 25, 26, 27, 28, 29, 30, 31, _u ],
    [ 16, 17, 18, 19, 20, 21, 22, 23, 24 ],
    [ _u,  9, 10, 11, 12, 13, 14, 15, _u ],
    [ _u, _u,  4,  5,  6,  7,  8, _u, _u ],
    [ _u, _u, _u,  1,  2,  3, _u, _u, _u ],
    [ _u, _u, _u, _u,  0, _u, _u, _u, _u ],
];

export const fieldCount = 41;

export const fieldRows = Object.freeze([
                8,
             7, 7, 7,
          6, 6, 6, 6, 6,
       5, 5, 5, 5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 4, 4,
       3, 3, 3, 3, 3, 3, 3,
          2, 2, 2, 2, 2,
             1, 1, 1,
                0,
]);

export const fieldCols = Object.freeze([
                4,
             3, 4, 5,
          2, 3, 4, 5, 6,
       1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7, 8,
       1, 2, 3, 4, 5, 6, 7,
          2, 3, 4, 5, 6,
             3, 4, 5,
                4,
]);

export const fieldCoords =
      Object.freeze(
         fieldRows.map(
               (r, i) => Object.freeze([r, fieldCols[i]] as [number, number])));

export const fieldNames = Object.freeze([
                            'e1',
                      'd2', 'e2', 'f2',
                'c3', 'd3', 'e3', 'f3', 'g3',
          'b4', 'c4', 'd4', 'e4', 'f4', 'g4', 'h4',
    'a5', 'b5', 'c5', 'd5', 'e5', 'f5', 'g5', 'h5', 'i5',
          'b6', 'c6', 'd6', 'e6', 'f6', 'g6', 'h6',
                'c7', 'd7', 'e7', 'f7', 'g7',
                      'd8', 'e8', 'f8',
                            'e9',
]);

export const gateIndex: readonly [number, number] = Object.freeze([0, 40]);

export function isOnBoard(r: number, c: number): boolean {
    return Math.abs(r - 4) + Math.abs(c - 4) < 5;
}

export function coordsToField(r: number, c: number): number|undefined {
    return isOnBoard(r, c) ? fieldIndex[r][c] : undefined;
}

export function parseField(s: string): number|undefined {
    const i = fieldNames.indexOf(s);
    return i !== -1 ? i : undefined;
}
