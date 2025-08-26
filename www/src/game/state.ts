// Game state representation.
//
// Keep the encoding/decoding logic in sync with State::Encode() /
// State::Decode() defined in state.cc.

import { fieldCount } from "./board";
import { God, godCount, pantheon, StatusEffects, type GodValue } from "./gods";
import { other, type PlayerValue } from "./player";
import { Action, parseTurnString, parseTurnStrings, partialTurnToString, Turn } from "./turn.ts";
import * as wasmApi from '../wasm-api.ts';

const base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

export type ReservedGodState = {
    state:   'reserved',  // not yet available
};
export type AvailableGodState = {
    state:   'available',  // can be summoned
};
export type AliveGodState = {
    state:   'alive',
    health:  number,
    effects: number,
    field:   number,
};
export type DeadGodState = {
    state:   'dead',
};

export type GodState = ReservedGodState|AvailableGodState|AliveGodState|DeadGodState;

export class GameState {
    readonly player: PlayerValue;
    readonly gods: readonly [readonly GodState[], readonly GodState[]];

    constructor(player: PlayerValue, gods: readonly [readonly GodState[], readonly GodState[]]) {
        this.player = player;
        this.gods = gods;
    }

    static fromString(stateString: string) {
        return decodeStateString(stateString);
    }

    // The following functions are wrappers around those defined in wasm-api.ts:

    static initial() {
        return GameState.fromString(wasmApi.initialStateString);
    }

    generateTurns(): Turn[] {
        const nextTurnStrings = wasmApi.generateTurns(this.toString());
        if (nextTurnStrings == null) {
            console.error('Invalid state string:', this);
            throw new Error('Invalid state string!');
        }
        return parseTurnStrings(nextTurnStrings);
    }

    // Not currently implemented:
    // function executeAction(stateString: string, actionString: string): string|undefined;

    executeActions(partialTurn: Action[]): GameState {
        const newStateString = wasmApi.executeActions(this.toString(), partialTurnToString(partialTurn));
        if (newStateString == null) {
            console.error('Failed to execute partial turn:', this, partialTurn);
            throw new Error('Failed to execute partial turn');
        }
        return decodeStateString(newStateString);
    }

    executeTurn(turn: Turn): GameState {
        const oldStateString = this.toString();
        const newStateString = wasmApi.executeTurn(oldStateString, turn.toString());
        if (newStateString == null || turn == null) {
            console.error('Invalid turn:', turn);
            throw new Error('Invalid turn string!');
        }
        return decodeStateString(newStateString);
    }

    endTurn(): GameState {
        // Note: this does not currently call the WASM API:
        //
        // function endTurn(stateString: string): string|undefined;
        //
        return new GameState(other(this.player), this.gods);
    }

    // Chooses a turn using an algorithm described by the given playerDesc (see
    // the CLI app for options). This may only be called when there are moves
    // left, i.e., when generateTurns() returns a nonempty list!
    chooseAiTurn(playerDesc: string): Turn {
        const turnString = wasmApi.chooseAiTurn(this.toString(), playerDesc);
        if (turnString == null) {
            console.error('AI move failed! stateString:', this, 'playerDesc:', playerDesc);
            throw new Error('AI failed!');
        }
        return parseTurnString(turnString);
    }

    toString() {
        let res = '';
        res += base64Digits[this.player];
        for (let p = 0; p < 2; ++p) {
            for (let g = 0; g < godCount; ++g) {
                const gs = this.gods[p][g];
                switch (gs.state) {
                    case 'alive':
                        res += base64Digits[gs.field];
                        res += base64Digits[(gs.health << 1) | ((gs.effects & StatusEffects.chained) ? 1 : 0)];
                        break;
                    case 'dead':
                        res += base64Digits[fieldCount + 0];
                        break;
                    case 'available':
                        res += base64Digits[fieldCount + 1];
                        break;
                    case 'reserved':
                        res += base64Digits[fieldCount + 2];
                        break;
                }
            }
        }
        return res;
    }
};

export function decodeStateString(s: string): GameState {
    let pos = 0;
    function read(lim: number): number {
        if (pos >= s.length) {
            throw new Error('read past end of string');
        }
        const i = base64Digits.indexOf(s.charAt(pos++));
        if (i == -1) {
            throw new Error('invalid character');
        }
        if (i >= lim) {
            throw new Error('value out of range');
        }
        return i;
    };

    function decodeGodState(god: GodValue): GodState {
        const field = read(fieldCount + 3);
        if (field < fieldCount) {  // in play
            const hpfx = read((pantheon[god].hit + 1)*2);
            const health = hpfx >> 1;
            const effects = (hpfx & 1) ? StatusEffects.chained : StatusEffects.none;
            return {state: 'alive', health, effects, field};
        }
        if (field === fieldCount) {
            return {state: 'dead'};
        }
        if (field === fieldCount + 1) {
            return {state: 'available'};
        }
        if (field === fieldCount + 2) {
            return {state: 'reserved'};
        }
        throw new Error('unreachable');
    }

    function decodeGods(): GodState[] {
        return Object.values(God).map(g => decodeGodState(g));
    }

    const player = read(2) as PlayerValue;
    const gods: [GodState[], GodState[]] = [decodeGods(), decodeGods()];
    // TODO: infer status effects from surrounding pieces

    const res = new GameState(player, gods);
    if (String(res) != s) {
        console.error('Game state encoding error! Expected:', s, 'Received:', res);
        alert('Game state encoding error!');
    }
    return res;
}
