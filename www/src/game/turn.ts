import { coordsToField, parseField } from "./board";
import { pantheon, type GodValue } from "./gods";
import type { PlayerValue } from "./player";

export type ActionTypeValue = 'summon' | 'move' | 'attack' | 'special';

export type Action = {
    type: ActionTypeValue;
    god: GodValue;
    field: number;
};

export type Turn = Action[];

const actionTypeChars = "@>!+";
const actionTypeNames = ['summon', 'move', 'attack', 'special'];

export function parseAction(s: string): Action {
    if (s.length !== 4) {
        throw new Error(`Invalid action length: ${s.length} (expected: 4)`);
    }
    const godId = s.charAt(0);
    const typeId = s.charAt(1);
    const fieldId = s.substring(2);
    const god = pantheon.findIndex((god) => god.id === s.charAt(0))
    if (god === -1) {
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
    return {
        type: typeId as ActionTypeValue,
        god: god as GodValue,
        field: fieldIndex,
    };
}

export function parseTurnString(s: string): Turn {
    if (s === 'x') return [];
    return s.split(',').map(parseAction);
}

export function parseTurnStrings(turnStrings: string[]): Turn[] {
    return turnStrings.map(parseTurnString);
}