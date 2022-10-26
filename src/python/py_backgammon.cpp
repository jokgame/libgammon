#include <map>
#include <vector>

#include "../../third_party/pybind11/include/pybind11/pybind11.h"

#include "../backgammon/backgammon.h"

class Grid {
  public:
    Grid() {
        m_grid.color = backgammon_color_t::BACKGAMMON_NOCOLOR;
        m_grid.count = 0;
    }
    Grid(backgammon_color_t color, int count) {
        m_grid.color = color;
        m_grid.count = count;
    }

    backgammon_color_t color() const { return m_grid.color; }
    int count() const { return m_grid.count; }
    void set_color(backgammon_color_t color) { m_grid.color = color; }
    void set_count(int count) { m_grid.count = count; }

  private:
    backgammon_grid_t m_grid;
};

struct Move {
    int from{0};
    int steps{0};
    int to{0};
};

class Action {
  public:
    Action() {}
    Action(const std::vector<Move> &moves) : m_moves(moves) {}
    Action(std::vector<Move> &&moves) : m_moves(moves) {}

    int num_move() const { return m_moves.size(); }
    Move get_move(int i) const { return m_moves[i]; }

  private:
    std::vector<Move> m_moves;
};

class Game {
  public:
    Game() { m_game = backgammon_game_new(); }
    ~Game() {
        if (m_game != nullptr) {
            backgammon_game_free(m_game);
            m_game = nullptr;
        }
    }

    void reset() { backgammon_game_reset(m_game); }

    Grid grid(int pos) {
        backgammon_grid_t grid = backgammon_game_get_grid(m_game, pos);
        return Grid(grid.color, grid.count);
    }

    std::vector<Action> get_actions(backgammon_color_t color, const std::vector<int> &roll) const {
        std::vector<Action> actions;
        backgammon_action_t *root = backgammon_game_get_actions(m_game, color, roll.data());

        struct VisitorContext {
            backgammon_game_t *game{nullptr};
            backgammon_color_t turn;
            std::vector<Action> actions;
        } context;
        context.game = m_game;
        context.turn = color;
        backgammon_action_visit(
            root,
            [](void *ctx, const backgammon_action_t **path, int size) {
                VisitorContext *context = (VisitorContext *)ctx;
                std::vector<Move> moves(size);
                for (int i = 0; i < size; ++i) {
                    moves[i].from = path[i]->from;
                    moves[i].steps = path[i]->steps;
                    moves[i].to = path[i]->to;
                }
                context->actions.push_back(Action(std::move(moves)));
            },
            &context);
        backgammon_action_free(root);
        actions = std::move(context.actions);
        return actions;
    }

    std::vector<double> encode_action(backgammon_color_t color, const Action &action) const {
        const int n = action.num_move();
        std::vector<double> vec(198);
        const backgammon_action_t *path[n];
        for (int i = 0; i < n; ++i) {
            const auto &m = action.get_move(i);
            backgammon_action_t *a = (backgammon_action_t *)malloc(sizeof(backgammon_action_t));
            a->from = m.from;
            a->steps = m.steps;
            a->to = m.to;
            path[i] = a;
        }
        backgammon_game_encode_action(m_game, color, path, n, vec.data());
        for (int i = 0; i < n; ++i) {
            free(const_cast<backgammon_action_t *>(path[i]));
        }
        return vec;
    }

    bool can_move_from(backgammon_color_t color, int from, int steps) const {
        return backgammon_game_can_move_from(m_game, color, from, steps);
    }

    bool can_move(backgammon_color_t color, int steps) const {
        return backgammon_game_can_move(m_game, color, steps);
    }

    bool move(backgammon_color_t color, int from, int to) {
        return backgammon_game_move(m_game, color, from, to);
    }

    bool can_bear_off(backgammon_color_t color) {
        return backgammon_game_can_bear_off(m_game, color);
    }

    backgammon_color_t winner() { return backgammon_game_winner(m_game); }

  private:
    struct backgammon_game_t *m_game{nullptr};
};

PYBIND11_MODULE(backgammon, mod) {
    namespace py = pybind11;

    py::enum_<backgammon_color_t>(mod, "Color")
        .value("NoColor", backgammon_color_t::BACKGAMMON_NOCOLOR)
        .value("White", backgammon_color_t::BACKGAMMON_WHITE)
        .value("Black", backgammon_color_t::BACKGAMMON_BLACK)
        .export_values();

    py::class_<Grid>(mod, "Grid")
        .def(py::init<>())
        .def(py::init<backgammon_color_t, int>())
        .def_property("color", &Grid::color, &Grid::set_color)
        .def_property("count", &Grid::count, &Grid::set_count);

    py::class_<Move>(mod, "Move")
        .def_readwrite("from", &Move::from)
        .def_readwrite("steps", &Move::steps)
        .def_readwrite("to", &Move::to);

    py::class_<Action>(mod, "Action")
        .def(py::init<>())
        .def(py::init<const std::vector<Move> &>())
        .def("num_move", &Action::num_move)
        .def("get_move", &Action::get_move);

    py::class_<Game>(mod, "Game")
        .def(py::init<>())
        .def("reset", &Game::reset)
        .def("grid", &Game::grid)
        .def("get_actions", &Game::get_actions)
        .def("encode_action", &Game::encode_action)
        .def("can_move_from", &Game::can_move_from)
        .def("can_move", &Game::can_move)
        .def("move", &Game::move)
        .def("can_bear_off", &Game::can_bear_off)
        .def("winner", &Game::winner);
}
