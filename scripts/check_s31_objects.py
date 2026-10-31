#!/usr/bin/env python3
"""Compile changed translation units against an installed S31 Arduino SDK.

This is an object/API check, not an ESP-IDF project build or firmware link.
No SDK downloads or changes to the installed Arduino packages are performed.
"""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gcc", required=True, type=Path)
    parser.add_argument("--sdk", required=True, type=Path,
                        help="esp32s31-libs version directory")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    output = root / "build" / "s31-objects"
    output.mkdir(parents=True, exist_ok=True)
    engine = root / "components" / "prboom"
    platform = root / "components" / "prboom-esp32-compat"
    base = [str(args.gcc.resolve()), "@" + str(args.sdk / "flags/c_flags"),
            "@" + str(args.sdk / "flags/defines"), "-iprefix",
            str(args.sdk / "include") + "/",
            "@" + str(args.sdk / "flags/includes"),
            "-I" + str(args.sdk / "qio_opi/include"),
            "-I" + str(engine / "include"), "-I" + str(engine),
            "-I" + str(platform / "include"), "-std=gnu11", "-DHAVE_CONFIG_H",
            '-DCONFIG_DOOM_WAD_ROOT="/sdcard/doom"', "-Wall", "-Wextra",
            "-Werror=implicit-function-declaration", "-c"]
    sources = [engine / name for name in
               ("pcm_mixer.c", "i_audio.c", "i_sound.c", "w_mmap.c", "dbopl.c")]
    sources.append(platform / "wad_file.c")
    for source in sources:
        subprocess.run(base + [str(source), "-o", str(output / (source.stem + ".o"))],
                       cwd=root, check=True)
    for stereo in (0, 1):
        subprocess.run(base + [f"-DCONFIG_DOOM_AUDIO_STEREO={stereo}",
                              str(platform / "sndhw.c"), "-o",
                              str(output / f"sndhw-stereo-{stereo}.o")],
                       cwd=root, check=True)
    print("S31 object checks passed (including mono and stereo I2S).")


if __name__ == "__main__":
    main()
