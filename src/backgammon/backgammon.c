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

backgammon_grid_t backgammon_game_get_grid(const backgammon_game_t *game, int pos) {
    assert(pos >= 0 && pos < BACKGAMMON_NUM_POSITIONS);
    return game->board[pos];
}

void backgammon_game_set_grid(struct backgammon_game_t *game, int pos, backgammon_grid_t grid) {
    game->board[pos] = grid;
}

typedef struct backgamme_game_key_t {
    uint64_t first;
    uint64_t second;
} backgammon_game_key_t;

static uint64_t backgammon_game_key_hash(backgammon_game_key_t key) {
    return key.first ^ (key.second << 1);
}

static void set_game_key_bit(backgammon_game_key_t *key, int offset, int value) {
    uint64_t *data;
    if (offset < 64) {
        data = &(key->first);
    } else {
        offset -= 64;
        data = &(key->second);
    }
    uint64_t mask = 1 << offset;
    if (value == 1) {
        *data = (*data) | mask;
    } else {
        *data = (*data) & (~mask);
    }
}

static int backgammon_is_same_key(backgammon_game_key_t key1, backgammon_game_key_t key2) {
    return key1.first == key2.first && key1.second == key2.second ? 1 : 0;
}

backgammon_game_key_t backgammon_game_key(const backgammon_game_t *game) {
    backgammon_game_key_t key;
    memset(&key, 0, sizeof(key));
    int offset = 0;
    for (int pos = 0; pos < BACKGAMMON_NUM_POSITIONS; pos++) {
        backgammon_grid_t grid = backgammon_game_get_grid(game, pos);
        if (pos >= BACKGAMMON_BOARD_MIN_POS && pos <= BACKGAMMON_BOARD_MAX_POS) {
            // marshal color using 1 bit
            set_game_key_bit(&key, offset, grid.color == BACKGAMMON_WHITE);
            offset++;
        }
        // marshal count (0~15) using 4 bits
        for (int b = 0; b < 4; b++) {
            set_game_key_bit(&key, offset, ((grid.count > b) & 0x1) == 0);
            offset++;
        }
    }
    return key;
}

typedef struct backgammon_linked_list_t {
    backgammon_game_key_t key;
    void *value;
    struct backgammon_linked_list_t *next;
} backgammon_linked_list_t;

static void backgammon_linked_list_free(backgammon_linked_list_t *list) {
    if (list == NULL) {
        return;
    }
    backgammon_linked_list_free(list->next);
    free(list);
}

typedef struct backgammon_hash_map_t {
    backgammon_linked_list_t **buckets;
    size_t num_buckets;
} backgammon_hash_map_t;

static size_t upper_power_2(size_t v) {
    if (v == 0) {
        return v;
    }
    assert(v > 0);
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    assert((v & (v - 1)) == 0);
    return v;
}

static void backgammon_hash_map_init(backgammon_hash_map_t *map, size_t hint) {
    map->num_buckets = upper_power_2(hint);
    if (map->num_buckets == 0) {
        map->num_buckets = 128;
    }
    size_t space = sizeof(backgammon_linked_list_t *) * map->num_buckets;
    map->buckets = (backgammon_linked_list_t **)malloc(space);
    memset(map->buckets, 0, space);
}

static void backgammon_hash_map_free(backgammon_hash_map_t *map) {
    for (size_t i = 0; i < map->num_buckets; i++) {
        backgammon_linked_list_free(map->buckets[i]);
    }
    free(map->buckets);
}

static void backgammon_hash_map_set(backgammon_hash_map_t *map, backgammon_game_key_t key,
                                    void *value) {
    uint64_t hash = backgammon_game_key_hash(key);
    size_t index = hash & (uint64_t)(map->num_buckets - 1);
    backgammon_linked_list_t *node = map->buckets[index];
    if (node == NULL) {
        node = (backgammon_linked_list_t *)malloc(sizeof(backgammon_linked_list_t));
        memset(node, 0, sizeof(backgammon_linked_list_t));
        node->key = key;
        node->value = value;
        map->buckets[index] = node;
        return;
    }
    while (1) {
        if (backgammon_is_same_key(node->key, key)) {
            node->value = value;
            return;
        }
        if (node->next == NULL) {
            break;
        }
        node = node->next;
    }
    node->next = (backgammon_linked_list_t *)malloc(sizeof(backgammon_linked_list_t));
    memset(node->next, 0, sizeof(backgammon_linked_list_t));
    node->next->key = key;
    node->next->value = value;
}

