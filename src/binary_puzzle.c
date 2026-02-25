#include "binary_puzzle.h"
#include "colors.h"
#include "reporter.h"
#include "string_builder.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define FILENAME "binary_puzzle.c"

typedef enum { CELL_ZERO, CELL_ONE, CELL_INVALID, CELL_UNKNOWN } cell_state_t;

struct BinaryPuzzle {
    uint8_t size;
    bool **solution;
    /* false values in mask represent hidden values in solution */
    bool **mask;

    cell_state_t **user_guesses;

    uint8_t i_selected;
    uint8_t j_selected;
};

/**
 * Initialize binary puzzle with random values.
 *
 * Return `true` iff successful.
 */
static bool binary_puzzle_initialize(BinaryPuzzle *self);

static void **get_empty_board(BinaryPuzzle *self, uint8_t member_size,
                              uint8_t default_value) {
    size_t i;
    void *contents;
    void **board = calloc(self->size, sizeof(void *));
    if (board == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        return NULL;
    }

    contents = calloc(self->size * self->size, member_size);
    if (contents == NULL) {
        report_system_error(FILENAME ": memory allocation failure");
        free(board);
        return NULL;
    }

    for (i = 0; i < self->size; i++) {
        board[i] = (uint8_t*)contents + i * self->size * member_size;
    }

    if (default_value != 0) {
        for (i = 0; i < self->size * self->size; i++) {
            *((uint8_t*)contents + i * member_size) = default_value;
        }
    }

    return board;
}

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

struct {
    uint16_t row_ct;
    uint16_t col_ct;
    struct termios orig_termios;
} g_term;

static void disable_raw_mode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_term.orig_termios) == -1) {
        printf(CLEAR_SCREEN RESET_CURSOR SHOW_CURSOR);
        report_system_error(FILENAME ": failed to disable raw mode");
        exit(1);
    }
}

static void update_window_size(void) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        report_system_error(FILENAME ": failed to get window size");
        exit(1);
    }

    g_term.col_ct = ws.ws_col;
    g_term.row_ct = ws.ws_row;
}

static void enable_raw_mode(void) {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &g_term.orig_termios) == -1) {
        report_system_error(FILENAME ": failed to get terminal attributes");
        exit(1);
    }
    atexit(disable_raw_mode);

    raw = g_term.orig_termios;

    /* disable ctrl+s and ctrl+q */
    raw.c_iflag &= ~IXON;

    /* ensure 8th bit preserved */
    raw.c_iflag &= ~ISTRIP;

    /* disable ctrl+v */
    raw.c_lflag &= ~IEXTEN;

    /* disable showing charcters as they're typed */
    raw.c_lflag &= ~ECHO;

    /* disable canonical mode (read byte-by-byte) */
    raw.c_lflag &= ~ICANON;

    /* disable output processing */
    raw.c_oflag &= ~OPOST;

    /* ensure 8 bits per character */
    raw.c_cflag |= CS8;

    /* set minimum bytes to read for read() to return */
    raw.c_cc[VMIN] = 0;

    /* time in deciseconds before read() should return */
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        report_system_error(FILENAME ": failed to enter raw mode");
        exit(1);
    }
}

