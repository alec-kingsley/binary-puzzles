#ifndef BINARY_PUZZLE_H
#define BINARY_PUZZLE_H

#include <stdint.h>

typedef struct BinaryPuzzle BinaryPuzzle;

typedef enum {
    BINARY_PUZZLE_EASY,
    BINARY_PUZZLE_MEDIUM,
    BINARY_PUZZLE_HARD
} binary_puzzle_difficulty_t;

/**
 * Enter interactive solver.
 */
void binary_puzzle_interactive(BinaryPuzzle *self);

/**
 * Print contents of `BinaryPuzzle`.
 */
void binary_puzzle_print(BinaryPuzzle *self);

/**
 * Create a new `BinaryPuzzle` object.
 *
 * Return `NULL` on failure
 */
BinaryPuzzle *binary_puzzle_create(uint8_t size, binary_puzzle_difficulty_t difficulty);

/**
 * Destroy the `BinaryPuzzle`.
 */
void binary_puzzle_destroy(BinaryPuzzle *self);

#endif
