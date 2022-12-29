#include <algorithm>
#include <array>
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <numeric>
#include <vector>

/**
 * 测试需要使用 AI 导出的 onnx 模型，这里使用 microsoft/onnxruntime 来加载和运行。
 *
 * macOS 上安装 onnxruntime:
 *
 *		brew install onnxruntime
 *
 * linux 上安装 onnxruntime (@see https://onnxruntime.ai/docs/build/inferencing.html):
 *
 *	build (需要 cmake 3.18 及以上版本):
 *		git clone --recursive https://github.com/Microsoft/onnxruntime
 *		cd onnxruntime
 *		./build.sh --config RelWithDebInfo --build_shared_lib --parallel
 *
 *	install:
 *		cd build/Linux/RelWithDebInfo
 *		sudo make install
 *
 *  setup env:
 *      export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
 *
 */
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include "../backgammon/backgammon.h"

static void usage(const char *name) { printf("Usage: %s <onnx1> [onnx2] [N]\n", name); }

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
            fprintf(out, "%s(%d->%d)\n", prefix, parent->move.from, parent->move.to);
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

std::string game_to_string(const backgammon_game_t *game) {
    char buf[128];
    int n = backgammon_game_to_string(buf, game);
    return std::string(buf, buf + n);
}

class TDGammonModel {
  public:
    static constexpr int64_t FEATURES = BACKGAMMON_NUM_FEATURES;
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
    std::shared_ptr<TDGammonModel> model{nullptr};
    backgammon_game_t *game{nullptr};
    backgammon_color_t turn{BACKGAMMON_WHITE};
    std::vector<backgammon_move_t> best_action{};
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
    backgammon_action_t *actions =
        backgammon_game_get_actions(game, BACKGAMMON_WHITE, roll[0], roll[1]);

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
    const char *filename1 = argv[1];
    const char *filename2 = argc > 2 ? argv[2] : filename1;
    const int N = argc > 3 ? std::max(atoi(argv[3]), 1) : 100;

    /* load TD-Gammon onnx */
    std::shared_ptr<TDGammonModel> model1;
    std::shared_ptr<TDGammonModel> model2;
    try {
        model1 = std::make_shared<TDGammonModel>(filename1);
        model2 = std::make_shared<TDGammonModel>(filename2);
    } catch (const Ort::Exception &exception) {
        printf("Error: %s\n", exception.what());
        return 1;
    }

    /* play games */
    int white_wins = 0;
    VisitorContext context;
    backgammon_move_t moves[4096];
    for (int i = 0; i < N; ++i) {
        if (context.game) {
            backgammon_game_free(context.game);
        }
        backgammon_game_t *game = backgammon_game_new();
        if (i == 0) {
            double vec[BACKGAMMON_NUM_FEATURES];
            backgammon_game_encode(game, BACKGAMMON_WHITE, vec);
            fprintf(stderr, "white win rate for white turn: %f\n", model1->run(vec));
            backgammon_game_encode(game, BACKGAMMON_BLACK, vec);
            fprintf(stderr, "white win rate for black turn: %f\n", model1->run(vec));
        }
        context.game = game;
        int roll[2];
        do {
            roll[0] = rand() % 6 + 1;
            roll[1] = rand() % 6 + 1;
        } while (roll[0] == roll[1]);
        backgammon_color_t turn = roll[0] > roll[1] ? BACKGAMMON_WHITE : BACKGAMMON_BLACK;
        int rounds = 0;
        while (backgammon_game_result(game).winner == BACKGAMMON_NOCOLOR) {
            ++rounds;
            context.reset(turn);
            context.model = turn == BACKGAMMON_WHITE ? model1 : model2;
            if (verbose > 0) {
                fprintf(stderr, "---------------- ROUNDS %d ----------------\n", rounds);
            }
            const char *player = turn == BACKGAMMON_WHITE ? "(W)" : "(B)";

            /* roll */
            if (rounds > 1) {
                roll[0] = rand() % 6 + 1;
                roll[1] = rand() % 6 + 1;
            }
            if (verbose > 0) {
                std::cout << "ROUND " << rounds << " " << player << ": roll=(" << roll[0] << roll[1]
                          << ")" << std::endl;
            }

            /* get actions */
            int n = backgammon_game_get_non_equivalent_actions(game, moves, turn, roll[0], roll[1]);

            /* select action */
            int begin_index = 0;
            double features[TDGammonModel::FEATURES];
            for (int i = 0; i < n; i++) {
                if (moves[i].steps == 0) {
                    if (i > begin_index) {
                        const auto num_moves = i - begin_index;
                        assert(backgammon_game_encode_moves(
                            context.game, context.turn, moves + begin_index, num_moves, features));
                        double score = 0;
                        switch (score_policy) {
                        case ScorePolicyType::REVERSE_WHITE: {
                            if (context.turn == BACKGAMMON_WHITE) {
                                score = context.model->run(features);
                            } else {
                                backgammon_game_reverse_features(features);
                                score = 1.0 - context.model->run(features);
                            }
                        } break;

                        case ScorePolicyType::REVERSE_BLACK: {
                            if (context.turn == BACKGAMMON_BLACK) {
                                score = context.model->run(features);
                            } else {
                                backgammon_game_reverse_features(features);
                                score = 1.0 - context.model->run(features);
                            }
                        } break;

                        case ScorePolicyType::REVERSE_AVERAGE: {
                            score = context.model->run(features);
                            backgammon_game_reverse_features(features);
                            score = (1.0 + score - context.model->run(features)) / 2.0;
                        } break;

                        default:
                            score = context.model->run(features);
                            break;
                        }
                        if (context.turn == BACKGAMMON_BLACK) {
                            score = 1.0 - score;
                        }
                        if (score > context.best_action_score) {
                            context.best_action_score = score;
                            context.best_action.resize(num_moves);
                            for (int index = begin_index; index < i; ++index) {
                                context.best_action[index - begin_index] = moves[index];
                            }
                        }
                    }
                    begin_index = i + 1;
                }
            }

            if (verbose > 0) {
                backgammon_game_print(stderr, game);
            }

            if (context.best_action.empty()) {
                if (verbose > 0) {
                    std::cout << "ROUND " << rounds << " " << player << ": no avaiable actions"
                              << std::endl;
                }
            } else {
                /* play action */
                for (const auto &move : context.best_action) {
                    if (verbose > 0) {
                        std::cout << "- MOVE: " << move.from << "-" << move.steps << "->" << move.to
                                  << std::endl;
                    }
                    backgammon_game_move(game, turn, move.from, move.to);
                    if (verbose > 0) {
                        std::cout << "= MOVE: " << move.from << "-" << move.steps << "->" << move.to
                                  << std::endl;
                    }
                }
                if (verbose > 0) {
                    std::cout << "ROUNDS " << rounds << " " << player << ":"
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
        int winner = backgammon_game_result(game).winner;
        printf("game %d: winner=%s, rounds=%d\n", i + 1,
               winner == BACKGAMMON_WHITE ? "WHITE" : "BLACK", rounds);
        if (winner == BACKGAMMON_WHITE) {
            white_wins++;
        }
    }
    printf("result: white wins %d/%d=%.1f%%\n", white_wins, N,
           (double)white_wins * 100 / (double)(N));
    return 0;
}
