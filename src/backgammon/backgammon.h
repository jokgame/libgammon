#ifndef _BACKGAMMON_H_
#define _BACKGAMMON_H_

#include <stddef.h>
#include <stdio.h>

/**
 * @brief 棋子颜色
 */
typedef enum backgammon_color_t {
    BACKGAMMON_NOCOLOR = 0,
    BACKGAMMON_WHITE = 1,
    BACKGAMMON_BLACK = 2,
} backgammon_color_t;

/**
 * @brief 错误码
 */
typedef enum backgammon_error_t {
    BACKGAMMON_OK = 0,                         /* 成功 */
    BACKGAMMON_ERR_MOVE_TO_ORIGIN = -1,        /* 移动至原位置 */
    BACKGAMMON_ERR_MOVE_EMPTY = -2,            /* 移动空位置 */
    BACKGAMMON_ERR_MOVE_OPPONENT_CHECKER = -3, /* 移动敌方棋子 */
    BACKGAMMON_ERR_MOVE_BAR_NEEDED = -4,       /* 需要先移动 bar 上的棋子 */
    BACKGAMMON_ERR_MOVE_BLOCKED = -5,          /* 目标位置被敌方占领 */
    BACKGAMMON_ERR_MOVE_OUT_OF_RANGE = -6,     /* 移动越界 */
    BACKGAMMON_ERR_MOVE_CANNOT_BEAR_OFF = -7,  /* 不可 bear off */
} backgammon_error_t;

/**
 * @brief 棋盘位置编号
 *
 *    13 14 15 16 17 18  25   19 20 21 22 23 24 27
 *   +-=--=--=--=--=--=-+-=-+-=--=--=--=--=--=-+-=-+
 *   | o           x    |   | x              o |   |
 *   | o           x    |   | x              o |   |
 *   | o           x    |   | x  BLACK HOME    |   |
 *   | o                |   | x                |   |
 *   | o                | B | x                | O |
 *   +------------------+ A +------------------+ F |
 *   | x                | R | o                | F |
 *   | x                |   | o                |   |
 *   | x           o    |   | o  WHITE HOME    |   |
 *   | x           o    |   | o              x |   |
 *   | x           o    |   | o              x |   |
 *   +-=--=--=--=--=--=-+-=-+-=--=--=--=--=--=-+-=-+
 *    12 11 10  9  8  7   0   6  5  4  3  2  1  26
 *
 * BLACK: x
 * WHITE: o
 *
 */
#define BACKGAMMON_BLACK_BAR_POS 0      /* 黑方中间条位置 */
#define BACKGAMMON_BOARD_MIN_POS 1      /* 棋盘最小位置 */
#define BACKGAMMON_BOARD_MAX_POS 24     /* 棋盘最大位置 */
#define BACKGAMMON_WHITE_BAR_POS 25     /* 白方中间条位置 */
#define BACKGAMMON_WHITE_OFF_POS 26     /* 白方槽位置 */
#define BACKGAMMON_BLACK_OFF_POS 27     /* 黑方槽位置 */
#define BACKGAMMON_NUM_POSITIONS 28     /* 总位置数量 */
#define BACKGAMMON_NUM_HOME_POSITIONS 6 /* Home 区域位置数量 */

#define BACKGAMMON_NUM_DICES 2     /* 骰子数量 */
#define BACKGAMMON_NUM_CHECKERS 15 /* 每一方的棋子个数 */

/**
 * @brief 格子信息，表示某个位置的棋子颜色和数量。
 */
typedef struct backgammon_grid_t {
    backgammon_color_t color;
    int count;
} backgammon_grid_t;

/**
 * @brief 移动操作构成的树节点。根据每一次投掷的骰子，获得玩家所有可以操作的移动行为，每个叶子
 * 节点及其所有祖先节点构成的路径（按顺序的多次移动操作）代表本轮的一种合法的动作。当 parent 为
 * null 时，忽略其 from 和 to，此时只是一个抽象根节点。
 */
typedef struct backgammon_action_t {
    int from, to;                         /* move: from -> to */
    struct backgammon_action_t *children; /* first child */
    struct backgammon_action_t *sibling;  /* next sibling */
} backgammon_action_t;

/**
 * @brief 创建新游戏
 */
struct backgammon_game_t *backgammon_game_new();

/**
 * @brief 使用指定的格子信息创建新游戏
 */
struct backgammon_game_t *backgammon_game_new_with_board(const backgammon_grid_t *grids,
                                                         const int *positions, size_t size);

/**
 * @brief 重置游戏状态
 */
void backgammon_game_reset(struct backgammon_game_t *game);

/**
 * @brief 拷贝游戏
 */
