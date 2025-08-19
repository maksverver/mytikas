// Native API wrapper.

// @ts-expect-error: Could not find a declaration file for module
import MytikasWasmApi from '../generated/wasm-api.js';

const {
    _mytikas_alloc,
    _mytikas_free,
    _initial_state_string,
    _mytikas_generate_turns,
    _mytikas_execute_turn,
    _mytikas_choose_ai_turn,
    getValue,
    // setValue,
    UTF8ToString,
    lengthBytesUTF8,
    stringToUTF8,
} = await MytikasWasmApi();

// This uses functions from Emscripten for string conversion:
//   - UTF8ToString(ptr[, maxBytesToRead][, ignoreNul])
//   - stringToUTF8(str, outPtr, maxBytesToWrite)
//   - lengthBytesUTF8(str)
// https://emscripten.org/docs/api_reference/preamble.js.html

function allocCstring(str: string): number {
    const len = lengthBytesUTF8(str);
    const ptr = _mytikas_alloc(len + 1);
    if (ptr) stringToUTF8(str, ptr, len + 1)
    return ptr;
}

function freeCstring(ptr: number|undefined) {
    if (ptr) _mytikas_free(ptr);
}

function freeCstringList(ptr: number|undefined) {
    if (ptr) _mytikas_free(ptr);
}

function fromCstring(ptr: number|undefined): string|undefined {
    return ptr ? UTF8ToString(ptr) : undefined;
}

function fromCstringList(ptr: number|undefined): string[]|undefined {
    if (!ptr) return undefined;
    const res = [];
    for (;;) {
        const s = UTF8ToString(ptr);
        if (s === '') break;
        res.push(s);
        ptr += lengthBytesUTF8(s) + 1;
    }
    return res;
}

export const initialStateString = (() => {
    const res = fromCstring(getValue(_initial_state_string, 'i8*'));
    if (res == null) throw new Error('Could not read initial state string!');
    return res;
})();

export function generateTurns(stateString: string): string[]|undefined {
    const stateCstring = allocCstring(stateString);
    let turnsCstring = null;
    try {
        turnsCstring = _mytikas_generate_turns(stateCstring);
        return fromCstringList(turnsCstring);
    } finally {
        freeCstring(stateCstring);
        freeCstringList(turnsCstring);
    }
}

export function executeTurn(stateString: string, turnString: string): string|undefined {
    const stateCstring = allocCstring(stateString);
    const turnCstring = allocCstring(turnString);
    let newStateCstring = null;
    try {
        newStateCstring = _mytikas_execute_turn(stateCstring, turnCstring);
        return fromCstring(newStateCstring);
    } finally {
        freeCstring(stateCstring);
        freeCstring(turnCstring);
        freeCstring(newStateCstring);
    }
}

export function chooseAiTurn(stateString: string, playerDescString: string): string|undefined {
    const stateCstring = allocCstring(stateString);
    const playerDescCstring = allocCstring(playerDescString);
    let turnCstring = null;
    try {
        turnCstring = _mytikas_choose_ai_turn(stateCstring, playerDescCstring);
        return fromCstring(turnCstring);
    } finally {
        freeCstring(stateCstring);
        freeCstring(playerDescCstring);
        freeCstring(turnCstring);
    }
}
