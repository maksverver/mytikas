// Wraps the native API, and calls `cont` with a single object as the argument,
// which has the following members:
//
//  initialStateString
//  generateTurns(stateString) -> array of turn strings
//  executeTurn(stateString, turnString) -> state string
//  chooseAiTurn(stateString, playerDescString) -> turn string
//
function initializeWasmApi(cont) {
    const {
        _mytikas_alloc,
        _mytikas_free,
        _initial_state_string,
        _mytikas_generate_turns,
        _mytikas_execute_turn,
        _mytikas_ai_turn,
    } = Module;

    // This uses functions from Emscripten for string conversion:
    //   - UTF8ToString(ptr[, maxBytesToRead][, ignoreNul])
    //   - stringToUTF8(str, outPtr, maxBytesToWrite)
    //   - lengthBytesUTF8(str)
    // https://emscripten.org/docs/api_reference/preamble.js.html

    function allocCstring(arg) {
        const str = String(arg);
        const len = lengthBytesUTF8(str);
        const ptr = _mytikas_alloc(len + 1);
        if (ptr) stringToUTF8(str, ptr, len + 1)
        return ptr;
    }

    function freeCstring(ptr) {
        if (ptr) _mytikas_free(ptr);
    }

    function freeCstringList(ptr) {
        if (ptr) _mytikas_free(ptr);
    }

    function fromCstring(ptr) {
        return ptr ? UTF8ToString(ptr) : undefined;
    }

    function fromCstringList(ptr) {
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

    const initialStateString = fromCstring(getValue(_initial_state_string, 'i8*'));

    function generateTurns(stateString) {
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

    function executeTurn(stateString, turnString) {
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

    function chooseAiTurn(stateString, playerDescString) {
        const stateCstring = allocCstring(stateString);
        const playerDescCstring = allocCstring(playerDescString);
        let turnCstring = null;
        try {
            turnCstring = _mytikas_ai_turn(stateCstring, playerDescCstring);
            return fromCstring(turnCstring);
        } finally {
            freeCstring(stateCstring);
            freeCstring(playerDescCstring);
            freeCstring(turnCstring);
        }
    }

    cont(Object.freeze({
        initialStateString,
        generateTurns,
        executeTurn,
        chooseAiTurn,
    }));
}
