# Mytikas

An implementation of the board game Crossing Olympus.


# Release building

% cmake -B build -D CMAKE_BUILD_TYPE=Release
% make -C build all test


# Local development

```
% cmake -B build -D CMAKE_BUILD_TYPE=Debug -D CMAKE_EXPORT_COMPILE_COMMANDS=ON
% make -C build all test
```

Binary ends up in `build/apps/play`.