static backgammon_action_t *backgammon_append_move(backgammon_action_t *parent, int from, int steps,
                                                   int to) {
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
    dst->move.from = from;
    dst->move.steps = steps;
    dst->move.to = to;
    dst->parent = parent;
    return dst;
}

static void backgammon_get_moves(const backgammon_game_t *game, backgammon_color_t color,
                                 backgammon_action_t *parent, const int *roll, int num_roll,
                                 backgammon_hash_map_t *map);

static void backgammon_try_get_moves_from(const backgammon_game_t *game, backgammon_color_t color,
                                          backgammon_action_t *parent, const int *roll,
                                          int num_roll, int from, backgammon_hash_map_t *map) {
    const int to = backgammon_game_can_move_from(game, color, from, roll[0]);
    if (to >= 0) {
        backgammon_game_t new_game;
        memcpy(&new_game, game, sizeof(backgammon_game_t));
        backgammon_game_move(&new_game, color, from, to);
        backgammon_action_t *node = backgammon_append_move(parent, from, roll[0], to);
        if (num_roll > 1) {
            backgammon_get_moves(&new_game, color, node, roll + 1, num_roll - 1, map);
        }
        if (node->children == NULL && map != NULL) {
            backgammon_hash_map_set(map, backgammon_game_key(&new_game), node);
        }
    }
}

static void backgammon_get_moves(const backgammon_game_t *game, backgammon_color_t color,
                                 backgammon_action_t *parent, const int *roll, int num_roll,
                                 backgammon_hash_map_t *map) {
    const int bar_pos = backgammon_get_bar_pos(color);
    if (game->board[bar_pos].count > 0) {
        /* 需要先移动中间条上棋子 */
        backgammon_try_get_moves_from(game, color, parent, roll, num_roll, bar_pos, map);
    } else {
        for (int pos = BACKGAMMON_BOARD_MIN_POS; pos <= BACKGAMMON_BOARD_MAX_POS; ++pos) {
            backgammon_try_get_moves_from(game, color, parent, roll, num_roll, pos, map);
        }
    }
}

static backgammon_action_t *backgammon_game_get_actions_with_map(const backgammon_game_t *game,
                                                                 backgammon_color_t color,
                                                                 int roll1, int roll2,
                                                                 backgammon_hash_map_t *map) {
    backgammon_action_t *root = (backgammon_action_t *)malloc(sizeof(backgammon_action_t));
    memset(root, 0, sizeof(backgammon_action_t));
    if (roll1 == roll2) {
        int duproll[BACKGAMMON_NUM_DICES * 2] = {roll1, roll1, roll1, roll1};
        backgammon_get_moves(game, color, root, duproll, BACKGAMMON_NUM_DICES * 2, map);
    } else {
        int roll[BACKGAMMON_NUM_DICES] = {roll1, roll2};
        int revroll[BACKGAMMON_NUM_DICES] = {roll2, roll1};
        backgammon_get_moves(game, color, root, roll, BACKGAMMON_NUM_DICES, map);
        backgammon_get_moves(game, color, root, revroll, BACKGAMMON_NUM_DICES, map);
    }
    return root;
}

backgammon_action_t *backgammon_game_get_actions(const backgammon_game_t *game,
                                                 backgammon_color_t color, int roll1, int roll2) {
    return backgammon_game_get_actions_with_map(game, color, roll1, roll2, NULL);
}

int backgammon_game_get_non_equivalent_actions(const struct backgammon_game_t *game,
                                               backgammon_move_t *result, backgammon_color_t color,
                                               int roll1, int roll2) {
    int n = 0;
    backgammon_hash_map_t map;
    backgammon_hash_map_init(&map, roll1 == roll2 ? 64 : 16);
    backgammon_action_t *root =
        backgammon_game_get_actions_with_map(game, color, roll1, roll2, &map);
    backgammon_move_t empty;
    memset(&empty, 0, sizeof(empty));
    for (size_t i = 0; i < map.num_buckets; i++) {
        backgammon_linked_list_t *node = map.buckets[i];
        while (node != NULL) {
            backgammon_action_t *action = (backgammon_action_t *)(node->value);
            result[n++] = empty;
            while (action != NULL && action->parent != NULL) {
                result[n++] = action->move;
                action = action->parent;
            }
            node = node->next;
        }
    }
    for (int i = 0; i + i < n; i++) {
        int j = n - i - 1;
        backgammon_move_t temp = result[i];
        result[i] = result[j];
        result[j] = temp;
    }
    backgammon_action_free(root);
    backgammon_hash_map_free(&map);
    return n;
}

