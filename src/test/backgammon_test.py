import libgammon

if __name__ == "__main__":
    env = libgammon.Env()
    actions = env.get_legal_actions([1, 3])
    for i in range(len(actions)):
        print(f"action {i:2d}:", end='')
        for j in range(actions[i].num_move()):
            move = actions[i].get_move(j)
            print(f" ({move.pos:2d} -> {move.to:2d})", end='')
        print()
