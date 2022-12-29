from typing import Optional
import gym
import numpy as np
import gym.spaces
import time

from _libgammon import Color, Action, Game


class Env(gym.Env):
    """Implements backgammon env
    """

    def __init__(self):
        self.game = Game()
        self.external = False

        dtype = np.float64
        low = np.zeros(198, dtype=dtype)
        high = np.ones(198, dtype=dtype)
        for i in range(3, 97, 4):
            high[i] = 6.0
        high[96] = 7.5
        for i in range(101, 195, 4):
            high[i] = 6.0
        high[194] = 7.5
        self.observation_space = gym.spaces.Box(low=low, high=high, dtype=dtype)
        self.action_space = gym.spaces.Discrete(64)
        self.current_player = Color.NOCOLOR
        self.reset(seed=int(time.time()))

        self.max_rounds = 10000
        self.rounds = 0
        self.white_hits = 0
        self.black_hits = 0

    def get_internal_color(self, external_color):
        if self.external:
            assert external_color == 0 or external_color == 1, print("invalid external color")
            return Color.WHITE if external_color == 0 else Color.BLACK
        return external_color

    def get_external_color(self, internal_color):
        if self.external:
            assert internal_color == Color.WHITE or internal_color == Color.BLACK, print(
                "invalid internal color")
            return 0 if internal_color == Color.WHITE else 1
        return internal_color

    def reset(self, *, seed: Optional[int] = None, options: Optional[dict] = None):
        """Overrides gym.Env reset method

        reset the game and roll for first player
        """
        super().reset(seed=seed, options=options)
        self.rounds = 0
        self.white_hits = 0
        self.black_hits = 0
        self.game.reset()
        roll = self.roll()
        while roll[0] == roll[1]:
            roll = self.roll()
        self.current_player = Color.WHITE if roll[0] > roll[1] else Color.BLACK
        features = self.get_features(self.get_external_color(self.current_player))
        return features, {
            'current_player': self.get_external_color(self.current_player),
            'roll': roll,
        }

    def render(self, mode=None):
        """Overrides gym.Env render method
        """
        pass

    def step(self, action: Action):
        """Overrides gym.Env step method
        """
        if action:
            for i in range(action.num_move()):
                move = action.get_move(i)
                if self.game.move(self.current_player, move.pos, move.to):
                    if self.current_player == Color.WHITE:
                        self.white_hits += 1
                    else:
                        self.black_hits += 1
        features = self.get_features(self.get_opponent())
        reward = 0.0
        terminated = False
        truncated = False
        winner = self.game.result().winner
        if winner != Color.NOCOLOR or self.rounds > self.max_rounds:
            if winner == Color.WHITE:
                reward = 1.0
            terminated = True
        info = {}
        if winner != Color.NOCOLOR:
            info['winner'] = self.get_external_color(winner)
        else:
            info['winner'] = None
        self.rounds += 1
        return features, reward, terminated, truncated, info

    def close(self):
        """Overrides gym.Env close method
        """
        pass

    def roll(self):
        return self.np_random.integers(low=1, high=7, size=2)

    def get_features(self, player, action: Action = None):
        player = self.get_internal_color(player)
        features = self.game.encode(
            player) if action is None else self.game.encode_action(player, action)
        if self.external:
            features = np.array(features)
        return features

    def get_legal_actions(self, roll):
        if roll[0] < 0:
            roll = (-roll[0], -roll[1])
        actions = self.game.get_actions(self.current_player, roll)
        if actions is not None and len(actions) == 0:
            return None
        return actions

    def get_opponent(self, player=None):
        if player is None:
            player = self.current_player
        else:
            player = self.get_internal_color(player)
        assert player == Color.WHITE or player == Color.BLACK, print("invalid player")
        return self.get_external_color(Color.BLACK if player == Color.WHITE else Color.WHITE)

    def next_opponent(self):
        self.current_player = Color.BLACK if self.current_player == Color.WHITE else Color.WHITE
        return self.get_external_color(self.current_player)


class ExternalEnv(Env):
    def __init__(self):
        super(ExternalEnv, self).__init__()
        self.external = True
        self.counter = 0
