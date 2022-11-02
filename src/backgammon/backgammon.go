package backgammon

// #include "backgammon.h"
// typedef struct backgammon_game_wrapper_t { struct backgammon_game_t* ptr; } backgammon_game_wrapper_t;
import "C"
import "unsafe"

func init() {
	sizeOfDouble := unsafe.Sizeof(C.double(0))
	if sizeOfDouble != 8 {
		panic("sizeof(double) != 8")
	}
}

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
const BLACK_BAR_POS = C.BACKGAMMON_BLACK_BAR_POS           /* 黑方中间条位置 */
const BOARD_MIN_POS = C.BACKGAMMON_BOARD_MIN_POS           /* 棋盘最小位置 */
const BOARD_MAX_POS = C.BACKGAMMON_BOARD_MAX_POS           /* 棋盘最大位置 */
const WHITE_BAR_POS = C.BACKGAMMON_WHITE_BAR_POS           /* 白方中间条位置 */
const WHITE_OFF_POS = C.BACKGAMMON_WHITE_OFF_POS           /* 白方槽位置 */
const BLACK_OFF_POS = C.BACKGAMMON_BLACK_OFF_POS           /* 黑方槽位置 */
const NUM_POSITIONS = C.BACKGAMMON_NUM_POSITIONS           /* 总位置数量 */
const NUM_HOME_POSITIONS = C.BACKGAMMON_NUM_HOME_POSITIONS /* Home 区域位置数量 */

const BACKGAMMON_NUM_DICES = C.BACKGAMMON_NUM_DICES       /* 骰子数量 */
const BACKGAMMON_NUM_CHECKERS = C.BACKGAMMON_NUM_CHECKERS /* 每一方的棋子个数 */

type Color = C.backgammon_color_t

const (
	None  = C.BACKGAMMON_NOCOLOR
	White = C.BACKGAMMON_WHITE
	Black = C.BACKGAMMON_BLACK
)

type Error C.backgammon_error_t

const (
	ErrMoveToOrigin        = Error(C.BACKGAMMON_ERR_MOVE_TO_ORIGIN)
	ErrMoveEmpty           = Error(C.BACKGAMMON_ERR_MOVE_EMPTY)
	ErrMoveOpponentChecker = Error(C.BACKGAMMON_ERR_MOVE_OPPONENT_CHECKER)
	ErrMoveBarNeeded       = Error(C.BACKGAMMON_ERR_MOVE_BAR_NEEDED)
	ErrMoveBlocked         = Error(C.BACKGAMMON_ERR_MOVE_BLOCKED)
	ErrMoveOutOfRange      = Error(C.BACKGAMMON_ERR_MOVE_OUT_OF_RANGE)
	ErrMoveCannotBearOff   = Error(C.BACKGAMMON_ERR_MOVE_CANNOT_BEAR_OFF)
)

func (err Error) Error() string {
	switch err {
	case ErrMoveToOrigin:
		return "move to origin"
	case ErrMoveEmpty:
		return "move empty"
	case ErrMoveOpponentChecker:
		return "move opponent checker"
	case ErrMoveBarNeeded:
		return "move bar needed"
	case ErrMoveBlocked:
		return "move blocked"
	case ErrMoveOutOfRange:
		return "move out of range"
	case ErrMoveCannotBearOff:
		return "move cannot bear off"
	default:
		return "unknown error"
	}
}

type Grid struct {
	Color Color
	Count C.int
}

type Move struct {
	From, Steps, To C.int
}

type Action struct {
	Move     Move
	Children *Action
	Sibling  *Action
}

func newAction(caction *C.backgammon_action_t) *Action {
	if caction == nil {
		return nil
	}
	return &Action{
		Move: Move{
			From:  caction.from,
			Steps: caction.steps,
			To:    caction.to,
		},
		Children: newAction(caction.children),
		Sibling:  newAction(caction.sibling),
	}
}

type Visitor func(path []*Action)