static void binary_puzzle_update_screen(BinaryPuzzle *self) {
    const uint16_t min_row_ct = self->size * 3;
    const uint16_t min_col_ct = self->size * 5;
    uint16_t i, j;
    uint8_t row, col;
    StringBuilder *contents = string_builder_create();
    const char *pls_expand_screen = "Screen size too small";
    string_builder_set(contents, CLEAR_SCREEN RESET_CURSOR HIDE_CURSOR);
    update_window_size();
    if (g_term.row_ct >= min_row_ct && g_term.col_ct >= min_col_ct) {
        for (row = 0; 2 * row < g_term.row_ct - min_row_ct; row++) {
            string_builder_append(contents, "\r\n");
        }

        for (i = 0; i < self->size; i++) {
            /* top */
            for (col = 0; 2 * col < g_term.col_ct - min_col_ct; col++) {
                string_builder_append(contents, " ");
            }
            for (j = 0; j < self->size; j++) {
                if (i == self->i_selected && j == self->j_selected) {
                    string_builder_append(contents, "╔═══╗");
                } else {
                    string_builder_append(contents, "┌───┐");
                }
            }
            string_builder_append(contents, "\r\n");

            /* middle */
            for (col = 0; 2 * col < g_term.col_ct - min_col_ct; col++) {
                string_builder_append(contents, " ");
            }
            for (j = 0; j < self->size; j++) {
                if (i == self->i_selected && j == self->j_selected) {
                    string_builder_append(contents, "║ ");
                } else {
                    string_builder_append(contents, "│ ");
                }
                if (!self->mask[i][j]) {
                    switch (self->user_guesses[i][j]) {
                    case CELL_ZERO:
                        string_builder_append(contents, CYAN "0" RESET);
                        break;
                    case CELL_ONE:
                        string_builder_append(contents, CYAN "1" RESET);
                        break;
                    case CELL_UNKNOWN:
                        string_builder_append(contents, BLUE "_" RESET);
                        break;
                    default:
                        string_builder_append(contents, RED "?" RESET);
                        break;
                    }
                } else {
                    string_builder_append(contents, self->solution[i][j]
                                                        ? GREEN "1" RESET
                                                        : GREEN "0" RESET);
                }

                if (i == self->i_selected && j == self->j_selected) {
                    string_builder_append(contents, " ║");
                } else {
                    string_builder_append(contents, " │");
                }
            }
            string_builder_append(contents, "\r\n");

            /* bottom */
            for (col = 0; 2 * col < g_term.col_ct - min_col_ct; col++) {
                string_builder_append(contents, " ");
            }
            for (j = 0; j < self->size; j++) {
                if (i == self->i_selected && j == self->j_selected) {
                    string_builder_append(contents, "╚═══╝");
                } else {
                    string_builder_append(contents, "└───┘");
                }
            }
            if (g_term.row_ct != min_row_ct || i != self->size) {
                string_builder_append(contents, "\r\n");
            }
        }
    } else {
        for (row = 0; 2 * row < g_term.row_ct; row++) {
            string_builder_append(contents, "\r\n");
        }
        if (g_term.col_ct >= strlen(pls_expand_screen)) {
            for (col = 0; 2 * col < g_term.col_ct - strlen(pls_expand_screen);
                 col++) {
                string_builder_append(contents, " ");
            }
            string_builder_append(contents, pls_expand_screen);
        }
    }
    write(STDOUT_FILENO, string_builder_to_string(contents),
          string_builder_len(contents));
    string_builder_destroy(contents);
}

