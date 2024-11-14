import os
import json
from typing import List

import yaml


def find_texture_file(texture_path_suffix: str, folders: List[str]):
    for folder in folders:
        tex_path = os.path.join(folder, texture_path_suffix)
        if os.path.exists(tex_path):
            return os.path.relpath(tex_path)

    raise FileNotFoundError(f"Texture file not found: {texture_path_suffix}")


def do_once(yaml_path: str):
    with open(yaml_path, 'r') as f:
        yml_content = yaml.safe_load(f)
    print(yml_content)

    yaml_path = os.path.relpath(yaml_path)
    loc, file_name_ext = os.path.split(yaml_path)
    scene_path = os.path.join(loc, yml_content["scene"])

    tex_lookup_pathes = yml_content["texture_lookup_pathes"]
    for i in range(len(tex_lookup_pathes)):
        tex_lookup_pathes[i] = os.path.join(loc, tex_lookup_pathes[i])

    with open(scene_path, 'r') as f:
        scene = json.load(f)

    textures = set()
    for s in scene["scenes"]:
        for m in s["materials"]:
            for k, v in m.items():
                if k.endswith("map") and v:
                    textures.add(v)

    for t in textures:
        tex_path = find_texture_file(t, tex_lookup_pathes)
        print(f"Texture file found: {tex_path}")


def main():
    do_once(r"C:\Users\woos8\Documents\Dalbaragi Resources\artist\dalbatch.yml")


if __name__ == '__main__':
    main()
