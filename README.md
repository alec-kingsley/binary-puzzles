# Binary Puzzle Generator

Generates interactive [binary puzzles](https://en.wikipedia.org/wiki/Takuzu)

![demo](./assets/demo.gif)

## Building

Run `make` to build. The resulting binary will be in `./bin/binary_puzzle`.

To animate the creation of the puzzle like shown above, add the `-DDEBUG` flag to `CFLAGS`  in the `Makefile`.

## Controls

`h`, `j`, `k` and `l` for movement

`q` to exit

`space` or `enter` to cycle cell options

`0` or `1` to explicitly set a cell

## Changing Board Settings

See `main.c` to alter the board size (must be an even number greater than 0 and less than 256,
though board sizes greater than around 50 will take annoyingly long to generate)
and difficulty (must be `BINARY_PUZZLE_EASY`, `BINARY_PUZZLE_MEDIUM`, or `BINARY_PUZZLE_HARD`)

## Todo

- Add win detection
- Add help menu
