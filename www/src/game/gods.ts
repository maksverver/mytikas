export const God = Object.freeze({
   zeus:        0,
   hephaestus:  1,
   hera:        2,
   poseidon:    3,
   apollo:      4,
   aphrodite:   5,
   ares:        6,
   hermes:      7,
   dionysus:    8,
   artemis:     9,
   hades:      10,
   athena:     11,
});

export const godCount = 12;

export type GodValue =  typeof God[keyof typeof God];

export const StatusEffects = Object.freeze({
    none:          0,
    chained:       1,
    damageBoost:   2,
    speedBoost:    4,
    shielded:      8,
});

export type StatusEffectsValue = 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15;

export const Dirs = Object.freeze({
    none:       0,
    orthogonal: 1,
    diagonal:   2,
    all8:       3,  // orthogonal | diagonal
    knight:     4,
    direct:     8,
});

export type DirsValue = 0|1|2|3|4|9|10|11|12;

// Keep this in sync with `pantheon` in src/state.cc
export const pantheon = (function() {
    const NO_DIRS = 0;  // TODO
    const ALL8    = 0;  // TODO
    const ORTHO   = 0;  // TODO
    const DIAG    = 0;  // TODO
    const KNIGHT  = 0;  // TODO
    const DIRECT = (d: number) => d;  // TODO

   function f(
         name: string,
         id: string,
         emoji: string,
         hit: number,
         mov: number,
         dmg: number,
         rng: number,
         mov_dirs: number,
         atk_dirs: number,
         aura: number) {
      return Object.freeze({name, id, emoji, hit, mov, dmg, rng, mov_dirs, atk_dirs, aura});
   }

    return Object.freeze([
        // name        id  emoji  hit mov dmg rng  mov_dirs         atk_dirs         aura
        f("Zeus",       'Z', "⚡️", 10,   1, 10,  3, ALL8,            DIRECT(ORTHO),  StatusEffects.none       ),
        f("Hephaestus", 'H', "🔨",  9,   2,  7,  2, ORTHO,           DIRECT(ORTHO),  StatusEffects.damageBoost),
        f("hEra",       'E', "👸",  8,   2,  5,  2, DIAG,            DIAG,           StatusEffects.none       ),
        f("Poseidon",   'P', "🔱",  7,   3,  4,  0, ORTHO,           NO_DIRS,        StatusEffects.none       ),
        f("apOllo",     'O', "🏹",  6,   2,  2,  3, ALL8,            ALL8,           StatusEffects.none       ),
        f("Aphrodite",  'A', "🌹",  6,   3,  6,  1, ALL8,            ALL8,           StatusEffects.none       ),
        f("aRes",       'R', "⚔️",  5,   3,  5,  3, DIRECT(ALL8),    DIRECT(ALL8),   StatusEffects.none       ),
        f("herMes",     'M', "🪽",  5,   3,  3,  2, ALL8,            DIRECT(ALL8),   StatusEffects.speedBoost ),
        f("Dionysus",   'D', "🍇",  4,   1,  4,  0, KNIGHT,          NO_DIRS,        StatusEffects.none       ),
        f("arTemis",    'T', "🦌",  4,   2,  4,  2, ALL8,            DIRECT(DIAG),   StatusEffects.none       ),
        f("hadeS",      'S', "🐕",  3,   3,  3,  1, DIRECT(ALL8),    NO_DIRS,        StatusEffects.none       ),
        f("atheNa",     'N', "🛡️",  3,   1,  3,  3, ALL8,            DIRECT(ALL8),   StatusEffects.shielded   ),
    ]);
})();
