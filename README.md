# Binary Puzzle Generator

Generates valid solution boards for [binary puzzles](https://en.wikipedia.org/wiki/Takuzu)

## Building

Run `make` to build. The resulting binary will be in `./bin/binary_puzzle`.

By default, the binary will animate the creation process of the solution board. To disable this
(and significantly speed up generation time), remove the `-DDEBUG` flag from the `Makefile`.
