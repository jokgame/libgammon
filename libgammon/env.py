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

        dtype = np.float64
        low = np.zeros((198, 1), dtype=dtype)
        high = np.ones((198, 1), dtype=dtype)
        for i in range(3, 97, 4):
            high[i] = 6.0
        high[96] = 7.5
        for i in range(101, 195, 4):
            high[i] = 6.0
        high[194] = 7.5
        self.observation_space = gym.spaces.Box(low=low, high=high, dtype=dtype)
        self.action_space = gym.spaces.Discrete(64)
        self.max_steps = 10000
        self.current_player = Color.NOCOLOR
        self.steps = 0
        self.reset(seed=int(time.time()))

    def reset(self, *, seed: Optional[int] = None, options: Optional[dict] = None):
        """Overrides gym.Env reset method

        reset the game and roll for first player
        """
        super().reset(seed=seed, options=options)
        self.steps = 0
        self.game.reset()
        roll = self.roll()
        while roll[0] == roll[1]:
            roll = self.roll()
        self.current_player = Color.WHITE if roll[0] > roll[1] else Color.BLACK
        return self.get_features(self.current_player), {
            'current_player': self.current_player,
            'roll': roll,
        }

    def render(self, mode=None):
        """Overrides gym.Env render method
        """
        pass

    def step(self, action: Action):
        """Overrides gym.Env step method
        """
        for i in range(action.num_move()):
            self.game.move(self.current_player, action.get_move(i))
        features = self.get_features(self.get_opponent())
        reward = 0.0
        terminated = False
        truncated = False
        winner = self.game.winner()
        if winner != Color.NOCOLOR or self.steps > self.max_steps:
            if winner == Color.WHITE:
                reward = 1.0
            terminated = True
        return features, reward, terminated, truncated, {'winner': self.game.winner()}

    def close(self):
        """Overrides gym.Env close method
        """
        pass

    def roll(self):
        return self.np_random.integers(low=1, high=6, size=2)

    def get_features(self, player, action: Action = None):
        return self.game.encode(player) if action is None else self.game.encode_action(player, action)

    def get_legal_actions(self, roll):
        return self.game.get_actions(self.current_player, roll)

    def get_opponent(self, player=None):
        if player is None:
            player = self.current_player
        assert player == Color.WHITE or player == Color.BLACK, print("invalid player")
        return Color.BLACK if self.current_player == Color.WHITE else Color.WHITE
