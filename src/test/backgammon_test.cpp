#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <numeric>
#include <vector>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

extern "C" {
#include "../backgammon/backgammon.h"
}

static void usage(const char *name) { printf("Usage: %s <onnx model filename> [N]\n", name); }

static int max(int a, int b) { return a > b ? a : b; }

static void print_actions(FILE *out, backgammon_action_t *tree) {
    const backgammon_action_t *path[32];
    int size = 0;
    path[size++] = tree;
    while (size > 0) {
        const backgammon_action_t *parent = path[size - 1];
        if (size == 1) {
            fprintf(out, ".\n");
        } else {
            char prefix[size * 4 - 3];
            memset(prefix, ' ', size * 4 - 4);
            prefix[size * 4 - 4] = 0;
            fprintf(out, "%s(%d->%d)\n", prefix, parent->from, parent->to);
        }
        if (parent->children) {
            path[size++] = parent->children;
            continue;
        }
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

class TDGammonModel {
  public:
    static constexpr int64_t FEATURES = 198;
    static constexpr int64_t OUTPUTS = 1;

  public:
    TDGammonModel(const char *filename) : session_{env_, filename, Ort::SessionOptions{nullptr}} {
        auto m = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        input_tensor_ = Ort::Value::CreateTensor<double>(m, input_, FEATURES, &FEATURES, 1);
        output_tensor_ = Ort::Value::CreateTensor<double>(m, output_, OUTPUTS, &OUTPUTS, 1);
    }

    double run(const double *input) {
        const char *input_names[] = {"GameState"};
        const char *output_names[] = {"WhiteWinRate"};

        memcpy(input_, input, sizeof(double) * FEATURES);
        Ort::RunOptions run_options;
        session_.Run(run_options, input_names, &input_tensor_, 1, output_names, &output_tensor_, 1);
        return output_[0];
    }

  private:
    Ort::Env env_;
    Ort::Session session_;

    Ort::Value input_tensor_{nullptr};
    Ort::Value output_tensor_{nullptr};

    double input_[FEATURES];
    double output_[1];
};

struct VisitorContext {
    struct Move {
        int from, to;
    };
    std::shared_ptr<TDGammonModel> model{nullptr};
    backgammon_game_t *game{nullptr};
    backgammon_color_t turn{BACKGAMMON_WHITE};
    std::vector<Move> best_action{};
    double best_action_score{-1};

    void reset(backgammon_color_t turn_) {
        turn = turn_;
        best_action.clear();
        best_action_score = -1;
    }
};

int test() {
    size_t size = 0;
    const int roll[2] = {2, 3};
    backgammon_grid_t grids[BACKGAMMON_NUM_POSITIONS];
    int positions[BACKGAMMON_NUM_POSITIONS];
    grids[size].color = BACKGAMMON_WHITE;
    grids[size].count = 1;
    positions[size++] = BACKGAMMON_BOARD_MIN_POS;
    grids[size].color = BACKGAMMON_WHITE;
    grids[size].count = 1;
    positions[size++] = BACKGAMMON_BOARD_MIN_POS + 2;

    backgammon_game_t *game = backgammon_game_new_with_board(grids, positions, size);
    backgammon_action_t *actions = backgammon_game_get_actions(game, BACKGAMMON_WHITE, roll);

    print_actions(stderr, actions);

    free(game);
    return 0;
}

int main(int argc, char **argv) {
    srand(time(NULL));

    const int verbose = 0;
    const bool enable_rev_score = true;

    /**
     * TDGammon 算法训练的模型存在不一致问题：给定一个棋盘状态 s，对应的反对称状态记为 s',
     * 那么该模型可能无法满足恒等式:
     *
     *      f(s) + f(s') = 1
     *
     * 所谓反对称状态是指将每个棋子移动到对方对应的位置处之后获得的状态。一个合理胜率评估函数应该满足上述
     * 恒等式，但是网络模型却无法保证。如果不进行处理（朴素策略），会导致白棋 AI 和黑棋 AI 不对等。
     */
    enum ScorePolicyType {
        NAIVE = 0,           /* 朴素策略 */
        REVERSE_WHITE = 1,   /* 对白色回合状态反转 */
        REVERSE_BLACK = 2,   /* 对黑色回合状态反转 */
        REVERSE_AVERAGE = 3, /* 反转之后求平均值: g(s) = (f(s) + 1 - f(s')) / 2 */
    };
    const ScorePolicyType score_policy = NAIVE;

    if (argc == 1) {
        usage(argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    const int N = argc == 3 ? max(atoi(argv[2]), 1) : 10000;

    /* load TD-Gammon onnx */
    std::shared_ptr<TDGammonModel> model;
    try {
        model = std::make_shared<TDGammonModel>(filename);
    } catch (const Ort::Exception &exception) {
        printf("Error: %s\n", exception.what());
        return 1;
    }

    /* play games */
    int white_wins = 0;
    VisitorContext context;
    context.model = model;
    for (int i = 0; i < N; ++i) {
        if (context.game) {
            backgammon_game_free(context.game);
        }
        backgammon_game_t *game = backgammon_game_new();
        context.game = game;
        backgammon_color_t turn = rand() % 2 == 0 ? BACKGAMMON_WHITE : BACKGAMMON_BLACK;
        int roll[2];
        int steps = 0;
        while (backgammon_game_winner(game) == BACKGAMMON_NOCOLOR) {
            ++steps;
            context.reset(turn);
            if (verbose > 0) {
                fprintf(stderr, "---------------- STEPS %d ----------------\n", steps);
            }
            const char *player = turn == BACKGAMMON_WHITE ? "(W)" : "(B)";

            /* roll */
            roll[0] = rand() % 6 + 1;
            roll[1] = rand() % 6 + 1;
            if (verbose > 0) {
                std::cout << "step " << steps << " " << player << ": roll=(" << roll[0] << roll[1]
                          << ")" << std::endl;
            }

            /* get actions */
            backgammon_action_t *actions = backgammon_game_get_actions(game, turn, roll);
            if (verbose > 1) {
                fprintf(stderr, "---------------- ACTION ----------------\n");
                print_actions(stderr, actions);
                fprintf(stderr, "----------------------------------------\n");
            }

            /* select action */
            backgammon_action_visit(
                actions,
                [](void *ctx, const backgammon_action_t **path, int size) {
                    VisitorContext *context = (VisitorContext *)ctx;
                    double features[TDGammonModel::FEATURES];
                    assert(backgammon_game_encode_action(context->game, context->turn, path, size,
                                                         features) == TDGammonModel::FEATURES);
                    double score = 0;
                    switch (score_policy) {
                    case ScorePolicyType::REVERSE_WHITE: {
                        if (context->turn == BACKGAMMON_WHITE) {
                            score = context->model->run(features);
                        } else {
                            backgammon_game_reverse_features(features);
                            score = 1.0 - context->model->run(features);
                        }
                    } break;

                    case ScorePolicyType::REVERSE_BLACK: {
                        if (context->turn == BACKGAMMON_BLACK) {
                            score = context->model->run(features);
                        } else {
                            backgammon_game_reverse_features(features);
                            score = 1.0 - context->model->run(features);
                        }
                    } break;

                    case ScorePolicyType::REVERSE_AVERAGE: {
                        score = context->model->run(features);
                        backgammon_game_reverse_features(features);
                        score = (1.0 + score - context->model->run(features)) / 2.0;
                    } break;

                    default:
                        score = context->model->run(features);
                        break;
                    }
                    if (context->turn == BACKGAMMON_BLACK) {
                        score = 1.0 - score;
                    }
                    if (score > context->best_action_score) {
                        context->best_action_score = score;
                        context->best_action.resize(size);
                        for (int i = 0; i < size; ++i) {
                            context->best_action[i].from = path[i]->from;
                            context->best_action[i].to = path[i]->to;
                        }
                    }
                },
                &context);
            backgammon_action_free(actions);

            if (verbose > 0) {
                backgammon_game_print(stderr, game);
            }

            if (context.best_action.empty()) {
                if (verbose > 0) {
                    std::cout << "step " << steps << " " << player << ": no avaiable actions"
                              << std::endl;
                }
            } else {
                /* play action */
                for (const auto &move : context.best_action) {
                    backgammon_game_move(game, turn, move.from, move.to);
                }
                if (verbose > 0) {
                    std::cout << "step " << steps << " " << player << ":"
                              << " action=";
                    for (const auto &move : context.best_action) {
                        std::cout << "(" << move.from << "->" << move.to << ")";
                    }
                    std::cout << std::endl;
                }
            }

            /* next turn */
            turn = turn == BACKGAMMON_WHITE ? BACKGAMMON_BLACK : BACKGAMMON_WHITE;
        }

        /* print game result */
        int winner = backgammon_game_winner(game);
        printf("game %d: winner=%s, steps=%d\n", i + 1,
               winner == BACKGAMMON_WHITE ? "WHITE" : "BLACK", steps);
        if (winner == BACKGAMMON_WHITE) {
            white_wins++;
        }
    }
    printf("result: white wins %d/%d=%.1f%%\n", white_wins, N,
           (double)white_wins * 100 / (double)(N));
    return 0;
}