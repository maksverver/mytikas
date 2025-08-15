const boardElem = document.getElementById('board');
const boardPiecesElem = document.getElementById('board-pieces');

const lightPlayer = 0;
const darkPlayer  = 1;
const playerCount = 2;

const zeus        =  0;
const hephaestus  =  1;
const hera        =  2;
const poseidon    =  3;
const apollo      =  4;
const aphrodite   =  5;
const ares        =  6;
const hermes      =  7;
const dionysus    =  8;
const artemis     =  9;
const hades       = 10;
const athena      = 11;
const godCount    = 12;

const godLetters = 'ZHEPOARMDTSN';
const godEmojis  = '';

// Keep this in sync with `pantheon` in src/state.cc
const pantheon = (function() {
    const NO_DIRS = 0;  // TODO
    const ALL8    = 0;  // TODO
    const ORTHO   = 0;  // TODO
    const DIAG    = 0;  // TODO
    const KNIGHT  = 0;  // TODO
    const UNAFFECTED   = 0;  // TODO
    const DAMAGE_BOOST = 0;  // TODO
    const SPEED_BOOST  = 0;  // TODO
    const SHIELDED     = 0;  // TODO
    const DIRECT = (d) => d;  // TODO

    const defs = [
        // name        id  emoji  hit mov dmg rng  mov_dirs         atk_dirs        aura
        ["Zeus",       'Z', "⚡️", 10,   1, 10,  3, ALL8,            DIRECT(ORTHO),  UNAFFECTED   ],
        ["Hephaestus", 'H', "🔨",  9,   2,  7,  2, ORTHO,           DIRECT(ORTHO),  DAMAGE_BOOST ],
        ["hEra",       'E', "👸",  8,   2,  5,  2, DIAG,            DIAG,           UNAFFECTED   ],
        ["Poseidon",   'P', "🔱",  7,   3,  4,  0, ORTHO,           NO_DIRS,        UNAFFECTED   ],
        ["apOllo",     'O', "🏹",  6,   2,  2,  3, ALL8,            ALL8,           UNAFFECTED   ],
        ["Aphrodite",  'A', "🌹",  6,   3,  6,  1, ALL8,            ALL8,           UNAFFECTED   ],
        ["aRes",       'R', "⚔️",  5,   3,  5,  3, DIRECT(ALL8),    DIRECT(ALL8),   UNAFFECTED   ],
        ["herMes",     'M', "🪽",  5,   3,  3,  2, ALL8,            DIRECT(ALL8),   SPEED_BOOST  ],
        ["Dionysus",   'D', "🍇",  4,   1,  4,  0, KNIGHT,          NO_DIRS,        UNAFFECTED   ],
        ["arTemis",    'T', "🦌",  4,   2,  4,  2, ALL8,            DIRECT(DIAG),   UNAFFECTED   ],
        ["hadeS",      'S', "🐕",  3,   3,  3,  1, DIRECT(ALL8),    NO_DIRS,        UNAFFECTED   ],
        ["atheNa",     'N', "🛡️",  3,   1,  3,  3, ALL8,            DIRECT(ALL8),   SHIELDED     ],
    ];

    return Object.freeze(defs.map(
        ([name, id, emoji, hit, mov, dmg, rng, mov_dirs, atk_dirs, aura]) =>
        ({name, id, emoji, hit, mov, dmg, rng, mov_dirs, atk_dirs, aura})
    ).map(Object.freeze));
})();

const boardSize = 9;

const fieldIndex = [
    [ -1, -1, -1, -1,  0, -1, -1, -1, -1 ],
    [ -1, -1, -1,  1,  2,  3, -1, -1, -1 ],
    [ -1, -1,  4,  5,  6,  7,  8, -1, -1 ],
    [ -1,  9, 10, 11, 12, 13, 14, 15, -1 ],
    [ 16, 17, 18, 19, 20, 21, 22, 23, 24 ],
    [ -1, 25, 26, 27, 28, 29, 30, 31, -1 ],
    [ -1, -1, 32, 33, 34, 35, 36, -1, -1 ],
    [ -1, -1, -1, 37, 38, 39, -1, -1, -1 ],
    [ -1, -1, -1, -1, 40, -1, -1, -1, -1 ],
];

