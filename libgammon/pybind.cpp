#include <exception>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "../third_party/pybind11/include/pybind11/pybind11.h"
#include "../third_party/pybind11/include/pybind11/stl.h"

#include "../src/backgammon/backgammon.h"

static std::string color_to_string(backgammon_color_t color) {
    switch (color) {
    case BACKGAMMON_WHITE:
        return "WHITE";
    case BACKGAMMON_BLACK:
        return "BLACK";
    default:
        return "NONE";
    }
}

static std::string win_kind_to_string(backgammon_win_kind_t kind) {
    switch (kind) {
    case BACKGAMMON_WIN_NORMAL:
        return "NORMAL";
    case BACKGAMMON_WIN_GAMMON:
        return "GAMMON";
    case BACKGAMMON_WIN_BACKGAMMON:
        return "BACKGAMMON";
    default:
        return "NONE";
    }
}

class Stringer {
  public:
    virtual std::ostream &marshal(std::ostream &os) const = 0;

    std::string to_string() const {
        std::stringstream ss;
        marshal(ss);
        return ss.str();
    }
};

class Result : public Stringer {
  public:
    backgammon_color_t winner;
    backgammon_win_kind_t kind;

    std::ostream &marshal(std::ostream &os) const override {
        os << "{winner=" << color_to_string(winner);
        os << ",kind=" << win_kind_to_string(kind) << "}";
        return os;
    }
};

class Grid : public Stringer {
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

    std::ostream &marshal(std::ostream &os) const override {
        if (m_grid.count == 0) {
            os << "{}";
            return os;
        }
        os << "{color=" << color_to_string(m_grid.color);
        os << ",count=" << m_grid.count << "}";
        return os;
    }

  private:
    backgammon_grid_t m_grid;
};

class Move : public Stringer {
  public:
    int pos{0};
    int steps{0};
    int to{0};

    std::ostream &marshal(std::ostream &os) const override {
        return os << "{pos=" << pos << ",steps=" << steps << ",to=" << to << "}";
    }
};

class Action : public Stringer {
  public:
    Action() {}
    Action(const std::vector<Move> &moves) : m_moves(moves) {}
    Action(std::vector<Move> &&moves) : m_moves(moves) {}

    int num_move() const { return m_moves.size(); }
    Move get_move(int i) const { return m_moves[i]; }

    std::ostream &marshal(std::ostream &os) const override {
        os << '[';
        for (size_t i = 0; i < m_moves.size(); i++) {
            m_moves[i].marshal(os);
        }
        return os << ']';
    }

  private:
    std::vector<Move> m_moves;
};

class Game : public Stringer {
  public:
    Game() { m_game = backgammon_game_new(); }
    Game(backgammon_game_t *game) { m_game = game; }
    ~Game() {
        if (m_game != nullptr) {
            backgammon_game_free(m_game);
            m_game = nullptr;
        }
    }

    void reset() { backgammon_game_reset(m_game); }

    Grid grid(int pos) const {
        backgammon_grid_t grid = backgammon_game_get_grid(m_game, pos);
        return Grid(grid.color, grid.count);
    }

    std::vector<Action> get_actions(backgammon_color_t color, const std::vector<int> &roll) const {
        if (roll.size() != 2) {
            throw std::length_error("expected two dices, but got " + std::to_string(roll.size()));
        }
        backgammon_action_t *root = backgammon_game_get_actions(m_game, color, roll[0], roll[1]);
        if (root->children == nullptr) {
            return {};
        }
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
                    moves[i].pos = path[i]->move.from;
                    moves[i].steps = path[i]->move.steps;
                    moves[i].to = path[i]->move.to;
                }
                context->actions.push_back(Action(moves));
                std::cout << "  push action: " << context->actions.back().to_string() << std::endl;
            },
            &context);
        backgammon_action_free(root);
        return std::move(context.actions);
    }

    std::vector<double> encode(backgammon_color_t color) const {
        std::vector<double> vec(BACKGAMMON_NUM_FEATURES);
        backgammon_game_encode(m_game, color, vec.data());
        return vec;
    }

    std::vector<double> encode_action(backgammon_color_t color, const Action &action) const {
        const int n = action.num_move();
        backgammon_move_t moves[n];
        for (int i = 0; i < n; ++i) {
            const auto &m = action.get_move(i);
            backgammon_move_t a;
            a.from = m.to;
            a.steps = m.steps;
            a.to = m.to;
            moves[i] = a;
        }
        std::vector<double> vec(BACKGAMMON_NUM_FEATURES);
        backgammon_game_encode_moves(m_game, color, moves, n, vec.data());
        return vec;
    }

    bool can_move_from(backgammon_color_t color, int pos, int steps) const {
        return backgammon_game_can_move_from(m_game, color, pos, steps);
    }

    bool can_move(backgammon_color_t color, int steps) const {
        return backgammon_game_can_move(m_game, color, steps);
    }

    bool move(backgammon_color_t color, int pos, int to) {
        return backgammon_game_move(m_game, color, pos, to);
    }

    bool can_bear_off(backgammon_color_t color) const {
        return backgammon_game_can_bear_off(m_game, color);
    }

    Result result() const {
        backgammon_result_t x = backgammon_game_result(m_game);
        Result result;
        result.kind = x.kind;
        result.winner = x.winner;
        return result;
    }

    backgammon_color_t get_opponent(backgammon_color_t color) const {
        switch (color) {
        case backgammon_color_t::BACKGAMMON_WHITE:
            return backgammon_color_t::BACKGAMMON_BLACK;
        case backgammon_color_t::BACKGAMMON_BLACK:
            return backgammon_color_t::BACKGAMMON_WHITE;
        default:
            return backgammon_color_t::BACKGAMMON_NOCOLOR;
        }
    }

    std::map<int, Grid> save_state() const {
        std::map<int, Grid> state;
        for (int pos = 0; pos < BACKGAMMON_NUM_POSITIONS; ++pos) {
            state[pos] = grid(pos);
        }
        return state;
    }

    void restore_state(const std::map<int, Grid> &state) {
        backgammon_grid_t empty;
        memset(&empty, 0, sizeof(backgammon_grid_t));
        for (int pos = 0; pos < BACKGAMMON_NUM_POSITIONS; ++pos) {
            backgammon_game_set_grid(m_game, pos, empty);
        }
        for (const auto &kv : state) {
            backgammon_grid_t grid;
            grid.color = kv.second.color();
            grid.count = kv.second.count();
            backgammon_game_set_grid(m_game, kv.first, grid);
        }
    }

    std::ostream &marshal(std::ostream &os) const override {
        char buf[128];
        int n = backgammon_game_to_string(buf, m_game);
        return os << std::string(buf, buf + n);
    }

  private:
    struct backgammon_game_t *m_game{nullptr};
};

