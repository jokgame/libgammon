#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"

static backgammon_grid_t backgammon_make_grid(backgammon_color_t color, int count) {
    backgammon_grid_t grid;
    grid.color = color;
    grid.count = count;
    return grid;
}

static int backgammon_get_bar_pos(backgammon_color_t color) {
    return color == BACKGAMMON_WHITE ? BACKGAMMON_WHITE_BAR_POS : BACKGAMMON_BLACK_BAR_POS;
}

static int backgammon_get_off_pos(backgammon_color_t color) {
    return color == BACKGAMMON_WHITE ? BACKGAMMON_WHITE_OFF_POS : BACKGAMMON_BLACK_OFF_POS;
}

static int backgammon_is_home_pos(backgammon_color_t color, int pos) {
    if (color == BACKGAMMON_WHITE) {
        return pos >= 1 && pos <= 6;
    }
    return pos >= 19 && pos <= 24;
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
        if (pos < 1) {
            *hit_off = pos == 0;
            return BACKGAMMON_WHITE_OFF_POS;
        }
    } else {
        pos += moves;
        if (pos > 24) {
            *hit_off = pos == 25;
            return BACKGAMMON_BLACK_OFF_POS;
        }
    }
    return pos;
}

static int backgammon_can_move(const backgammon_game_t *game, backgammon_color_t color, int from,
                               int to, int hit_off) {
    if (from == to) {
        return 0;
    }
    /* 当前位置为空或者不是己方棋子 */
    if (game->board[from].count <= 0 || game->board[from].color != color) {
        return 0;
    }
    /* 中间条上有棋子时必须先移动中间的棋子 */
    const int bar_pos = backgammon_get_bar_pos(color);
    if (from != bar_pos && game->board[bar_pos].count > 0) {
        return 0;
    }

    const int off_pos = backgammon_get_off_pos(color);
    if (to == off_pos) {
        if (!backgammon_game_can_bear_off(game, color)) {
            return 0;
        }
        if (!hit_off) {
            /* 在可以 bear-off 时，如果不是恰好命中 off 位置，则必须要求没有距离 off 更远的棋子 */
            if (color == BACKGAMMON_WHITE) {
                for (int pos = from + 1; pos <= 6; ++pos) {
                    if (game->board[pos].color == color && game->board[pos].count > 0) {
                        return 0;
                    }
                }
            } else {
                for (int pos = from - 1; pos >= 19; --pos) {
                    if (game->board[pos].color == color && game->board[pos].count > 0) {
                        return 0;
                    }
                }
            }
        }
    } else if (game->board[to].color != color && game->board[to].count > 1) {
        return 0;
    }
    return 1;
}

backgammon_game_t *backgammon_game_new() {
    backgammon_game_t *game = (backgammon_game_t *)malloc(sizeof(backgammon_game_t));
    memset(game, 0, sizeof(backgammon_game_t));
    game->board[1] = backgammon_make_grid(BACKGAMMON_BLACK, 2);
    game->board[12] = backgammon_make_grid(BACKGAMMON_BLACK, 5);
    game->board[17] = backgammon_make_grid(BACKGAMMON_BLACK, 3);
    game->board[19] = backgammon_make_grid(BACKGAMMON_BLACK, 5);
    game->board[24] = backgammon_make_grid(BACKGAMMON_WHITE, 2);
    game->board[13] = backgammon_make_grid(BACKGAMMON_WHITE, 5);
    game->board[8] = backgammon_make_grid(BACKGAMMON_WHITE, 3);
    game->board[6] = backgammon_make_grid(BACKGAMMON_WHITE, 5);
    return game;
}

void backgammon_game_free(backgammon_game_t *game) {
    if (game) {
        free(game);
    }
}

backgammon_grid_t backgammon_game_get_grid(const backgammon_game_t *game, int pos) {
    assert(pos >= 0 && pos < BACKGAMMON_NUM_POSITIONS);
    return game->board[pos];
}

static backgammon_action_t *backgammon_append_move(backgammon_action_t *parent, int from, int to) {
    backgammon_action_t *dst;
    if (!parent->children) {
        parent->children = (backgammon_action_t *)malloc(sizeof(backgammon_action_t));
        memset(parent->children, 0, sizeof(backgammon_action_t));
        dst = parent->children;
    } else {
        // TODO(wangjin): 是否有必要在 move 节点中记录最右侧的 sibling 以避免遍历?
        dst = parent->children;
        while (dst->sibling) {
            dst = dst->sibling;
        }
        dst->sibling = (backgammon_action_t *)malloc(sizeof(backgammon_action_t));
        memset(dst->sibling, 0, sizeof(backgammon_action_t));
        dst = dst->sibling;
    }
    dst->parent = parent;
    dst->move.from = from;
    dst->move.to = to;
    return dst;
}

#define backgammon_try_get_moves(game, color, parent, roll, num_roll, from, to, precise)           \
    do {                                                                                           \
        if (backgammon_can_move(game, color, from, to, hit_off)) {                                 \
            backgammon_game_t new_game;                                                            \
            memcpy(&new_game, game, sizeof(backgammon_game_t));                                    \
            backgammon_game_move(&new_game, color, from, to);                                      \
            backgammon_action_t *node = backgammon_append_move(parent, from, to);                  \
            if (num_roll > 1) {                                                                    \
                backgammon_get_moves(&new_game, color, node, roll + 1, num_roll - 1);              \
            }                                                                                      \
        }                                                                                          \
    } while (0)

