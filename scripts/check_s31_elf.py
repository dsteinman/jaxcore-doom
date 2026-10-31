#!/usr/bin/env python3
"""Verify the linked S31 image contains Doom and the intended bring-up backends."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=project / "build/s31")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    elf = build / "jaxcore-doom.elf"
    image = build / "jaxcore-doom.bin"
    data = elf.read_bytes()
    require(data[:6] == b"\x7fELF\x01\x01", "Expected a little-endian 32-bit ELF")
    require(struct.unpack_from("<H", data, 18)[0] == 243, "ELF is not RISC-V")
    cache = (build / "CMakeCache.txt").read_text()
    nm_match = re.search(r"^CMAKE_NM:FILEPATH=(.+)$", cache, re.MULTILINE)
    require(nm_match, "CMake did not record its target nm executable")
    output = subprocess.check_output([nm_match[1], "--defined-only", str(elf)],
                                      text=True)
    symbols = {line.split()[-1] for line in output.splitlines() if line.split()}
    config = (build / "sdkconfig").read_text()
    storage_enabled = "CONFIG_JAXCORE_STORAGE_SDMMC=y" in config
    required = {"doom_main", "D_DoomMain", "P_Ticker", "R_RenderPlayerView",
                "doom_pcm_render", "I_Open", "I_StartSound", "W_CacheLumpNum",
                "sndhw_init", "spi_lcd_send", "gamepadPoll"}
    if storage_enabled:
        required |= {"jaxcore_storage_mount", "sdmmc_card_init", "sdmmc_host_init_slot",
                     "f_mount", "esp_vfs_fat_register", "ff_diskio_register_sdmmc"}
        format_symbols = {"f_mkfs", "f_fdisk", "esp_vfs_fat_sdcard_format",
                          "esp_vfs_fat_sdcard_format_cfg"}
        require(not (format_symbols & symbols), "Unexpected formatting code linked: " +
                ", ".join(sorted(format_symbols & symbols)))
    require(required <= symbols, "Engine symbols missing: " +
            ", ".join(sorted(required - symbols)))
    require('CONFIG_IDF_TARGET="esp32s31"' in config, "Wrong SDK target")
    require("CONFIG_DOOM_WAD_FILESYSTEM=y" in config, "WAD filesystem disabled")
    for option in ("DOOM_DISPLAY_SPI", "DOOM_AUDIO_I2S", "DOOM_LEGACY_INPUT"):
        require(f"CONFIG_{option}=y" not in config, "Unexpected legacy backend: " + option)
    map_text = (build / "jaxcore-doom.map").read_text()
    for source in ("display_null.c.obj", "sndhw_null.c.obj", "gamepad_null.c.obj"):
        require(source in map_text, "Missing bring-up backend: " + source)
    image_bytes = image.read_bytes()
    report = {
        "target": "esp32s31",
        "elf_architecture": "32-bit little-endian RISC-V",
        "required_engine_symbols": sorted(required),
        "application_bytes": len(image_bytes),
        "application_sha256": hashlib.sha256(image_bytes).hexdigest(),
        "hardware_run": False,
        "sdmmc_storage_enabled": storage_enabled,
        "storage_automatic_formatting": False,
        "profile": "headless/software-audio/no-input/filesystem-preflight",
    }
    (build / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
