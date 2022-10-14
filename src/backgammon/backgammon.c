#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"

/**
 * @brief 游戏状态
 */
typedef struct backgammon_game_t {
    backgammon_grid_t board[BACKGAMMON_NUM_POSITIONS]; /* 棋盘各个位置的信息 */
} backgammon_game_t;

static backgammon_grid_t backgammon_make_grid(backgammon_color_t color, int count) {
    backgammon_grid_t grid;
    grid.color = color;
    grid.count = count;
    return grid;
}

static backgammon_color_t backgammon_get_opponent(backgammon_color_t color) {
    return color == BACKGAMMON_WHITE ? BACKGAMMON_BLACK : BACKGAMMON_WHITE;
}

static int backgammon_get_bar_pos(backgammon_color_t color) {
    return color == BACKGAMMON_WHITE ? BACKGAMMON_WHITE_BAR_POS : BACKGAMMON_BLACK_BAR_POS;
}

static int backgammon_get_off_pos(backgammon_color_t color) {
    return color == BACKGAMMON_WHITE ? BACKGAMMON_WHITE_OFF_POS : BACKGAMMON_BLACK_OFF_POS;
}

static int backgammon_is_home_pos(backgammon_color_t color, int pos) {
    if (color == BACKGAMMON_WHITE) {
        return pos >= BACKGAMMON_BOARD_MIN_POS &&
               pos < BACKGAMMON_BOARD_MIN_POS + BACKGAMMON_NUM_HOME_POSITIONS;
    }
    return pos > BACKGAMMON_BOARD_MAX_POS - BACKGAMMON_NUM_HOME_POSITIONS &&
           pos <= BACKGAMMON_BOARD_MAX_POS;
}

static int backgammon_is_bar_pos(int pos) {
    return pos == BACKGAMMON_WHITE_BAR_POS || pos == BACKGAMMON_BLACK_BAR_POS;
}

static int backgammon_is_off_pos(int pos) {
    return pos == BACKGAMMON_WHITE_OFF_POS || pos == BACKGAMMON_BLACK_OFF_POS;
}

static int backgammon_add_moves(backgammon_color_t color, int pos, int moves, int *hit_off) {
    if (color == BACKGAMMON_WHITE) {
        pos -= moves;
        if (pos < BACKGAMMON_BOARD_MIN_POS) {
            *hit_off = pos == BACKGAMMON_BOARD_MIN_POS - 1;
            return BACKGAMMON_WHITE_OFF_POS;
        }
    } else {
        pos += moves;
        if (pos > BACKGAMMON_BOARD_MAX_POS) {
            *hit_off = pos == BACKGAMMON_BOARD_MAX_POS + 1;
            return BACKGAMMON_BLACK_OFF_POS;
        }
    }
    return pos;
}

backgammon_game_t *backgammon_game_new() {
    backgammon_game_t *game = (backgammon_game_t *)malloc(sizeof(backgammon_game_t));
    backgammon_game_reset(game);
    return game;
}

backgammon_game_t *backgammon_game_new_with_board(const backgammon_grid_t *grids,
                                                  const int *positions, size_t size) {
    backgammon_game_t *game = (backgammon_game_t *)malloc(sizeof(backgammon_game_t));
    memset(game, 0, sizeof(backgammon_game_t));
    for (size_t i = 0; i < size; ++i) {
        game->board[positions[i]] = grids[i];
    }
    return game;
}

