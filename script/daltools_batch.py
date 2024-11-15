import os
import json
import shutil
import sys
from typing import Iterable, Optional

import yaml


def mkdir_tree(path: str):
    os.makedirs(path, exist_ok=True)


def make_neighbour_path(host_file_path: str, neighbour_file_path: str):
    host_dir = os.path.split(host_file_path)[0]
    return os.path.relpath(os.path.join(host_dir, neighbour_file_path))


def find_texture_file(texture_path_suffix: str, folders: Iterable[str]):
    for folder in folders:
        tex_path = os.path.join(folder, texture_path_suffix)
        if os.path.exists(tex_path):
            return os.path.relpath(tex_path)

    raise FileNotFoundError(f"Texture file not found: {texture_path_suffix}")


class YamlLoader:
    def __init__(self, yaml_path: str):
        with open(yaml_path, 'r') as f:
            self.__yml_content = yaml.safe_load(f)
        print(json.dumps(self.__yml_content, indent=4))

        self.__yaml_path = os.path.relpath(yaml_path)

        self.__tex_lookup_paths = self.__yml_content["texture_lookup_paths"]
        for i in range(len(self.__tex_lookup_paths)):
            self.__tex_lookup_paths[i] = os.path.join(self.loc, self.__tex_lookup_paths[i])

    @property
    def loc(self):
        return os.path.split(self.__yaml_path)[0]

    @property
    def src_path(self):
        return os.path.join(self.loc, self.__yml_content["src"])

    @property
    def src_loc(self):
        return os.path.split(self.src_path)[0]

    @property
    def tex_lookup_paths(self):
        return iter(self.__tex_lookup_paths)

    def gen_ktx_conversions(self):
        for x in self.__yml_content["ktx_conversion"]:
            yield x


class KtxParameters:
    def __init__(self):
        self.__src_path: str = ""
        self.__dst_path: Optional[str] = None
        self.__channels: Optional[str] = None
        self.__srgb: Optional[bool] = None

    @property
    def src_path(self):
        return self.__src_path

    @src_path.setter
    def src_path(self, value: str):
        self.__src_path = str(value)

    @property
    def dst_path(self):
        if self.__dst_path is None:
            raise ValueError("Destination path not set")
        return self.__dst_path

    @property
    def channels(self):
        return self.__channels

    @channels.setter
    def channels(self, value):
        value = str(value).lower()
        if value not in ("r", "rg", "rgb", "rgba"):
            raise ValueError(f"Invalid channels: {value})")
        self.__channels = value

    @property
    def srgb(self):
        return self.__srgb

    @srgb.setter
    def srgb(self, value):
        self.__srgb = bool(value)

    def finalize(self, ktx_dir: str, yaml_loc: str):
        if self.__channels is None:
            raise ValueError("Channel value not set")
        if self.__srgb is None:
            raise ValueError("sRGB value not set")

        if not os.path.isfile(self.__src_path):
            raise FileNotFoundError(f"Source file not found: {self.__src_path}")

        src_rel_path = os.path.relpath(self.src_path, yaml_loc)
        self.__dst_path = os.path.join(ktx_dir, os.path.splitext(src_rel_path)[0] + ".ktx")
        return

    def make_ktx_cmd(self):
        commands = [
            "ktx",
            "create",
            "--encode uastc",
            "--generate-mipmap",
        ]

        if self.channels == "rgba":
            format_prefix = "R8G8B8A8"
        elif self.channels == "rgb":
            format_prefix = "R8G8B8"
        elif self.channels == "rg":
            format_prefix = "R8G8"
        elif self.channels == "r":
            format_prefix = "R8"
        else:
            raise ValueError(f"Invalid channel count: {commands}")

        if self.srgb:
            commands.append(f"--format {format_prefix}_SRGB")
            commands.append("--assign-oetf srgb")
            commands.append("--convert-oetf srgb")
        else:
            commands.append(f"--format {format_prefix}_UNORM")
            commands.append("--assign-oetf linear")
            commands.append("--convert-oetf linear")

        commands.append(f'"{self.src_path}"')
        commands.append(f'"{self.dst_path}"')

        return " ".join(commands)


