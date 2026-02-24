#ifndef BINARY_PUZZLE_H
#define BINARY_PUZZLE_H

#include <stdint.h>

typedef struct BinaryPuzzle BinaryPuzzle;

/**
 * Print contents of `BinaryPuzzle`.
 */
void binary_puzzle_print(BinaryPuzzle *self);

/**
 * Create a new `BinaryPuzzle` object.
 *
 * Return `NULL` on failure
 */
BinaryPuzzle *binary_puzzle_create(uint8_t size);

/**
 * Destroy the `BinaryPuzzle`.
 */
void binary_puzzle_destroy(BinaryPuzzle *self);

#endif