void backgammon_game_reset(struct backgammon_game_t *game) {
    memset(game, 0, sizeof(backgammon_game_t));
    /* 白子初始位置 */
    game->board[BACKGAMMON_BOARD_MIN_POS + 23] = backgammon_make_grid(BACKGAMMON_WHITE, 2);
    game->board[BACKGAMMON_BOARD_MIN_POS + 12] = backgammon_make_grid(BACKGAMMON_WHITE, 5);
    game->board[BACKGAMMON_BOARD_MIN_POS + 7] = backgammon_make_grid(BACKGAMMON_WHITE, 3);
    game->board[BACKGAMMON_BOARD_MIN_POS + 5] = backgammon_make_grid(BACKGAMMON_WHITE, 5);
    /* 黑子初始位置 */
    game->board[BACKGAMMON_BOARD_MIN_POS + 0] = backgammon_make_grid(BACKGAMMON_BLACK, 2);
    game->board[BACKGAMMON_BOARD_MIN_POS + 11] = backgammon_make_grid(BACKGAMMON_BLACK, 5);
    game->board[BACKGAMMON_BOARD_MIN_POS + 16] = backgammon_make_grid(BACKGAMMON_BLACK, 3);
    game->board[BACKGAMMON_BOARD_MIN_POS + 18] = backgammon_make_grid(BACKGAMMON_BLACK, 5);
}

backgammon_game_t *backgammon_game_clone(const struct backgammon_game_t *game) {
    backgammon_game_t *new_game = (backgammon_game_t *)malloc(sizeof(backgammon_game_t));
    memcpy(new_game, game, sizeof(backgammon_game_t));
    return new_game;
}

void backgammon_game_free(backgammon_game_t *game) {
    if (game) {
        free(game);
    }
}

static backgammon_action_t *backgammon_append_move(backgammon_action_t *parent, int from, int to) {
    backgammon_action_t *dst;
    if (!parent->children) {
        parent->children = (backgammon_action_t *)malloc(sizeof(backgammon_action_t));
        memset(parent->children, 0, sizeof(backgammon_action_t));
        dst = parent->children;
    } else {
        dst = parent->children;
        while (dst->sibling) {
            dst = dst->sibling;
        }
        dst->sibling = (backgammon_action_t *)malloc(sizeof(backgammon_action_t));
        memset(dst->sibling, 0, sizeof(backgammon_action_t));
        dst = dst->sibling;
    }
    dst->from = from;
    dst->to = to;
    return dst;
}

static void backgammon_get_moves(const backgammon_game_t *game, backgammon_color_t color,
                                 backgammon_action_t *parent, const int *roll, int num_roll);

static void backgammon_try_get_moves(const backgammon_game_t *game, backgammon_color_t color,
                                     backgammon_action_t *parent, const int *roll, int num_roll,
                                     int from) {
    const int to = backgammon_game_can_move(game, color, from, roll[0]);
    if (to >= 0) {
        backgammon_game_t new_game;
        memcpy(&new_game, game, sizeof(backgammon_game_t));
        backgammon_game_move(&new_game, color, from, to);
        backgammon_action_t *node = backgammon_append_move(parent, from, to);
        if (num_roll > 1) {
            backgammon_get_moves(&new_game, color, node, roll + 1, num_roll - 1);
        }
    }
}

static void backgammon_get_moves(const backgammon_game_t *game, backgammon_color_t color,
                                 backgammon_action_t *parent, const int *roll, int num_roll) {
    const int bar_pos = backgammon_get_bar_pos(color);
    if (game->board[bar_pos].count > 0) {
        /* 需要先移动中间条上棋子 */
        backgammon_try_get_moves(game, color, parent, roll, num_roll, bar_pos);
        return;
    }
    for (int pos = 0; pos < BACKGAMMON_NUM_POSITIONS; ++pos) {
        if (backgammon_is_off_pos(pos) || backgammon_is_bar_pos(pos)) {
            continue;
        }
        backgammon_try_get_moves(game, color, parent, roll, num_roll, pos);
    }
}

backgammon_grid_t backgammon_game_get_grid(const backgammon_game_t *game, int pos) {
    assert(pos >= 0 && pos < BACKGAMMON_NUM_POSITIONS);
    return game->board[pos];
}