const fieldCount = 41;

const fieldRows = [
                0,
             1, 1, 1,
          2, 2, 2, 2, 2,
       3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4,
       5, 5, 5, 5, 5, 5, 5,
          6, 6, 6, 6, 6,
             7, 7, 7,
                8,
];

const fieldCols = [
                4,
             3, 4, 5,
          2, 3, 4, 5, 6,
       1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7, 8,
       1, 2, 3, 4, 5, 6, 7,
          2, 3, 4, 5, 6,
             3, 4, 5,
                4,
];

const fieldNames = [
                            'e9',
                      'd8', 'e8', 'f8',
                'c7', 'd7', 'e7', 'f7', 'g7',
          'b6', 'c6', 'd6', 'e6', 'f6', 'g6', 'h6',
    'a5', 'b5', 'c5', 'd5', 'e5', 'f5', 'g5', 'h5', 'i5',
          'b4', 'c4', 'd4', 'e4', 'f4', 'g4', 'h4',
                'c3', 'd3', 'e3', 'f3', 'g3',
                      'd2', 'e2', 'f2',
                            'e1',
];

const fieldSizePx = 60;

function other(player) {
    return (
        player == lightPlayer ? darkPlayer  :
        player == darkPlayer  ? lightPlayer :
        undefined);
}

function isOnBoard(r, c) {
    return Math.abs(r - 4) + Math.abs(c - 4) < 5;
}

function coordsToField(r, c) {
    return isOnBoard(r, c) ? fieldIndex[r][c] : -1;
}

function parseField(s) {
    if (typeof s !== 'string') {
        throw new Error(`Invalid type of argument for parseField(): ${typeof s}`);
    }
    return fieldNames.indexOf(s);
}

let boardPieceElems = Array.from({length: fieldCount});

function setPiecePos(pieceElem, field) {
    pieceElem.style.left = `${fieldCols[field] * fieldSizePx}px`;
    pieceElem.style.top  = `${fieldRows[field] * fieldSizePx}px`;
    console.log(field);
}

function placePiece(player, god, field) {
    const pieceElem = document.createElement('div');
    pieceElem.classList.add('piece');
    boardPieceElems[field] = pieceElem;
    boardPiecesElem.appendChild(pieceElem);
    pieceElem.appendChild(document.createTextNode(pantheon[god].emoji));
    setPiecePos(pieceElem, field);
}

function movePiece(src, dst) {
    setPiecePos(boardPieceElems[src], dst);
}

// Initialize the DOM structure. Doesn't use the WASM API, so can be run
// as soon as the DOM tree is loaded.
function initializeBoard() {
    for (let row = 0; row < 9; ++row) {
        for (let col = 0; col < 9; ++col) {
            if (isOnBoard(row, col)) {
                const fieldElem = document.createElement('div');
                boardElem.appendChild(fieldElem);
                fieldElem.classList.add('square', ['dark', 'light'][(row + col) % 2]);
                if (!isOnBoard(row - 1, col)) fieldElem.classList.add('border-top');
                if (!isOnBoard(row + 1, col)) fieldElem.classList.add('border-bottom');
                if (!isOnBoard(row, col - 1)) fieldElem.classList.add('border-left');
                if (!isOnBoard(row, col + 1)) fieldElem.classList.add('border-right');
                fieldElem.style.gridRow = row + 1;
                fieldElem.style.gridColumn = col + 1;
            }
        }
    }
}

// Initialization function that runs after the Web Assembly module has been loaded.
function initializeMytikas(wasmApi) {
    /* TEMP -- TODO: DELETE THIS */
    placePiece(lightPlayer, zeus, parseField('e1'));
    setTimeout(() => {
        movePiece(parseField('e1'), parseField('a5'));
    }, 1000);
}

// Initialize DOM tree.
initializeBoard();