def __do_ktx_conversion(ktx_params: KtxParameters, ktx_dir: str):

    mkdir_tree(ktx_dir)
    cmd = ktx_params.make_ktx_cmd()
    if 0 != os.system(cmd):
        raise RuntimeError("Failed to convert to KTX")
    else:
        print("Success:", cmd)


def do_once(yaml_path: str):
    yaml_data = YamlLoader(yaml_path)
    out_dir = os.path.join(yaml_data.loc, "out")

    ktx_map = {}
    ktx_path = os.path.join(out_dir, "ktx")
    for x in yaml_data.gen_ktx_conversions():
        ktx_params = KtxParameters()
        ktx_params.src_path = find_texture_file(x["src"], yaml_data.tex_lookup_paths)
        ktx_params.channels = x["channels"]
        ktx_params.srgb = x["srgb"]
        ktx_params.finalize(ktx_path, yaml_data.loc)
        __do_ktx_conversion(ktx_params, ktx_path)
        ktx_map[ktx_params.src_path] = ktx_params.dst_path

    with open(yaml_data.src_path, 'r') as f:
        json_data = json.load(f)

    textures = set()
    for s in json_data["scenes"]:
        for m in s["materials"]:
            for k, v in m.items():
                if k.endswith("map") and v:
                    textures.add(v)

    intermediate_dir = os.path.join(out_dir, "intermediate")
    mkdir_tree(intermediate_dir)

    dal_tex_map = {}
    for t in textures:
        tex_path = make_neighbour_path(yaml_data.src_path, t)
        if tex_path in ktx_map.keys():
            ktx_tex_path = ktx_map[tex_path]
            tex_path_in_dal = os.path.relpath(ktx_tex_path, ktx_path)
        else:
            tex_path_in_dal = os.path.relpath(tex_path, yaml_data.src_loc)

        if t != tex_path_in_dal:
            dal_tex_map[t] = tex_path_in_dal

        tex_path_in_intermediate = os.path.join(intermediate_dir, tex_path_in_dal)
        shutil.copy(tex_path, tex_path_in_intermediate)

    for s in json_data["scenes"]:
        for m in s["materials"]:
            for key in m.keys():
                value = m[key]
                if value in dal_tex_map.keys():
                    m[key] = dal_tex_map[value]

    new_json_path = os.path.join(intermediate_dir, os.path.split(yaml_data.src_path)[1])
    with open(new_json_path, 'w') as f:
        json.dump(json_data, f, indent=4)

    bin_path = os.path.splitext(yaml_data.src_path)[0] + ".bin"
    if os.path.exists(bin_path):
        shutil.copy(bin_path, os.path.join(intermediate_dir, os.path.split(bin_path)[1]))

    daltools_compile_commands = [
        "daltools", "compile", "-c", "0", f'"{new_json_path}"',
    ]
    cmd = " ".join(daltools_compile_commands)
    if 0 != os.system(cmd):
        raise RuntimeError("Failed to compile")
    print("Success:", cmd)

    daltools_bundle_commands = [
        "daltools",
        "bundle",
        "-q",
        "6",
        "-o",
        f'"{os.path.abspath(os.path.join(intermediate_dir, "artist.dun"))}"',
        f'"{os.path.abspath(os.path.join(intermediate_dir, "*.dmd"))}"',
        f'"{os.path.abspath(os.path.join(intermediate_dir, "*.png"))}"',
        f'"{os.path.abspath(os.path.join(intermediate_dir, "*.tga"))}"',
        f'"{os.path.abspath(os.path.join(intermediate_dir, "*.jpg"))}"',
        f'"{os.path.abspath(os.path.join(intermediate_dir, "*.ktx"))}"',
    ]
    cmd = " ".join(daltools_bundle_commands)
    if 0 != os.system(cmd):
        raise RuntimeError("Failed to bundle")
    print("Success:", cmd)


def main():
    for x in sys.argv[1:]:
        do_once(x)


if __name__ == '__main__':
    main()