backgammon_action_t *backgammon_game_get_actions(const backgammon_game_t *game,
                                                 backgammon_color_t color, const int *roll) {
    backgammon_action_t *root = (backgammon_action_t *)malloc(sizeof(backgammon_action_t));
    memset(root, 0, sizeof(backgammon_action_t));
    if (roll[0] == roll[1]) {
        int duproll[BACKGAMMON_NUM_DICES * 2] = {roll[0], roll[0], roll[0], roll[0]};
        backgammon_get_moves(game, color, root, duproll, BACKGAMMON_NUM_DICES * 2);
    } else {
        int revroll[BACKGAMMON_NUM_DICES] = {roll[1], roll[0]};
        backgammon_get_moves(game, color, root, roll, BACKGAMMON_NUM_DICES);
        backgammon_get_moves(game, color, root, revroll, BACKGAMMON_NUM_DICES);
    }
    return root;
}

void backgammon_action_free(backgammon_action_t *tree) {
    if (tree) {
        for (backgammon_action_t *child = tree->children; !!child; child = child->sibling) {
            backgammon_action_free(child);
        }
        free(tree);
    }
}

void backgammon_action_visit(const backgammon_action_t *tree, backgammon_action_visitor visitor,
                             void *ctx) {
    const backgammon_action_t *path[32];
    int size = 0;
    path[size++] = tree;
    while (size > 0) {
        const backgammon_action_t *parent = path[size - 1];
        if (parent->children) {
            path[size++] = parent->children;
            continue;
        }
        visitor(ctx, path + 1, size - 1);
        while (!parent->sibling) {
            size--;
            if (size == 0) {
                return;
            }
            parent = path[size - 1];
        }
        path[size - 1] = parent->sibling;
    }
}

int backgammon_game_can_move(const backgammon_game_t *game, backgammon_color_t color, int from,
                             int steps) {
    /* from 位置越界检查 */
    if (from < 0 || from >= BACKGAMMON_NUM_POSITIONS) {
        return BACKGAMMON_ERR_MOVE_OUT_OF_RANGE;
    }
    if (backgammon_is_off_pos(from)) {
        return BACKGAMMON_ERR_MOVE_OUT_OF_RANGE;
    }
    if (from == backgammon_get_bar_pos(backgammon_get_opponent(color))) {
        return BACKGAMMON_ERR_MOVE_OUT_OF_RANGE;
    }
    /* to 位置越界检查 */
    int hit_off = 1;
    const int to = backgammon_add_moves(color, from, steps, &hit_off);
    if (to < 0 || to >= BACKGAMMON_NUM_POSITIONS) {
        return BACKGAMMON_ERR_MOVE_OUT_OF_RANGE;
    }
    if (backgammon_is_bar_pos(to)) {
        return BACKGAMMON_ERR_MOVE_OUT_OF_RANGE;
    }
    if (to == backgammon_get_off_pos(backgammon_get_opponent(color))) {
        return BACKGAMMON_ERR_MOVE_OUT_OF_RANGE;
    }
    /* 移动至原位置了 */
    if (from == to) {
        return BACKGAMMON_ERR_MOVE_TO_ORIGIN;
    }
    /* 当前位置为空 */
    if (game->board[from].count <= 0) {
        return BACKGAMMON_ERR_MOVE_EMPTY;
    }
    /* 当前位置不是己方棋子 */
    if (game->board[from].color != color) {
        return BACKGAMMON_ERR_MOVE_OPPONENT_CHECKER;
    }
    /* 中间条上有棋子时必须先移动中间的棋子 */
    const int bar_pos = backgammon_get_bar_pos(color);
    if (from != bar_pos && game->board[bar_pos].count > 0) {
        return BACKGAMMON_ERR_MOVE_BAR_NEEDED;
    }

    const int off_pos = backgammon_get_off_pos(color);
    if (to == off_pos) {
        if (!backgammon_game_can_bear_off(game, color)) {
            return BACKGAMMON_ERR_MOVE_CANNOT_BEAR_OFF;
        }
        if (!hit_off) {
            /* 如果不是恰好命中 off 位置，则必须要求没有比 from 距离 off 更远的棋子 */
            int begin_pos, end_pos, direction;
            if (color == BACKGAMMON_WHITE) {
                end_pos = BACKGAMMON_BOARD_MIN_POS + BACKGAMMON_NUM_HOME_POSITIONS;
                direction = +1;
                begin_pos = from + direction;
                assert(begin_pos <= end_pos);
            } else {
                end_pos = BACKGAMMON_BOARD_MAX_POS - BACKGAMMON_NUM_HOME_POSITIONS;
                direction = -1;
                begin_pos = from + direction;
                assert(begin_pos >= end_pos);
            }
            for (int pos = begin_pos; pos != end_pos; pos += direction) {
                if (game->board[pos].color == color && game->board[pos].count > 0) {
                    return BACKGAMMON_ERR_MOVE_CANNOT_BEAR_OFF;
                }
            }
        }
    } else if (game->board[to].color != color && game->board[to].count > 1) {
        return BACKGAMMON_ERR_MOVE_BLOCKED;
    }
    return to;
}

