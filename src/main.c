#include "binary_puzzle.h"
#include <stdlib.h>
#include <time.h>

int main(void) {
    BinaryPuzzle *binary_puzzle;
    srand(time(NULL));
    binary_puzzle = binary_puzzle_create(14);

    if (binary_puzzle != NULL) {
#ifndef DEBUG
        binary_puzzle_print(binary_puzzle);
#endif
        binary_puzzle_destroy(binary_puzzle);
    }
    return 0;
}
