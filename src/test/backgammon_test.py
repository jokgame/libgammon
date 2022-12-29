import argparse
from numpy import random
import onnxruntime as ort

import libgammon


def main():
    parser = argparse.ArgumentParser(description="test")
    parser.add_argument("--seed", help="random seed", type=int, default=None)
    parser.add_argument("--model1", help="model filename for first player", type=str, default=None)
    parser.add_argument("--model2", help="model filename for second player", type=str, default=None)
    parser.add_argument("-N", help="how many games should play", type=int, default=100)
    parser.add_argument("-v", help="enable verbose logging", type=bool, default=False)

    args = parser.parse_args()

    if args.model1 is None or args.model1 == '':
        raise Exception("--model1 required")

    # load models
    model1 = ort.InferenceSession(args.model1)
    if args.model2 is None or args.model2 == '':
        model2 = model1
    else:
        model2 = ort.InferenceSession(args.model2)

    env = libgammon.Env()

    outputs = model1.run(
        None,
        {"GameState": env.game.encode(libgammon.Color.WHITE)},
    )
    print(outputs[0])

    outputs = model1.run(
        None,
        {"GameState": env.game.encode(libgammon.Color.BLACK)},
    )
    print(outputs[0])
    if args.N < 1:
        return

    verbose = args.v
    white_wins = 0
    for e in range(0, args.N):
        if verbose > 0:
            print(f"play {e+1}th game")
        features, info = env.reset()
        roll = info['roll']
        turn = info['current_player']

        while True:
            player_name = "WHITE" if turn == libgammon.Color.WHITE else "BLACK"
            # roll and get all legal actions
            if roll is None:
                roll = [random.randint(1, 7), random.randint(1, 7)]
            if verbose > 0:
                print(f"== ROUND {env.rounds+1}:{player_name} roll ({roll[0]}{roll[1]})")

            actions = env.get_legal_actions(roll)
            roll = None

            # select best action
            best_action = None
            if actions is None:
                if verbose > 0:
                    print(f"{player_name} no actions")
            else:
                if verbose > 0:
                    print(f"{player_name} select best action from {len(actions)} actions")
                model = model1 if turn == libgammon.Color.WHITE else model2
                best_action = None
                best_win_rate = 0
                for i in range(0, len(actions)):
                    features = env.get_features(turn, actions[i])
                    win_rate = model.run(None, {"GameState": features})[0]
                    if turn == libgammon.Color.BLACK:
                        win_rate = 1 - win_rate
                    if best_action is None or win_rate > best_win_rate:
                        best_action = actions[i]
                        best_win_rate = win_rate
                if verbose > 0:
                    print(f"{player_name} select the best action {best_action}")

            # do the best action
            features, reward, terminated, truncated, info = env.step(best_action)
            if terminated:
                white_wins += 1 if info['winner'] == libgammon.Color.WHITE else 0
                player_name = "WHITE" if info['winner'] == libgammon.Color.WHITE else "BLACK"
                print(f"{e+1} game: {player_name} won after {env.rounds} rounds")
                break

            # next player
            env.current_player = env.get_opponent()
            turn = env.current_player

    print(f"result: white wins {white_wins}/{args.N}")


if __name__ == "__main__":
    main()