int backgammon_game_move(backgammon_game_t *game, backgammon_color_t color, int from, int to) {
    assert(from >= 0 && from < BACKGAMMON_NUM_POSITIONS);
    assert(to >= 0 && to < BACKGAMMON_NUM_POSITIONS);

    if (game->board[to].count == 0 || game->board[to].color == color) {
        /* 目标位置没有棋子或是己方棋子，则直接移动至目标位置即可 */
        game->board[to].color = color;
        game->board[to].count++;
        game->board[from].count--;
        return 0;
    }
    /* 敌方在此处恰有 1 个棋子，此时攻击敌方棋子到中间条上 */
    assert(game->board[to].count == 1);
    const int opponent = game->board[to].color;
    game->board[to].color = color;
    game->board[to].count = 1;
    game->board[from].count--;
    const int opponent_bar_pos = backgammon_get_bar_pos(opponent);
    game->board[opponent_bar_pos].color = opponent;
    game->board[opponent_bar_pos].count++;
    return 1;
}

int backgammon_game_can_bear_off(const backgammon_game_t *game, backgammon_color_t color) {
    const int bar_pos = backgammon_get_bar_pos(color);
    for (int pos = 0; pos < BACKGAMMON_NUM_POSITIONS; ++pos) {
        if (backgammon_is_off_pos(pos)) {
            continue;
        }
        if (pos == bar_pos && game->board[pos].count > 0) {
            return 0;
        }
        if (!backgammon_is_home_pos(color, pos) && game->board[pos].color == color &&
            game->board[pos].count > 0) {
            return 0;
        }
    }
    return 1;
}

backgammon_color_t backgammon_game_winner(const backgammon_game_t *game) {
    if (game->board[BACKGAMMON_WHITE_OFF_POS].count == BACKGAMMON_NUM_CHECKERS) {
        return BACKGAMMON_WHITE;
    }
    if (game->board[BACKGAMMON_BLACK_OFF_POS].count == BACKGAMMON_NUM_CHECKERS) {
        return BACKGAMMON_BLACK;
    }
    return BACKGAMMON_NOCOLOR;
}

int backgammon_game_encode(const backgammon_game_t *game, backgammon_color_t color, double *vec) {
    int offset = 0;
    backgammon_color_t colors[2] = {BACKGAMMON_WHITE, BACKGAMMON_BLACK};
    for (int i = 0; i < 2; ++i) {
        for (int pos = BACKGAMMON_BOARD_MIN_POS; pos <= BACKGAMMON_BOARD_MAX_POS; ++pos) {
            const int count = game->board[pos].color == colors[i] ? game->board[pos].count : 0;
            if (count < 4) {
                for (int j = 0; j < 4; ++j) {
                    vec[offset++] = j < count ? 1.0 : 0.0;
                }
            } else {
                vec[offset++] = 1.0;
                vec[offset++] = 1.0;
                vec[offset++] = 1.0;
                vec[offset++] = ((double)count - 3.0) / 2.0;
            }
        }
        vec[offset++] = (double)(game->board[backgammon_get_bar_pos(colors[i])].count) / 2.0;
        vec[offset++] = (double)(game->board[backgammon_get_off_pos(colors[i])].count) / 2.0;
    }
    if (color == BACKGAMMON_WHITE) {
        vec[offset++] = 1.0;
        vec[offset++] = 0.0;
    } else {
        vec[offset++] = 0.0;
        vec[offset++] = 1.0;
    }
    return offset;
}

