#include "binary_puzzle.h"
#include "reporter.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#include "colors.h"
#endif

#define FILENAME "binary_puzzle.c"

struct BinaryPuzzle {
    uint8_t size;
    bool **solution;
};

/**
 * Initialize binary puzzle with random values.
 *
 * Return `true` iff successful.
 */
static bool binary_puzzle_initialize(BinaryPuzzle *self);

void binary_puzzle_print(BinaryPuzzle *self) {
    size_t i, j;

    for (i = 0; i < self->size; i++) {
        for (j = 0; j < self->size; j++) {
            printf(self->solution[i][j] ? "1 " : "0 ");
        }
        printf("\n");
    }
}

typedef enum { CELL_ZERO, CELL_ONE, CELL_INVALID, CELL_UNKNOWN } cell_state_t;

static cell_state_t binary_puzzle_get_cell_state(BinaryPuzzle *self,
                                                 bool **initialized, size_t i,
                                                 size_t j) {
    if (i < self->size && j < self->size) {
        if (initialized[i][j]) {
            return self->solution[i][j] ? CELL_ONE : CELL_ZERO;
        }
    }
    return CELL_UNKNOWN;
}

static cell_state_t cell_state_combine(size_t cell_ct, ...) {
    va_list ap;
    size_t i;
    cell_state_t current_cell_state;
    cell_state_t result = CELL_UNKNOWN;
    va_start(ap, cell_ct);
    for (i = 0; i < cell_ct; i++) {
        current_cell_state = va_arg(ap, cell_state_t);

        if (result != CELL_INVALID) {
            if (current_cell_state == CELL_ONE && result == CELL_ZERO) {
                result = CELL_INVALID;
            } else if (current_cell_state == CELL_ZERO && result == CELL_ONE) {
                result = CELL_INVALID;
            } else if (result == CELL_UNKNOWN) {
                result = current_cell_state;
            }
        }
    }
    va_end(ap);

    return result;
}

static cell_state_t binary_puzzle_check_3_rule(BinaryPuzzle *self,
                                               bool **initialized, size_t i,
                                               size_t j) {
    int8_t dir;
    int8_t di, dj;
    cell_state_t primary_neighbor, secondary_neighbor;
    cell_state_t expected_result, result = CELL_UNKNOWN;
    bool opposite;
    for (dir = 0; dir < 6; dir++) {
        di = (dir == 0 || dir == 4) ? 1 : dir == 2 ? -1 : 0;
        dj = (dir == 1 || dir == 5) ? 1 : dir == 3 ? -1 : 0;
        opposite = dir == 4 || dir == 5;
        primary_neighbor
            = binary_puzzle_get_cell_state(self, initialized, i + di, j + dj);
        if (opposite) {
            secondary_neighbor = binary_puzzle_get_cell_state(self, initialized,
                                                              i - di, j - dj);
        } else {
            secondary_neighbor = binary_puzzle_get_cell_state(
                self, initialized, i + 2 * di, j + 2 * dj);
        }

        if (primary_neighbor == secondary_neighbor
            && primary_neighbor != CELL_UNKNOWN) {
            expected_result
                = primary_neighbor == CELL_ZERO ? CELL_ONE : CELL_ZERO;

            result = cell_state_combine(2, result, expected_result);
        }
    }
    return result;
}

static cell_state_t binary_puzzle_check_evenness_rule(BinaryPuzzle *self,
                                                      bool **initialized,
                                                      size_t i, size_t j) {
    size_t k;
    size_t one_ct = 0, zero_ct = 0;
    cell_state_t column_cell_state = CELL_UNKNOWN,
                 row_cell_state = CELL_UNKNOWN;
    cell_state_t cell_state;
    for (k = 0; k < self->size; k++) {
        cell_state = binary_puzzle_get_cell_state(self, initialized, i, k);
        one_ct += cell_state == CELL_ONE ? 1 : 0;
        zero_ct += cell_state == CELL_ZERO ? 1 : 0;
    }
    if (2 * one_ct == self->size) {
        row_cell_state = CELL_ZERO;
    } else if (2 * zero_ct == self->size) {
        row_cell_state = CELL_ONE;
    }
    one_ct = 0;
    zero_ct = 0;
    for (k = 0; k < self->size; k++) {
        cell_state = binary_puzzle_get_cell_state(self, initialized, k, j);
        one_ct += cell_state == CELL_ONE ? 1 : 0;
        zero_ct += cell_state == CELL_ZERO ? 1 : 0;
    }
    if (2 * one_ct == self->size) {
        column_cell_state = CELL_ZERO;
    } else if (2 * zero_ct == self->size) {
        column_cell_state = CELL_ONE;
    }
    return cell_state_combine(2, column_cell_state, row_cell_state);
}

