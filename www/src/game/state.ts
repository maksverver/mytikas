import { fieldCount } from "./board";
import { God, pantheon, StatusEffects, type GodValue } from "./gods";
import type { PlayerValue } from "./player";

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

export type GameState = {
    player: PlayerValue,
    gods: [GodState[], GodState[]],
};

const base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Keep this in sync with State::Encode() / State::Decode() in state.cc
export function decodeStateString(s: string): GameState|undefined {
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
    return { player, gods };
}