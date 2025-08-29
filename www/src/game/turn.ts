import { fieldCount, fieldNames, parseField } from "./board";
import { godCount, pantheon, type GodValue } from "./gods";

export const ActionType = {
    summon:  0,
    move:    1,
    attack:  2,
    special: 3,
};
export type ActionTypeValue = typeof ActionType[keyof typeof ActionType];

const passString = 'x';

const actionTypeNames = ['summon', 'move', 'attack', 'special'];
const actionTypeChars = "@>!+";

export class Action {
    type: ActionTypeValue;
    god: GodValue;
    field: number;

    constructor(type: ActionTypeValue, god: GodValue, field: number) {
        this.type = type;
        this.god = god;
        this.field = field;
    }

    // Encodes the current action as an integer between 0 and 4*12*41 == 1968 (exclusive).
    // This is useful for equality comparisons.
    encodeInt() {
        return ((this.type * godCount) + this.god) * fieldCount + this.field;
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