PYBIND11_MODULE(_libgammon, mod) {
    namespace py = pybind11;

    enum position_t { _placeholder_ };
    py::enum_<position_t>(mod, "Position")
        .value("BLACK_BAR_POS", (position_t)BACKGAMMON_BLACK_BAR_POS)
        .value("BOARD_MIN_POS", (position_t)BACKGAMMON_BOARD_MIN_POS)
        .value("BOARD_MAX_POS", (position_t)BACKGAMMON_BOARD_MAX_POS)
        .value("WHITE_BAR_POS", (position_t)BACKGAMMON_WHITE_BAR_POS)
        .value("WHITE_OFF_POS", (position_t)BACKGAMMON_WHITE_OFF_POS)
        .value("BLACK_OFF_POS", (position_t)BACKGAMMON_BLACK_OFF_POS)
        .value("NUM_POSITIONS", (position_t)BACKGAMMON_NUM_POSITIONS)
        .value("NUM_HOME_POSITIONS", (position_t)BACKGAMMON_NUM_HOME_POSITIONS)
        .export_values();

    py::enum_<backgammon_color_t>(mod, "Color")
        .value("NOCOLOR", backgammon_color_t::BACKGAMMON_NOCOLOR)
        .value("WHITE", backgammon_color_t::BACKGAMMON_WHITE)
        .value("BLACK", backgammon_color_t::BACKGAMMON_BLACK)
        .export_values();

    py::enum_<backgammon_win_kind_t>(mod, "WinKind")
        .value("NORMAL", backgammon_win_kind_t::BACKGAMMON_WIN_NORMAL)
        .value("GAMMON", backgammon_win_kind_t::BACKGAMMON_WIN_GAMMON)
        .value("BACKGAMMON", backgammon_win_kind_t::BACKGAMMON_WIN_BACKGAMMON)
        .export_values();

    py::class_<Result>(mod, "Result")
        .def_readwrite("kind", &Result::kind)
        .def_readwrite("winner", &Result::winner)
        .def("__repr__", &Result::to_string);

    py::class_<Grid>(mod, "Grid")
        .def(py::init<>())
        .def(py::init<backgammon_color_t, int>())
        .def_property("color", &Grid::color, &Grid::set_color)
        .def_property("count", &Grid::count, &Grid::set_count)
        .def("__repr__", &Grid::to_string);

    py::class_<Move>(mod, "Move")
        .def_readwrite("pos", &Move::pos)
        .def_readwrite("steps", &Move::steps)
        .def_readwrite("to", &Move::to)
        .def("__repr__", &Move::to_string);

    py::class_<Action>(mod, "Action")
        .def(py::init<>())
        .def(py::init<const std::vector<Move> &>())
        .def("num_move", &Action::num_move)
        .def("get_move", &Action::get_move)
        .def("__repr__", &Action::to_string);

    py::class_<Game>(mod, "Game")
        .def(py::init<>())
        .def("reset", &Game::reset)
        .def("grid", &Game::grid)
        .def("get_actions", &Game::get_actions)
        .def("encode", &Game::encode)
        .def("encode_action", &Game::encode_action)
        .def("can_move_from", &Game::can_move_from)
        .def("can_move", &Game::can_move)
        .def("move", &Game::move)
        .def("can_bear_off", &Game::can_bear_off)
        .def("result", &Game::result)
        .def("get_opponent", &Game::get_opponent)
        .def("save_state", &Game::save_state)
        .def("restore_state", &Game::restore_state)
        .def("__repr__", &Game::to_string);
}