func (action *Action) Visit(visitor Visitor) {
	var path = make([]*Action, 0, 32)
	path = append(path, action)
	for {
		var parent = path[len(path)-1]
		if parent.Children != nil {
			path = append(path, parent.Children)
			continue
		}
		visitor(path[1:])
		for parent.Sibling == nil {
			path = path[:len(path)-1]
			if len(path) == 0 {
				return
			}
			parent = path[len(path)-1]
		}
		path[len(path)-1] = parent.Sibling
	}
}

type Game struct {
	wrapper C.backgammon_game_wrapper_t
}

func NewGame() *Game {
	game := &Game{}
	game.wrapper.ptr = C.backgammon_game_new()
	return game
}

func (game *Game) Close() {
	if game.wrapper.ptr != nil {
		C.backgammon_game_free(game.wrapper.ptr)
		game.wrapper.ptr = nil
	}
}

func (game *Game) Reset(grids map[int]Grid) {
	if grids != nil {
		for i := 0; i < NUM_POSITIONS; i++ {
			var grid, ok = grids[i]
			var cgrid C.backgammon_grid_t
			if ok {
				cgrid.color = grid.Color
				cgrid.count = grid.Count
			} else {
				cgrid.color = None
				cgrid.count = 0
			}
			C.backgammon_game_set_grid(game.wrapper.ptr, C.int(i), cgrid)
		}
	} else {
		C.backgammon_game_reset(game.wrapper.ptr)
	}
}

func (game *Game) Clone() *Game {
	newGame := &Game{}
	newGame.wrapper.ptr = C.backgammon_game_clone(game.wrapper.ptr)
	return newGame
}

func (game *Game) GetGrid(pos int) Grid {
	cgrid := C.backgammon_game_get_grid(game.wrapper.ptr, C.int(pos))
	return Grid{
		Color: cgrid.color,
		Count: cgrid.count,
	}
}

func (game *Game) SetGrid(pos int, grid Grid) {
	var cgrid C.backgammon_grid_t
	cgrid.color = grid.Color
	cgrid.count = grid.Count
	C.backgammon_game_set_grid(game.wrapper.ptr, C.int(pos), cgrid)
}

func (game *Game) GetActions(color Color, roll1, roll2 int) *Action {
	caction := C.backgammon_game_get_actions(game.wrapper.ptr, color, C.int(roll1), C.int(roll2))
	action := newAction(caction)
	C.backgammon_action_free(caction)
	return action
}

func (game *Game) CanMoveFrom(color Color, from int, steps int) bool {
	return C.backgammon_game_can_move_from(game.wrapper.ptr, color, C.int(from), C.int(steps)) == 1
}

func (game *Game) CanMove(color Color, steps int) bool {
	return C.backgammon_game_can_move(game.wrapper.ptr, color, C.int(steps)) == 1
}

func (game *Game) Move(color Color, from, to int) bool {
	return C.backgammon_game_move(game.wrapper.ptr, color, C.int(from), C.int(to)) == 1
}

func (game *Game) CanBearOff(color Color) bool {
	return C.backgammon_game_can_bear_off(game.wrapper.ptr, color) == 1
}

func (game *Game) Winner() Color {
	return C.backgammon_game_winner(game.wrapper.ptr)
}

func (game *Game) Encode(color Color) []float64 {
	var vec = make([]float64, 198)
	C.backgammon_game_encode(game.wrapper.ptr, color, (*C.double)(&vec[0]))
	return vec
}

func (game *Game) EncodeMoves(color Color, moves []Move) []float64 {
	var vec = make([]float64, 198)
	C.backgammon_game_encode_moves(
		game.wrapper.ptr,
		color,
		(*C.backgammon_move_t)(unsafe.Pointer(&moves[0])),
		C.int(len(moves)),
		(*C.double)(&vec[0]),
	)
	return vec
}

func (game *Game) ReverseFeatures(vec []float64) {
	C.backgammon_game_reverse_features((*C.double)(&vec[0]))
}
