from .env import Env, ExternalEnv
from _libgammon import Color, Grid, Move, Action, Game

from gym.envs.registration import register

register("Backgammon-v0", entry_point="libgammon:Env")
register("BackgammonExt-v0", entry_point="libgammon:ExternalEnv")