#define backgammon_swap_double(x, y)                                                               \
    do {                                                                                           \
        double tmp = x;                                                                            \
        x = y;                                                                                     \
        y = tmp;                                                                                   \
    } while (0)

void backgammon_game_reverse_features(double *vec) {
    /**
     * 位置变换，颜色变换
     *
     * | 1 2 ........ 24 |b&o| 1 2 ........ 24 |b&o|cur|
     * |<-    4 * 24   ->| 2 |<-    4 * 24   ->| 2 | 2 |
     */
    const int num_positions = BACKGAMMON_BOARD_MAX_POS - BACKGAMMON_BOARD_MIN_POS + 1;
    for (int pos = BACKGAMMON_BOARD_MIN_POS; pos <= BACKGAMMON_BOARD_MAX_POS; ++pos) {
        const int i = 4 * (pos - BACKGAMMON_BOARD_MIN_POS);
        const int j = 4 * (BACKGAMMON_BOARD_MAX_POS - pos) + 4 * num_positions + 2;
        assert(i >= 0);
        assert(j >= 0);
        assert(i + 3 < 198);
        assert(j + 3 < 198);
        backgammon_swap_double(vec[i], vec[j]);
        backgammon_swap_double(vec[i + 1], vec[j + 1]);
        backgammon_swap_double(vec[i + 2], vec[j + 2]);
        backgammon_swap_double(vec[i + 3], vec[j + 3]);
    }
    backgammon_swap_double(vec[4 * num_positions], vec[8 * num_positions + 2]);
    backgammon_swap_double(vec[4 * num_positions + 1], vec[8 * num_positions + 3]);
    backgammon_swap_double(vec[196], vec[197]);
}

int backgammon_game_encode_action(const backgammon_game_t *game, backgammon_color_t color,
                                  const backgammon_action_t **path, int num_moves, double *vec) {
    backgammon_game_t new_game;
    memcpy(&new_game, game, sizeof(backgammon_game_t));
    for (int i = 0; i < num_moves; ++i) {
        backgammon_game_move(&new_game, color, path[i]->from, path[i]->to);
    }
    /* 执行动作后切换到对手角度进行状态编码 */
    return backgammon_game_encode(&new_game, backgammon_get_opponent(color), vec);
}

void backgammon_game_print(FILE *out, const backgammon_game_t *game) {
    char buf[128];
    int offset = 0;
    for (int i = 0; i < BACKGAMMON_NUM_POSITIONS; ++i) {
        buf[offset++] = ' ';
        if (game->board[i].count > 0) {
            if (game->board[i].color == BACKGAMMON_WHITE) {
                buf[offset++] = 'W';
            } else {
                buf[offset++] = 'B';
            }
            if (game->board[i].count < 10) {
                buf[offset++] = '0' + game->board[i].count;
            } else {
                buf[offset++] = 'A' + (game->board[i].count - 10);
            }
        } else {
            buf[offset++] = ' ';
            buf[offset++] = ' ';
        }
    }
    buf[offset] = 0;
    fprintf(out, "= %s\n", buf);
    offset = 0;
    for (int i = 0; i < BACKGAMMON_NUM_POSITIONS; ++i) {
        buf[offset++] = ' ';
        if (i < 10) {
            buf[offset++] = ' ';
        } else {
            buf[offset++] = '0' + (i / 10);
        }
        buf[offset++] = '0' + (i % 10);
    }
    buf[offset] = 0;
    fprintf(out, "= %s\n", buf);
}