static cell_state_t binary_puzzle_check_uniqueness_rule(BinaryPuzzle *self,
                                                        bool **initialized,
                                                        size_t i, size_t j) {
    size_t k, l;
    bool unique;

    /* yeah GOOD LUCK updating THIS code */
    for (k = 0; k < self->size; k++) {
        if (k == i)
            goto skip_row_check;
        unique = false;
        for (l = 0; l < self->size && !unique; l++) {
            if (l == j)
                continue;
            if (!initialized[i][l] || !initialized[k][l]) {
                unique = true;
            } else if (self->solution[i][l] != self->solution[k][l]) {
                unique = true;
            }
        }
        if (!unique) {
            return CELL_INVALID;
        }
    skip_row_check:
        if (k == j)
            continue;
        unique = false;
        for (l = 0; l < self->size && !unique; l++) {
            if (l == i)
                continue;
            if (!initialized[l][j] || !initialized[l][k]) {
                unique = true;
            }
            if (self->solution[l][j] != self->solution[l][k]) {
                unique = true;
            }
        }
        if (!unique) {
            return CELL_INVALID;
        }
    }

    return CELL_UNKNOWN;
}

static float binary_puzzle_get_one_probability(BinaryPuzzle *self,
                                               bool **initialized, size_t i,
                                               size_t j) {
    size_t k;
    uint8_t row_ones_needed = self->size / 2;
    uint8_t row_zeroes_needed = self->size / 2;
    uint8_t col_ones_needed = self->size / 2;
    uint8_t col_zeroes_needed = self->size / 2;
    uint16_t one_straws;
    uint16_t zero_straws;

    for (k = 0; k < self->size; k++) {
        if (initialized[i][k]) {
            if (self->solution[i][k]) {
                row_ones_needed--;
            } else {
                row_zeroes_needed--;
            }
        }
        if (initialized[k][j]) {
            if (self->solution[k][j]) {
                col_ones_needed--;
            } else {
                col_zeroes_needed--;
            }
        }
    }
    one_straws = row_ones_needed * col_ones_needed;
    zero_straws = row_zeroes_needed * col_zeroes_needed;
    return (1.0f * one_straws) / (one_straws + zero_straws);
}

#ifdef DEBUG
#pragma GCC push_options
#pragma GCC optimize("O0")
static void binary_puzzle_print_initialization_frame(BinaryPuzzle *self,
                                                     bool **initialized) {
    size_t i, j;
    printf(CLRSCRN);

    for (i = 0; i < self->size; i++) {
        for (j = 0; j < self->size; j++) {
            if (initialized[i][j]) {
                printf(GREEN "%s " RESET, self->solution[i][j] ? "1" : "0");
            } else {
                printf(RED "X " RESET);
            }
        }
        printf("\n");
    }
    /* sleep */
    for (i = 0; i < 20000000; i++) {
    }
}
#pragma GCC pop_options
#endif

static bool binary_puzzle_solve(BinaryPuzzle *self, bool **initialized);

static float dramaticity(float probability) {
    return probability < 0.5 ? 1 - probability : probability;
}

static bool binary_puzzle_guess(BinaryPuzzle *self, bool **initialized) {
    size_t most_dramatic_i = 0, most_dramatic_j = 0;
    size_t i, j;
    float most_dramatic_one_probability = 0.5;
    float one_probability;
    bool contender_found = false;
    cell_state_t cell_state;

    for (i = 0; i < self->size; i++) {
        for (j = 0; j < self->size; j++) {
            if (!initialized[i][j]) {
                one_probability = binary_puzzle_get_one_probability(
                    self, initialized, i, j);
                if (!contender_found
                    || dramaticity(one_probability)
                           > dramaticity(most_dramatic_one_probability)) {
                    contender_found = true;
                    most_dramatic_one_probability = one_probability;
                    most_dramatic_i = i;
                    most_dramatic_j = j;
                }
            }
        }
    }

    cell_state = (1.0 * rand()) / RAND_MAX < most_dramatic_one_probability
                     ? CELL_ONE
                     : CELL_ZERO;

    self->solution[most_dramatic_i][most_dramatic_j] = cell_state == CELL_ONE;
    initialized[most_dramatic_i][most_dramatic_j] = true;
#ifdef DEBUG
    binary_puzzle_print_initialization_frame(self, initialized);
#endif
    if (!binary_puzzle_solve(self, initialized)) {
        self->solution[most_dramatic_i][most_dramatic_j]
            = cell_state != CELL_ONE;
#ifdef DEBUG
        binary_puzzle_print_initialization_frame(self, initialized);
#endif
        if (!binary_puzzle_solve(self, initialized)) {
            return false;
        }
    }
    return true;
}

