import json
import sys


def format_bytes(size):
    power = 2**10
    n = 0
    power_labels = {0: 'B', 1: 'KB', 2: 'MB', 3: 'GB'}
    while size > power:
        size /= power
        n += 1
    return f"{size:.2f} {power_labels[n]}"


def inspect_json(json_file):
    with open(json_file, 'r') as f:
        json_data = json.load(f)

    anim_pairs = []
    for scene in json_data["scenes"]:
        animations = scene["animations"]
        for anim in animations:
            name = anim["name"]
            data_size = anim["joints data size"]
            anim_pairs.append((data_size, name))
    anim_pairs.sort()

    for size, name in anim_pairs:
        print(f"{name}: {format_bytes(size)}")


def main():
    for x in sys.argv[1:]:
        inspect_json(x)


if __name__ == '__main__':
    main()