void binary_puzzle_interactive(BinaryPuzzle *self) {
    char key;
    int read_status;
    bool keep_playing = true;
    enable_raw_mode();
    self->user_guesses = (cell_state_t **)get_empty_board(
        self, sizeof(cell_state_t), CELL_UNKNOWN);

    while (keep_playing) {
        binary_puzzle_update_screen(self);
        read_status = read(STDIN_FILENO, &key, 1);
        if (read_status == 1) {
            switch (key) {
            case 'q':
                printf(CLEAR_SCREEN RESET_CURSOR SHOW_CURSOR);
                keep_playing = false;
                break;
            case 'h':
                self->j_selected += self->size - 1;
                self->j_selected %= self->size;
                break;
            case 'j':
                self->i_selected++;
                self->i_selected %= self->size;
                break;
            case 'k':
                self->i_selected += self->size - 1;
                self->i_selected %= self->size;
                break;
            case 'l':
                self->j_selected++;
                self->j_selected %= self->size;
                break;
            case '\n':
            case ' ':
                switch (
                    self->user_guesses[self->i_selected][self->j_selected]) {
                case CELL_UNKNOWN:
                    self->user_guesses[self->i_selected][self->j_selected]
                        = CELL_ZERO;
                    break;
                case CELL_ZERO:
                    self->user_guesses[self->i_selected][self->j_selected]
                        = CELL_ONE;
                    break;
                case CELL_ONE:
                    self->user_guesses[self->i_selected][self->j_selected]
                        = CELL_UNKNOWN;
                    break;
                default:
                    break;
                }
                break;
            case '0':
                self->user_guesses[self->i_selected][self->j_selected]
                    = CELL_ZERO;
                break;
            case '1':
                self->user_guesses[self->i_selected][self->j_selected]
                    = CELL_ONE;
                break;
            default:
                break;
            }
        } else if (read_status == -1 && errno != EAGAIN) {
            report_system_error(FILENAME ": failed to get user input");
            exit(1);
        }
    }
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
            } else if (result == CELL_UNKNOWN
                       || current_cell_state == CELL_INVALID) {
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

#pragma GCC push_options
#pragma GCC optimize("O0")
static void binary_puzzle_print_initialization_frame(BinaryPuzzle *self,
                                                     bool **initialized,
                                                     bool sleep) {
    size_t i, j;
    for (i = 0; i < self->size; i++) {
        for (j = 0; j < self->size; j++) {
            if (!self->mask[i][j]) {
                printf(BLUE "? " RESET);
            } else if (initialized == NULL || initialized[i][j]) {
                printf(GREEN "%s " RESET, self->solution[i][j] ? "1" : "0");
            } else {
                printf(RED "X " RESET);
            }
        }
        printf("\n");
    }
    /* sleep */
    if (sleep) {
        for (i = 0; i < 20000000; i++) {
        }
    }
}
#pragma GCC pop_options

void binary_puzzle_print(BinaryPuzzle *self) {
    binary_puzzle_print_initialization_frame(self, NULL, false);
}

typedef enum {
    SOLVE_SUCCESS,
    SOLVE_OUT_OF_GUESSES,
    SOLVE_REACHED_INVALID,
    SOLVE_SYSTEM_ERROR
} solve_status_t;

static solve_status_t
binary_puzzle_initialize_solution(BinaryPuzzle *self, bool **initialized,
                                  uint16_t allowed_guesses);

static cell_state_t binary_puzzle_get_expected_cell_state(BinaryPuzzle *self,
                                                          bool **initialized,
                                                          size_t i, size_t j);

static float dramaticity(float probability) {
    return probability < 0.5 ? 1 - probability : probability;
}

static solve_status_t
binary_puzzle_make_probable_guess(BinaryPuzzle *self, bool **initialized,
                                  uint16_t allowed_guesses) {
    size_t most_dramatic_i = 0, most_dramatic_j = 0;
    size_t i, j;
    float most_dramatic_one_probability = 0.5;
    float one_probability;
    bool contender_found = false;
    cell_state_t cell_state;
    solve_status_t solve_status;

    if (allowed_guesses == 0)
        return SOLVE_OUT_OF_GUESSES;
    if (allowed_guesses != UINT16_MAX)
        allowed_guesses--;

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
    if (allowed_guesses == UINT16_MAX)
        binary_puzzle_print_initialization_frame(self, initialized, true);
#endif
    if ((solve_status = binary_puzzle_initialize_solution(self, initialized,
                                                          allowed_guesses))
        != SOLVE_SUCCESS) {
        self->solution[most_dramatic_i][most_dramatic_j]
            = cell_state != CELL_ONE;
#ifdef DEBUG
        if (allowed_guesses == UINT16_MAX)
            binary_puzzle_print_initialization_frame(self, initialized, true);
#endif
        solve_status = binary_puzzle_initialize_solution(self, initialized,
                                                         allowed_guesses);
    }
    return solve_status;
}

static cell_state_t binary_puzzle_get_expected_cell_state(BinaryPuzzle *self,
                                                          bool **initialized,
                                                          size_t i, size_t j) {
    /* cell states according to the three rules */
    cell_state_t cell_state_a, cell_state_b, cell_state_c;
    /* check 3-in-a-row rule */
    cell_state_a = binary_puzzle_check_3_rule(self, initialized, i, j);

    /* check half per row/column rule */
    cell_state_b = binary_puzzle_check_evenness_rule(self, initialized, i, j);

    /* check matching row/column rule */
    cell_state_c = binary_puzzle_check_uniqueness_rule(self, initialized, i, j);

    return cell_state_combine(3, cell_state_a, cell_state_b, cell_state_c);
}

static solve_status_t
binary_puzzle_initialize_solution(BinaryPuzzle *self, bool **initialized,
                                  uint16_t allowed_guesses) {
    size_t i, j;
    /* actual cell state */
    cell_state_t cell_state;
    solve_status_t solve_status = SOLVE_SUCCESS;
    bool **frame_initialized
        = (bool **)get_empty_board(self, sizeof(bool), false);
    bool updated, has_remaining_cells;
    if (frame_initialized == NULL) {
        return SOLVE_SYSTEM_ERROR;
    }

    for (i = 0; i < self->size; i++) {
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
                    cell_state = binary_puzzle_get_expected_cell_state(
                        self, frame_initialized, i, j);
                    if (cell_state == CELL_ONE || cell_state == CELL_ZERO) {
                        self->solution[i][j] = cell_state == CELL_ONE;
                        frame_initialized[i][j] = true;
                        updated = true;
#ifdef DEBUG
                        if (allowed_guesses == UINT16_MAX)
                            binary_puzzle_print_initialization_frame(
                                self, frame_initialized, true);
#endif
                    } else {
                        has_remaining_cells = true;
                        if (cell_state == CELL_INVALID) {
                            solve_status = SOLVE_REACHED_INVALID;
                            goto binary_puzzle_solve_done;
                        }
                    }
                }
            }
        }
    } while (updated);

    if (solve_status == SOLVE_SUCCESS && has_remaining_cells) {
        solve_status = binary_puzzle_make_probable_guess(
            self, frame_initialized, allowed_guesses);
    }