static void backgammon_get_moves(const backgammon_game_t *game, backgammon_color_t color,
                                 backgammon_action_t *parent, const int *roll, int num_roll) {
    const int bar_pos = backgammon_get_bar_pos(color);
    if (game->board[bar_pos].count > 0) {
        /* 需要先移动中间条上棋子 */
        int hit_off = 1;
        const int to = backgammon_add_moves(color, bar_pos, roll[0], &hit_off);
        backgammon_try_get_moves(game, color, parent, roll, num_roll, bar_pos, to, hit_off);
        return;
    }
    for (int pos = 0; pos < BACKGAMMON_NUM_POSITIONS; ++pos) {
        if (backgammon_is_off_pos(pos) || backgammon_is_bar_pos(pos)) {
            continue;
        }
        int hit_off = 1;
        const int to = backgammon_add_moves(color, pos, roll[0], &hit_off);
        backgammon_try_get_moves(game, color, parent, roll, num_roll, pos, to, hit_off);
    }
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

backgammon_error_t backgammon_game_move(backgammon_game_t *game, backgammon_color_t color, int from,
                                        int to) {
    assert(from >= 0 && from < BACKGAMMON_NUM_POSITIONS && !backgammon_is_off_pos(from));
    if (!(to >= 1 && to < BACKGAMMON_NUM_POSITIONS && !backgammon_is_bar_pos(to))) {
        printf("to=%d\n", to);
        int *x = 0;
        *x = 0;
    }

    /* 当前位置为空或者不是己方棋子 */
    if (game->board[from].count <= 0) {
        return BACKGAMMON_ERR_MOVE_EMPTY;
    }
    if (game->board[from].color != color) {
        return BACKGAMMON_ERR_MOVE_OPPONENT_CHECKER;
    }

    /* 中间条上有棋子时必须先移动中间的棋子 */
    const int bar_pos = backgammon_get_bar_pos(color);
    if (from != bar_pos && game->board[bar_pos].count > 0) {
        return BACKGAMMON_ERR_MOVE_BAR_NEEDED;
    }

    if (game->board[to].count == 0 || game->board[to].color == color) {
        /* 目标位置没有棋子或是己方棋子，则直接移动至目标位置即可 */
        game->board[to].color = color;
        game->board[to].count++;
        game->board[from].count--;
    } else {
        /* 敌方在此处有多个棋子时不可移动到此处 */
        if (game->board[to].count > 1) {
            return BACKGAMMON_ERR_MOVE_BLOCKED;
        }
        /* 敌方在此处恰有 1 个棋子，此时攻击敌方棋子到中间条上 */
        const int opponent = game->board[to].color;
        game->board[to].color = color;
        game->board[to].count = 1;
        game->board[from].count--;
        const int opponent_bar_pos = backgammon_get_bar_pos(opponent);
        game->board[opponent_bar_pos].color = opponent;
        game->board[opponent_bar_pos].count++;
    }
    return BACKGAMMON_OK;
}

bool backgammon_game_can_bear_off(const backgammon_game_t *game, backgammon_color_t color) {
    const int bar_pos = backgammon_get_bar_pos(color);
    for (int pos = 0; pos < BACKGAMMON_NUM_POSITIONS; ++pos) {
        if (backgammon_is_off_pos(pos)) {
            continue;
        }
        if (pos == bar_pos && game->board[pos].count > 0) {
            return false;
        }
        if (!backgammon_is_home_pos(color, pos) && game->board[pos].color == color &&
            game->board[pos].count > 0) {
            return false;
        }
    }
    return true;
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

bool backgammon_game_encode(const backgammon_game_t *game, backgammon_color_t color, double *vec) {
    int offset = 0;
    backgammon_color_t colors[2] = {BACKGAMMON_WHITE, BACKGAMMON_BLACK};
    for (int i = 0; i < 2; ++i) {
        const backgammon_color_t color = colors[i];
        for (int pos = 1; pos <= 24; ++pos) {
            const int count = game->board[pos].color == color ? game->board[pos].count : 0;
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
        vec[offset++] = (double)(game->board[backgammon_get_bar_pos(color)].count) / 2.0;
        vec[offset++] = (double)(game->board[backgammon_get_off_pos(color)].count) / 2.0;
    }
    if (color == BACKGAMMON_WHITE) {
        vec[offset++] = 1.0;
        vec[offset++] = 0.0;
    } else {
        vec[offset++] = 0.0;
        vec[offset++] = 1.0;
    }
    assert(offset == 198);
    return true;
}

bool backgammon_game_encode_action(const backgammon_game_t *game, backgammon_color_t color,
                                   const backgammon_move_t *moves, int num_moves, double *vec) {
    backgammon_game_t new_game;
    memcpy(&new_game, game, sizeof(backgammon_game_t));
    for (int i = 0; i < num_moves; ++i) {
        const int err = backgammon_game_move(&new_game, color, moves[i].from, moves[i].to);
        if (err != BACKGAMMON_OK) {
            return false;
        }
    }
    return backgammon_game_encode(&new_game, color, vec);
}