struct backgammon_game_t *backgammon_game_clone(const struct backgammon_game_t *game);

/**
 * @brief 释放游戏
 *
 * @param game 当前游戏状态
 */
void backgammon_game_free(struct backgammon_game_t *game);

/**
 * @brief 获取指定位置处格子信息，遍历范围 [0, BACKGAMMON_NUM_POSITIONS] 可获取所有格子
 *
 * @param game 当前游戏状态
 * @param pos 指定位置
 * @return backgammon_grid_t
 */
backgammon_grid_t backgammon_game_get_grid(const struct backgammon_game_t *game, int pos);

/**
 * @brief 获取所有合法的动作
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param roll 投掷的每个骰子的点数
 * @return backgammon_action_t* 返回一棵动作树，每个叶子节点对应的路径代表一个合法动作（含多次移动）
 */
backgammon_action_t *backgammon_game_get_actions(const struct backgammon_game_t *game,
                                                 backgammon_color_t color, const int *roll);

/**
 * @brief 释放动作树
 *
 * @param tree 动作树
 */
void backgammon_action_free(backgammon_action_t *tree);

/**
 * @brief 动作树叶节点遍历回调函数原型
 *
 * @param ctx 自定义透传上下文参数，用于在回调函数中使用
 * @param path 叶子节点对应的路径，该路径不包含根节点
 * @param size 路径长度，即所含节点数量
 */
typedef void (*backgammon_action_visitor)(void *ctx, const backgammon_action_t **path, int size);

/**
 * @brief 遍历动作树的每一个路径，每条路径代表一个动作
 *
 * @param tree 动作树
 * @param visitor 动作树叶节点遍历回调函数
 * @param ctx 自定义透传上下文参数，用于在回调函数中使用
 */
void backgammon_action_visit(const backgammon_action_t *tree, backgammon_action_visitor visitor,
                             void *ctx);

/**
 * @brief 判定指定玩家是否可以从 from 位置移动指定步数
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param from 被移动棋子的位置
 * @param steps 步数
 * @return int 返回负数时表示出现错误，对应 backgammon_error_t 枚举，否则表示移动目标位置
 */
int backgammon_game_can_move_from(const struct backgammon_game_t *game, backgammon_color_t color,
                                  int from, int steps);

/**
 * @brief 判定指定玩家是否存在一个位置可以移动指定步数
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param steps 步数
 * @return int 存在时返回 1，否则返回 0
 */
int backgammon_game_can_move(const struct backgammon_game_t *game, backgammon_color_t color,
                             int steps);

/**
 * @brief 移动指定某方玩家的棋子。该函数总假定参数都是合法的，调用该函数之前应该先检查是否可以移动。
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param from 被移动棋子的位置
 * @param to 被移动棋子的目标位置
 * @return int 如果移动操作使对方棋子被攻击则返回 1， 否则返回 0
 */
int backgammon_game_move(struct backgammon_game_t *game, backgammon_color_t color, int from,
                         int to);

/**
 * @brief 查询指定玩家是否可以开始 bear off
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @return int 可以 bear off 时返回 1，否则返回 0
 */
int backgammon_game_can_bear_off(const struct backgammon_game_t *game, backgammon_color_t color);

/**
 * @brief 返回游戏结束后的胜利方颜色，如果游戏没有结束则返回 BACKGAMMON_NOCOLOR
 *
 * @param game 当前游戏状态
 * @return backgammon_color_t
 */
backgammon_color_t backgammon_game_winner(const struct backgammon_game_t *game);

/**
 * @brief 按 TD-Gammon 算法编码棋盘状态
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param vec 输出向量，需要有 198 个元素
 * @return int 返回实际编码特征的个数
 */
int backgammon_game_encode(const struct backgammon_game_t *game, backgammon_color_t color,
                           double *vec);

/**
 * @brief 获取原状态编码的反对称编码
 *
 * @param vec 被反转的向量，需要有 198 个元素
 */
void backgammon_game_reverse_features(double *vec);

/**
 * @brief 按 TD-Gammon 算法编码执行指定动作后的棋盘状态
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param path 构成动作的所有移动操作
 * @param num_moves 移动操作个数
 * @param vec 输出向量，需要有 198 个元素
 * @return int 编码特征个数
 */
int backgammon_game_encode_action(const struct backgammon_game_t *game, backgammon_color_t color,
                                  const backgammon_action_t **path, int num_moves, double *vec);

/**
 * @brief 打印游戏状态
 *
 * @param game 当前游戏状态
 */
void backgammon_game_print(FILE *out, const struct backgammon_game_t *game);

#endif // _BACKGAMMON_H_