binary_puzzle_solve_done:
    free(*frame_initialized);
    free(frame_initialized);
    return solve_status;
}

/**
 * Initialize binary puzzle. Return false on failure.
 */
static bool binary_puzzle_initialize(BinaryPuzzle *self) {
    bool successful;
    bool **initialized = (bool **)get_empty_board(self, sizeof(bool), false);
    if (initialized == NULL) {
        return false;
    }

    successful
        = binary_puzzle_initialize_solution(self, initialized, UINT16_MAX);

    free(*initialized);
    free(initialized);
    return successful;
}

static bool binary_puzzle_can_mask(BinaryPuzzle *self, size_t i, size_t j,
                                   uint16_t allowed_guesses) {
    bool **fake_initialized
        = (bool **)get_empty_board(self, sizeof(bool), false);
    bool **fake_solution = (bool **)get_empty_board(self, sizeof(bool), false);
    bool **real_solution;
    size_t k, l;
    cell_state_t cell_state;
    bool can_mask;
    for (k = 0; k < self->size; k++) {
        for (l = 0; l < self->size; l++) {
            if (self->mask[k][l]) {
                fake_initialized[k][l] = true;
                fake_solution[k][l] = self->solution[k][l];
            }
        }
    }
    fake_initialized[i][j] = false;
    real_solution = self->solution;
    self->solution = fake_solution;
    cell_state
        = binary_puzzle_get_expected_cell_state(self, fake_initialized, i, j);
    if ((cell_state == CELL_ONE && real_solution[i][j])
        || (cell_state == CELL_ZERO && !real_solution[i][j])) {
        can_mask = true;
    } else {
        fake_initialized[i][j] = true;
        fake_solution[i][j] = !real_solution[i][j];

        can_mask = binary_puzzle_initialize_solution(self, fake_initialized,
                                                     allowed_guesses)
                   == SOLVE_REACHED_INVALID;
    }
    self->solution = real_solution;
    free(*fake_initialized);
    free(fake_initialized);
    return can_mask;
}

static bool
binary_puzzle_initialize_mask(BinaryPuzzle *self,
                              binary_puzzle_difficulty_t difficulty) {
    const uint16_t allowed_guesses = difficulty == BINARY_PUZZLE_EASY     ? 0
                                     : difficulty == BINARY_PUZZLE_MEDIUM ? 3
                                                                          : 8;
    size_t i, j;
    bool **contenders = (bool **)get_empty_board(self, sizeof(bool), true);
    size_t contender_ct = self->size * self->size;
    size_t contender_idx;
    if (contenders == NULL) {
        return false;
    }

    while (contender_ct > 0) {
    apply_next_mask:
        contender_idx = (1.0 * rand() / RAND_MAX) * contender_ct;
        for (i = 0; i < self->size; i++) {
            for (j = 0; j < self->size; j++) {
                if (contenders[i][j]) {
                    if (contender_idx == 0) {
                        contenders[i][j] = false;
                        contender_ct--;

                        if (!binary_puzzle_can_mask(self, i, j,
                                                    allowed_guesses)) {
                            goto apply_next_mask;
                        }

                        self->mask[i][j] = false;
#ifdef DEBUG
                        binary_puzzle_print_initialization_frame(self, NULL,
                                                                 true);
#endif
                        goto apply_next_mask;
                    }
                    contender_idx--;
                }
            }
        }
    }

    free(*contenders);
    free(contenders);
    return true;
}

BinaryPuzzle *binary_puzzle_create(uint8_t size,
                                   binary_puzzle_difficulty_t difficulty) {
    BinaryPuzzle *new = NULL;

    if (size == 0 || size % 2 != 0) {
        report_logic_error(
            "cannot initialize binary puzzle with 0 or odd size");
        exit(1);
    }

    new = calloc(1, sizeof(BinaryPuzzle));
    if (new == NULL)
        goto binary_puzzle_create_fail;

    new->size = size;
    new->solution = (bool **)get_empty_board(new, sizeof(bool), false);
    if (new->solution == NULL)
        goto binary_puzzle_create_fail;

    new->mask = (bool **)get_empty_board(new, sizeof(bool), true);
    if (new->mask == NULL)
        goto binary_puzzle_create_fail;

    if (binary_puzzle_initialize(new) != SOLVE_SUCCESS) {
        goto binary_puzzle_create_fail;
    }

    if (!binary_puzzle_initialize_mask(new, difficulty)) {
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
        if (self->mask != NULL) {
            free(*self->mask);
            free(self->mask);
        }
        if (self->user_guesses != NULL) {
            free(*self->user_guesses);
            free(self->user_guesses);
        }
        free(self);
    }
}
