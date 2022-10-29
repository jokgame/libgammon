from .env import Env
from _libgammon import Color, Grid, Move, Action, Game

from gym.envs.registration import register

register("Backgammon-v0", entry_point="libgammon:Env")