void backgammon_action_free(backgammon_action_t *tree) {
    if (tree) {
        for (backgammon_action_t *child = tree->children; !!child;) {
            backgammon_action_t *next = child->sibling;
            backgammon_action_free(child);
            child = next;
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

int backgammon_game_can_move_from(const backgammon_game_t *game, backgammon_color_t color, int from,
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

int backgammon_game_can_move(const backgammon_game_t *game, backgammon_color_t color, int steps) {
    for (int pos = BACKGAMMON_BOARD_MIN_POS; pos <= BACKGAMMON_BOARD_MAX_POS; ++pos) {
        if (backgammon_game_can_move_from(game, color, pos, steps) >= 0) {
            return 1;
        }
    }
    if (backgammon_game_can_move_from(game, color, backgammon_get_bar_pos(color), steps) >= 0) {
        return 1;
    }
    return 0;
}

int backgammon_game_move(backgammon_game_t *game, backgammon_color_t color, int from, int to) {
    assert(from >= 0 && from < BACKGAMMON_NUM_POSITIONS);
    assert(to >= 0 && to < BACKGAMMON_NUM_POSITIONS);
    assert(game->board[from].color == color);
    assert(game->board[from].count > 0);

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

backgammon_result_t backgammon_game_result(const backgammon_game_t *game) {
    backgammon_result_t result;
    result.winner = BACKGAMMON_NOCOLOR;
    result.kind = BACKGAMMON_WIN_NORMAL;
    if (game->board[BACKGAMMON_WHITE_OFF_POS].count == BACKGAMMON_NUM_CHECKERS) {
        result.winner = BACKGAMMON_WHITE;
        for (int i = 0; i <= 6; i++) {
            if (game->board[i].count > 0 && game->board[i].color == BACKGAMMON_BLACK) {
                result.kind = BACKGAMMON_WIN_BACKGAMMON;
                return result;
            }
        }
        if (game->board[BACKGAMMON_BLACK_OFF_POS].count == 0) {
            result.kind = BACKGAMMON_WIN_GAMMON;
        }
    } else if (game->board[BACKGAMMON_BLACK_OFF_POS].count == BACKGAMMON_NUM_CHECKERS) {
        result.winner = BACKGAMMON_BLACK;
        for (int i = 19; i <= 25; i++) {
            if (game->board[i].count > 0 && game->board[i].color == BACKGAMMON_WHITE) {
                result.kind = BACKGAMMON_WIN_BACKGAMMON;
                return result;
            }
        }
        if (game->board[BACKGAMMON_WHITE_OFF_POS].count == 0) {
            result.kind = BACKGAMMON_WIN_GAMMON;
        }
    }
    return result;
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
        vec[offset++] = (double)(game->board[backgammon_get_off_pos(colors[i])].count) / 15.0;
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

#undef backgammon_swap_double

int backgammon_game_encode_action(const backgammon_game_t *game, backgammon_color_t color,
                                  const backgammon_action_t **path, int num_moves, double *vec) {
    backgammon_game_t new_game;
    memcpy(&new_game, game, sizeof(backgammon_game_t));
    for (int i = 0; i < num_moves; ++i) {
        backgammon_game_move(&new_game, color, path[i]->move.from, path[i]->move.to);
    }
    /* 执行动作后切换到对手角度进行状态编码 */
    return backgammon_game_encode(&new_game, backgammon_get_opponent(color), vec);
}

int backgammon_game_encode_moves(const struct backgammon_game_t *game, backgammon_color_t color,
                                 const backgammon_move_t *moves, int num_moves, double *vec) {
    backgammon_game_t new_game;
    memcpy(&new_game, game, sizeof(backgammon_game_t));
    for (int i = 0; i < num_moves; ++i) {
        backgammon_game_move(&new_game, color, moves[i].from, moves[i].to);
    }
    /* 执行动作后切换到对手角度进行状态编码 */
    return backgammon_game_encode(&new_game, backgammon_get_opponent(color), vec);
}

void backgammon_game_print(FILE *out, const backgammon_game_t *game) {
    char buf[128];
    int n = backgammon_game_to_string(buf, game);
    buf[n] = 0;
    fprintf(out, "%s\n", buf);
}

int backgammon_game_to_string(char *buf, const backgammon_game_t *game) {
    int offset = 0;
    for (int i = 0; i < BACKGAMMON_NUM_POSITIONS; ++i) {
        if (game->board[i].count == 0) {
            continue;
        }
        if (offset > 0) {
            buf[offset++] = ' ';
        }
        if (i < 10) {
            buf[offset++] = '0' + i;
        } else {
            buf[offset++] = '0' + i / 10;
            buf[offset++] = '0' + i % 10;
        }
        buf[offset++] = ':';
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
    }
    return offset;
}
