import { fieldCount, fieldNames, parseField } from "./board";
import { base64Digits } from "./encoding";
import { godCount, pantheon, type GodValue } from "./gods";

export const ActionType = {
    summon:  0,
    move:    1,
    attack:  2,
    special: 3,
};
export type ActionTypeValue = typeof ActionType[keyof typeof ActionType];

const passString = 'x';

//const actionTypeNames = ['summon', 'move', 'attack', 'special'];
const actionTypeChars = "@>!+";

// Limit on the integer returned by Action.encodeInt().
// The product of godCount * fieldCount * #actions == 4*12*41 == 1968
const maxActionInt = 4*12*41;

export class Action {
    type: ActionTypeValue;
    god: GodValue;
    field: number;

    constructor(type: ActionTypeValue, god: GodValue, field: number) {
        this.type = type;
        this.god = god;
        this.field = field;
    }

    // Encodes the current action as an integer between 0 and maxActionInt (exclusive).
    // This is useful for equality comparisons, and used for compact history encoding.
    encodeInt() {
        return ((this.type * godCount) + this.god) * fieldCount + this.field;
    }

    static decodeInt(i: number) {
        if (!Number.isInteger(i) || i < 0 || i >= maxActionInt) {
            throw new Error(`Action int out of range: ${i}`);
        }
        const field = i % fieldCount;
        i = (i - field) / fieldCount;
        const god = i % godCount as GodValue;
        i = (i - god) / godCount;
        const type = i as ActionTypeValue;
        return new Action(type, god, field);
    }

    equals(that: Action) {
        return (
            this.type  === that.type  &&
            this.god   === that.god   &&
            this.field === that.field)
    }

    // Encodes the current action in a standard string like Z@e1 (god, type, field).
    // The reverse of parseAction().
    toString() {
        return pantheon[this.god]?.id + actionTypeChars[this.type] + fieldNames[this.field];
    }
}

export function parseAction(s: string): Action {
    if (s.length !== 4) {
        throw new Error(`Invalid action length: ${s.length} (expected: 4)`);
    }
    const godId = s.charAt(0);
    const typeId = s.charAt(1);
    const fieldId = s.substring(2);
    const god = pantheon.findIndex((god) => god.id === s.charAt(0))
    if (god < 0 || god >= godCount) {
        throw new Error(`Invalid god id: ${godId}`);
    }
    const type = actionTypeChars.indexOf(typeId);
    if (type === -1) {
        throw new Error(`Invalid type id: ${typeId}`);
    }
    const fieldIndex = parseField(fieldId);
    if (fieldIndex == null) {
        throw new Error(`Invalid field id: ${fieldId}`);
    }
    return new Action(type, god as GodValue, fieldIndex);
}

export class Turn {
    readonly actions: readonly Action[];

    constructor(actions: readonly Action[]) {
        this.actions = actions;  // should I copy here?
    }

    equals(that: Turn) {
        return this.actions.length === that.actions.length &&
            this.actions.every((action, i) => action.equals(that.actions[i]));
    }

    toString() {
        return partialTurnToString(this.actions);
    }
}

export function partialTurnToString(actions: readonly Action[]): string {
    return actions.length === 0 ? passString : actions.join(',');
}

export function parseTurnString(s: string): Turn {
    return new Turn(s === passString ? [] : s.split(',').map(parseAction));
}

export function parseTurnStrings(turnStrings: readonly string[]): Turn[] {
    return turnStrings.map(parseTurnString);
}

export function formatTurnHistory(turns: readonly Turn[]) {
    return turns.join(';');
}

export function parseTurnHistory(s: string): Turn[] {
    return s.split(';').map(parseTurnString);
}

// Encodes a number between 0 and 4096 (exclusive) using two base-64 digits
// in little-endian order.
function encodeBase4096(n: number) {
    if (!Number.isInteger(n) || n < 0 || n >= 4096) {
        throw new Error(`Number out of range ${n}`);
    }
    return base64Digits[n & 63] + base64Digits[n >>> 6];
}

function decodeBase4096(s: string) {
    let x, y;
    if (s.length !== 2 ||
            (x = base64Digits.indexOf(s.charAt(0))) < 0 ||
            (y = base64Digits.indexOf(s.charAt(1))) < 0) {
        throw new Error(`Invalid base 4096 string: ${s}`);
    }
    return x + (y << 6);
}

// In the compact turn encoding, actions are encoding into base 4096 numbers,
// which are encoded with two base-64 digits each.
//
// Each non-final action is encoded as Action.encodeInt() + maxActionInt, while
// the final actin is just Action.encodeInt().
//
// A pass turn (an empty action list) is encoded as 2*maxActionInt instead.
// Note that maxActionInt < 2048 so this all fits nicely in base 4096 number.
//
export function formatCompactTurnHistory(turns: readonly Turn[]) {
    const ints = [];
    for (const {actions} of turns) {
        if (actions.length === 0) {
            ints.push(maxActionInt * 2);
        } else {
            for (let i = 0; i < actions.length; ++i) {
                ints.push(actions[i].encodeInt() + (i + 1 < actions.length ? maxActionInt : 0));
            }
        }
    }
    return ints.map(encodeBase4096).join('');
}

// Parses the compact turn history generated by formatCompactTurnHistory()
export function parseCompactTurnHistory(s: string): Turn[] {
    let i = 0;
    function nextInt() {
        const ss = s.substring(i, i + 2);
        i += 2;
        return decodeBase4096(ss);
    }
    const turns: Turn[] = [];
    while (i < s.length) {
        const actions: Action[] = [];
        let x = nextInt();
        if (x < 2*maxActionInt) {
            for (;;) {
                actions.push(Action.decodeInt(x % maxActionInt));
                if (x < maxActionInt) break;
                x = nextInt();
                if (x >= 2*maxActionInt) {
                    throw new Error(`Encoded integer ${x} out of range!`);
                }
            }
        } else {
            // pass
            if (x > 2*maxActionInt) {
                throw new Error(`Encoded integer ${x} out of range!`);
            }
        }
        turns.push(new Turn(actions));
    }
    return turns;
}
