export function chooseRandomly<T>(values: T[]): T {
    if (values.length === 0) {
        throw new Error('Cannot choose from an empty list!');
    }
    return values[Math.floor(Math.random() * values.length)];
}

export function zip2<T1, T2> (a: T1[], b: T2[]): [T1, T2][] {
    return Array.from(Array(Math.max(b.length, a.length)), (_, i) => [a[i], b[i]]);
}

export function classNames(obj: Record<string, unknown>): string {
    return Object.entries(obj)
        .filter(([_, value]) => Boolean(value))
        .map(([key, _]) => key)
        .join(' ');
}
