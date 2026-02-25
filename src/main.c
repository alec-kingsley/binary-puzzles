#include "binary_puzzle.h"
#include <stdlib.h>
#include <time.h>

#define BOARD_SIZE 14

int main(void) {
    BinaryPuzzle *binary_puzzle;
    srand(time(NULL));
    binary_puzzle = binary_puzzle_create(BOARD_SIZE, BINARY_PUZZLE_MEDIUM);

    if (binary_puzzle != NULL) {
        binary_puzzle_print(binary_puzzle);
        binary_puzzle_destroy(binary_puzzle);
    }
    return 0;
}
