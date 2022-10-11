#ifndef _BACKGAMMON_H_
#define _BACKGAMMON_H_

/**
 * @brief 棋子颜色，注意，白色总是代表先手方。
 */
static const int BACKGAMMON_NOCOLOR = 0;
static const int BACKGAMMON_WHITE = 1;
static const int BACKGAMMON_BLACK = 2;

/**
 * @brief 错误码
 */
static const int BACKGAMMON_OK = 0;                        /* 成功 */
static const int BACKGAMMON_ERR_MOVE_EMPTY = 0;            /* 移动空位置 */
static const int BACKGAMMON_ERR_MOVE_OPPONENT_CHECKER = 0; /* 移动敌方棋子 */
static const int BACKGAMMON_ERR_MOVE_BAR_NEEDED = 0;       /* 需要先移动 bar 上的棋子 */
static const int BACKGAMMON_ERR_MOVE_BLOCKED = 0;          /* 目标位置被敌方占领 */

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
static const int BACKGAMMON_BLACK_BAR_POS = 0;  /* 黑方中间条位置 */
static const int BACKGAMMON_WHITE_BAR_POS = 25; /* 白方中间条位置 */
static const int BACKGAMMON_WHITE_OFF_POS = 26; /* 白方槽位置 */
static const int BACKGAMMON_BLACK_OFF_POS = 27; /* 黑方槽位置 */
static const int BACKGAMMON_NUM_POSITIONS = 28; /* 总位置数量 */

static const int BACKGAMMON_NUM_DICES = 2;     /* 骰子数量 */
static const int BACKGAMMON_NUM_CHECKERS = 15; /* 每一方的棋子个数 */

/**
 * @brief 格子信息，表示某个位置的棋子颜色和数量。
 */
typedef struct backgammon_grid_s {
    int color;
    int count;
} backgammon_grid_t;

typedef struct backgammon_move_s {
    int from, to;
} backgammon_move_t;

/**
 * @brief 移动操作构成的树节点。根据每一次投掷的骰子，获得玩家所有可以操作的移动行为，每个叶子
 * 节点及其所有祖先节点构成的路径（按顺序的多次移动操作）代表本轮的一种合法的动作。当 parent 为
 * null 时，忽略其 from 和 to，此时这只是一个抽象根节点。
 */
typedef struct backgammon_action_s {
    backgammon_move_t move;
    struct backgammon_action_s *parent;
    struct backgammon_action_s *sibling;
    struct backgammon_action_s *children;
} backgammon_action_t;

/**
 * @brief 游戏状态
 */
typedef struct backgammon_game_s {
    backgammon_grid_t board[BACKGAMMON_NUM_POSITIONS]; /* 棋盘各个位置的信息 */
} backgammon_game_t;

/**
 * @brief 创建新游戏
 */
backgammon_game_t *backgammon_game_new();

/**
 * @brief 释放游戏
 *
 * @param game 当前游戏状态
 */
void backgammon_game_free(backgammon_game_t *game);

/**
 * @brief 获取游戏指定格子的信息。可通过遍历位置范围 [0, BACKGAMMON_NUM_POSITIONS) 获取
 * 整个棋盘的全部格子信息。
 *
 * @param game 当前游戏状态
 * @param pos 位置编号
 */
backgammon_grid_t backgammon_game_get_grid(const backgammon_game_t *game, int pos);

/**
 * @brief 获取搜有合法的动作
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param roll 投掷的每个骰子的点数
 */
backgammon_action_t *backgammon_game_get_actions(const backgammon_game_t *game, int color,
                                                 const int *roll);

/**
 * @brief 释放移动动作树
 *
 * @param tree 动作树
 */
void backgammon_action_free(backgammon_action_t *tree);

typedef void (*backgammon_action_visitor)(void *ctx, const backgammon_action_t **path, int size);

/**
 * @brief 遍历动作树的每一个路径，每条路径代表一个动作
 *
 * @param tree 动作树
 */
void backgammon_action_visit(const backgammon_action_t *tree, backgammon_action_visitor visitor,
                             void *ctx);

/**
 * @brief 移动指定某方玩家的棋子
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param from 被移动棋子的位置
 * @param to 被移动棋子的目标位置
 * @return int 返回 0 表示成功，否则表示错误原因
 */
int backgammon_game_move(backgammon_game_t *game, int color, int from, int to);

/**
 * @brief 查询指定玩家是否可以开始 bear-off
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @return int 返回 1 表示可以
 */
int backgammon_game_can_bear_off(const backgammon_game_t *game, int color);

/**
 * @brief 返回游戏结束后的胜利方颜色，如果游戏没有结束则返回 BACKGAMMON_NOCOLOR
 *
 * @param game 当前游戏状态
 * @return int BACKGAMMON_NOCOLOR|BACKGAMMON_WHITE|BACKGAMMON_BLACK
 */
int backgammon_game_winner(const backgammon_game_t *game);

/**
 * @brief 按 TD-Gammon 算法编码棋盘状态
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param vec 输出向量，需要有 198 个元素
 * @return int 编码成功返回 1
 */
int backgammon_game_encode(const backgammon_game_t *game, int color, double *vec);

/**
 * @brief 按 TD-Gammon 算法编码执行指定动作后的棋盘状态
 *
 * @param game 当前游戏状态
 * @param color 当前玩家棋子颜色
 * @param moves 构成动作的所有移动操作
 * @param num_moves 移动操作个数
 * @param vec 输出向量，需要有 198 个元素
 * @return int 编码成功返回 1
 */
int backgammon_game_encode_action(const backgammon_game_t *game, int color,
                                  const backgammon_move_t *moves, int num_moves, double *vec);

#endif // _BACKGAMMON_H_
