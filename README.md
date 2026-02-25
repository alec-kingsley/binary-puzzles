# Binary Puzzle Generator

Generates [binary puzzles](https://en.wikipedia.org/wiki/Takuzu)

![demo](./assets/demo.gif)

## Building

Run `make` to build. The resulting binary will be in `./bin/binary_puzzle`.

By default, the binary will animate the creation process of puzzle. To disable this
(and significantly speed up generation time), remove the `-DDEBUG` flag from the `Makefile`.

## Changing Board Settings

See `main.c` to alter the board size (must be an even number greater than 0 and less than 256)
and difficulty (must be `BINARY_PUZZLE_EASY`, `BINARY_PUZZLE_MEDIUM`, or `BINARY_PUZZLE_HARD`)