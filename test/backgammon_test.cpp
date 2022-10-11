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

void usage(const char *name) { printf("Usage: %s <onnx model filename> [N]\n", name); }

int max(int a, int b) { return a > b ? a : b; }

static const int NUM_FEATURES = 198;

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
    int turn{BACKGAMMON_WHITE};
    std::vector<backgammon_move_t> best_action{};
    double best_action_score{-1};

    void reset(int turn_) {
        turn = turn_;
        best_action.clear();
        best_action_score = -1;
    }
};

int main(int argc, char **argv) {
    srand(time(NULL));

    const int verbose = 1;
    if (argc == 1) {
        usage(argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    const int N = argc == 2 ? max(atoi(argv[1]), 1) : 100;

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
        int turn = BACKGAMMON_WHITE;
        int roll[2];
        int steps = 0;
        while (backgammon_game_winner(game) == BACKGAMMON_NOCOLOR) {
            ++steps;
            context.reset(turn);

            const char *player = turn == BACKGAMMON_WHITE ? "(W)" : "(B)";
            /* roll */
            roll[0] = rand() % 6 + 1;
            roll[1] = rand() % 6 + 1;
            std::cout << "step " << steps << " " << player << ": roll=(" << roll[0] << roll[1]
                      << ")" << std::endl;

            /* get actions */
            backgammon_action_t *actions = backgammon_game_get_actions(game, turn, roll);

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
                    if (score > context->best_action_score) {
                        context->best_action_score = score;
                        context->best_action = std::vector<backgammon_move_t>(moves, moves + size);
                    }
                },
                &context);
            backgammon_action_free(actions);

            /* play action */
            assert(!context.best_action.empty());
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