static bool binary_puzzle_solve(BinaryPuzzle *self, bool **initialized) {
    size_t i, j;
    /* cell states according to the three rules */
    cell_state_t cell_state_a, cell_state_b, cell_state_c;
    /* actual cell state */
    cell_state_t cell_state;
    bool invalid_state = false;
    bool *frame_contents;
    bool **frame_initialized = calloc(self->size, sizeof(bool *));
    bool updated, has_remaining_cells;
    if (frame_initialized == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        return false;
    }

    frame_contents = calloc(self->size * self->size, sizeof(bool));
    if (frame_contents == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        free(frame_initialized);
        return false;
    }

    for (i = 0; i < self->size; i++) {
        frame_initialized[i] = frame_contents + i * self->size * sizeof(bool);
        for (j = 0; j < self->size; j++) {
            frame_initialized[i][j] = initialized[i][j];
        }
    }

    do {
        updated = false;
        has_remaining_cells = false;
        for (i = 0; i < self->size; i++) {
            for (j = 0; j < self->size; j++) {
                if (!frame_initialized[i][j]) {
                    /* check 3-in-a-row rule */
                    cell_state_a = binary_puzzle_check_3_rule(
                        self, frame_initialized, i, j);

                    /* check half per row/column rule */
                    cell_state_b = binary_puzzle_check_evenness_rule(
                        self, frame_initialized, i, j);

                    /* check matching row/column rule */
                    cell_state_c = binary_puzzle_check_uniqueness_rule(
                        self, frame_initialized, i, j);

                    cell_state = cell_state_combine(3, cell_state_a,
                                                    cell_state_b, cell_state_c);
                    if (cell_state == CELL_ONE || cell_state == CELL_ZERO) {
                        self->solution[i][j] = cell_state == CELL_ONE;
                        frame_initialized[i][j] = true;
                        updated = true;
#ifdef DEBUG
                        binary_puzzle_print_initialization_frame(
                            self, frame_initialized);
#endif
                    } else {
                        has_remaining_cells = true;
                        if (cell_state == CELL_INVALID) {
                            invalid_state = true;
                            goto binary_puzzle_solve_done;
                        }
                    }
                }
            }
        }
    } while (updated);

    if (!invalid_state && has_remaining_cells) {
        invalid_state = !binary_puzzle_guess(self, frame_initialized);
    }

binary_puzzle_solve_done:
    free(frame_contents);
    free(frame_initialized);
    return !invalid_state;
}

/**
 * Initialize binary puzzle. Return false on failure.
 */
static bool binary_puzzle_initialize(BinaryPuzzle *self) {
    size_t i;
    bool successful;
    bool *contents;
    bool **initialized = calloc(self->size, sizeof(bool *));
    if (initialized == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        return false;
    }

    contents = calloc(self->size * self->size, sizeof(bool));
    if (contents == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        free(initialized);
        return false;
    }

    for (i = 0; i < self->size; i++) {
        initialized[i] = contents + i * self->size * sizeof(bool);
    }

    successful = binary_puzzle_solve(self, initialized);
    free(contents);
    free(initialized);
    return successful;
}

BinaryPuzzle *binary_puzzle_create(uint8_t size) {
    BinaryPuzzle *new = NULL;
    bool *solution_contents;
    size_t i;

    if (size == 0 || size % 2 != 0) {
        report_logic_error(
            "cannot initialize binary puzzle with 0 or odd size");
        exit(1);
    }

    new = calloc(1, sizeof(BinaryPuzzle));
    if (new == NULL)
        goto binary_puzzle_create_fail;

    new->size = size;
    new->solution = calloc(size, sizeof(bool *));
    if (new->solution == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        goto binary_puzzle_create_fail;
    }

    solution_contents = calloc(size * size, sizeof(bool));
    if (solution_contents == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        goto binary_puzzle_create_fail;
    }

    for (i = 0; i < size; i++) {
        new->solution[i] = solution_contents + i * size * sizeof(bool);
    }

    if (!binary_puzzle_initialize(new)) {
        goto binary_puzzle_create_fail;
    }

    return new;

binary_puzzle_create_fail:
    report_system_error(FILENAME ": failure to initialize");
    binary_puzzle_destroy(new);
    return NULL;
}

void binary_puzzle_destroy(BinaryPuzzle *self) {
    if (self != NULL) {
        if (self->solution != NULL) {
            free(*self->solution);
            free(self->solution);
        }
        free(self);
    }
}
