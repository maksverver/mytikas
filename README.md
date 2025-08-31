# Mytikas

An implementation of the board game Crossing Olympus.

More information about the game:

  * https://www.boldmovegames.com/
  * https://boardgamegeek.com/boardgame/242375/crossing-olympus


# Rules

See [RULES.md](./RULES.md) for a summary of the rules.


# Release building

```
% cmake -B build -D CMAKE_BUILD_TYPE=Release
% make -C build all test
```


# Local development

```
% cmake -B build -D CMAKE_BUILD_TYPE=Debug -D CMAKE_EXPORT_COMPILE_COMMANDS=ON
% make -C build all test
```

Binary ends up in `build/apps/play`.

Example invocation:

```
% apps/play minimax,max_depth=2 random
```
