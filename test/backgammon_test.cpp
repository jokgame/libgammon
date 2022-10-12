#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

extern "C" {
#include "../backgammon/backgammon.h"
}

#define NUM_FEATURES 198

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

static void print_board(const backgammon_game_t *game) {
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
    fprintf(stderr, "= %s\n", buf);
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
    fprintf(stderr, "= %s\n", buf);
}

static int backgammon_verify_board(const backgammon_game_t *game) {
    int white_count = 0;
    int black_count = 0;
    for (int i = 0; i < BACKGAMMON_NUM_POSITIONS; ++i) {
        if (game->board[i].color == BACKGAMMON_WHITE) {
            white_count += game->board[i].count;
        } else {
            black_count += game->board[i].count;
        }
    }
    return white_count == BACKGAMMON_NUM_CHECKERS && black_count == BACKGAMMON_NUM_CHECKERS;
}

struct TDGammonModel {
    TDGammonModel(const char *filename) : session_{env_, filename, Ort::SessionOptions{nullptr}} {
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        input_tensor_ = Ort::Value::CreateTensor<double>(memory_info, input_.data(), input_.size(),
                                                         input_shape_.data(), input_shape_.size());
        output_tensor_ =
            Ort::Value::CreateTensor<double>(memory_info, output_.data(), output_.size(),
                                             output_shape_.data(), output_shape_.size());
    }

    double run(const double *input) {
        const char *input_names[] = {"Input"};
        const char *output_names[] = {"Ouput"};

        for (int i = 0; i < NUM_FEATURES; ++i) {
            input_[i] = input[i];
        }
        Ort::RunOptions run_options;
        session_.Run(run_options, input_names, &input_tensor_, 1, output_names, &output_tensor_, 1);
        return output_[0];
    }

  private:
    Ort::Env env_;
    Ort::Session session_;

    Ort::Value input_tensor_{nullptr};
    std::array<int64_t, 1> input_shape_{NUM_FEATURES};

    Ort::Value output_tensor_{nullptr};
    std::array<int64_t, 1> output_shape_{1};

    std::array<double, NUM_FEATURES> input_{};
    std::array<double, 1> output_{};
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
    backgammon_game_t *game = backgammon_game_new();
    memset(game, 0, sizeof(backgammon_game_t));

    game->board[24].color = BACKGAMMON_BLACK;
    game->board[24].count = 14;
    game->board[27].color = BACKGAMMON_BLACK;
    game->board[27].count = 1;

    game->board[1].color = BACKGAMMON_WHITE;
    game->board[1].count = 14;
    game->board[26].color = BACKGAMMON_WHITE;
    game->board[26].count = 1;

    int roll[2] = {1, 2};
    backgammon_action_t *actions = backgammon_game_get_actions(game, BACKGAMMON_WHITE, roll);
    print_actions(stderr, actions);

    backgammon_game_free(game);
    return 0;
}

int main(int argc, char **argv) {
    srand(time(NULL));

    const int verbose = 0;
    if (argc == 1) {
        usage(argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    const int N = argc == 3 ? max(atoi(argv[2]), 1) : 100;

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
                    double features[NUM_FEATURES];
                    backgammon_move_t moves[size];
                    for (int i = 0; i < size; ++i) {
                        moves[i] = path[i]->move;
                    }
                    if (verbose > 1) {
                        std::cout << "candidate action=";
                        for (const auto &move : moves) {
                            std::cout << "(" << move.from << "->" << move.to << ")";
                        }
                        std::cout << std::endl;
                    }
                    assert(backgammon_game_encode_action(context->game, context->turn, moves, size,
                                                         features));
                    double score = context->model->run(features);
                    if (context->turn == BACKGAMMON_BLACK) {
                        score = 1.0 - score;
                    }
                    if (score > context->best_action_score) {
                        context->best_action_score = score;
                        context->best_action = std::vector<backgammon_move_t>(moves, moves + size);
                    }
                },
                &context);
            backgammon_action_free(actions);

            if (verbose > 0) {
                print_board(game);
            }

            if (context.best_action.empty()) {
                if (verbose > 0) {
                    std::cout << "step " << steps << " " << player << ": no avaiable actions"
                              << std::endl;
                }
            } else {
                /* play action */
                for (const auto &move : context.best_action) {
                    assert(backgammon_game_move(game, turn, move.from, move.to) == BACKGAMMON_OK);
                    if (!backgammon_verify_board(game)) {
                        fprintf(stderr, "invalid board after %d move from %d to %d\n", turn,
                                move.from, move.to);
                        print_board(game);
                        assert(backgammon_verify_board(game));
                    }
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