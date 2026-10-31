# Complete ESP32-S31 firmware build

## Status

The complete application compiles and links for ESP32-S31. The ELF contains the
game loop, renderer, gameplay logic, stereo mixer, WAD cache and filesystem
adapter. This is a build/diagnostics milestone; no hardware has been run.

The current S31 profile deliberately selects:

| Area | Current behavior |
| --- | --- |
| Engine rendering | 320x200 indexed pixels, framebuffer in PSRAM |
| Display | Headless sink; no ST7789 GPIO activity |
| Planned panel | 284x240 landscape, RGB565; full framebuffer 136,320 bytes |
| Planned presentation | 284x213 aspect-corrected image with 27 letterbox rows |
| Audio | Existing 22050 Hz stereo callback into a tick-paced software sink |
| Controls | No input backend; upstream S3 mapping is excluded |
| WAD storage | SDMMC/FAT mount at `/sdcard`; read-only WAD access under `/sdcard/doom` |
| Startup | Memory diagnostics, staged storage initialization, required-file preflight |

Startup requests SDMMC Slot 0 at 3.3 V, four data lines and a 10 MHz ceiling,
then mounts an existing FAT volume. It does not enter Doom unless the mount
succeeds and both `DOOM1.WAD` and `prboom.wad` can be opened. Missing/unformatted
media and absent WAD files produce diagnostics and stop engine startup.
No formatting or file creation is performed. These paths are not device-tested.
See [STORAGE.md](STORAGE.md) for storage configuration and diagnostics.
The full engine remains linked and is checked by `scripts/check_s31_elf.py`.

## Pinned environment

- SDK: official `espressif/esp-idf`, commit
  `9a97f6c54ec638111ce55cd36581b3c192f15207` from `release/v6.1`.
- Compiler: Espressif `riscv32-esp-elf`, `esp-15.2.0_20251204`.
- Initial host validation: macOS arm64, Python 3.13.7, Ninja 1.13.2.
- Engine/platform C sources use GNU C11 because their legacy boolean definitions
  predate C23. Other SDK components retain their own compiler settings.

The SDK/tool pin is in `config/idf-revision.json`. The source tree, compiler,
Python environment, and caches live under the workspace's sibling `toolchains`
directory, outside this repository. The scripts never change shell startup files.

## Setup and build

From the repository root, using Python 3.10 or later (3.13 was verified):

```sh
python3.13 scripts/setup_idf.py
scripts/idf.sh build
python3 scripts/check_s31_elf.py
scripts/idf.sh size
```

Setup downloads the pinned SDK and recursive submodules, installs official
S31 tools, creates the SDK Python venv, and installs the pinned Ninja package.
It does not reset an existing SDK at a different revision. If you already have
the installation created for this project, skip setup and run the build directly.

For another toolchain location, pass `--tools-root /your/path` to setup and set
`JAXCORE_TOOLS_ROOT=/your/path` when calling `scripts/idf.sh`. If multiple IDF
Python environments exist there, select one with `IDF_PYTHON_ENV_PATH`.

On macOS, Codex's restricted shell may block the component manager's process
inspection. Allow the build command to run outside that sandbox when requested;
the SDK's normal build tools perform this inspection. Compilation itself needs
no attached device.

Generated configuration and output are in `build/s31`, separate from host tests:

- `jaxcore-doom.elf`: linked RISC-V application with debug symbols.
- `jaxcore-doom.bin`: application image.
- `bootloader/bootloader.bin` and `partition_table/partition-table.bin`.
- `verification.json`: architecture, required engine symbols, size and image hash.

The verification script rejects an ELF that omits the game loop, renderer,
gameplay ticker, mixer, cache, or platform interfaces. It also checks the S31
profile excludes the legacy hardware backends. This is not an emulator test.

The S31 partition table uses a factory application partition and no WAD flash
partitions. OTA and production partitioning remain future decisions. The prior
S3 settings are preserved as `config/sdkconfig.s3.reference` and
`config/sdkconfig.defaults.s3.reference`; they are not validated build profiles
for this SDK migration.

## Reference measurements

The first successful complete build produced:

| Measurement | Bytes |
| --- | ---: |
| Application `.bin` | 1,469,440 |
| Static DIRAM use (code/data/BSS combined) | 200,114 |
| Static DIRAM remaining in linker layout | 303,630 |
| Factory application partition | 15,728,640 |

These measurements precede the SDMMC/FAT component; current image size and hash
are recorded in `build/s31/verification.json`. They are linker/build measurements.
Remaining runtime heap is smaller after
stacks and dynamic allocations. PSRAM capacity is detected at boot; none of
these figures establishes runtime memory headroom, throughput or frame rate.

The five desktop suites pass with address/undefined-behavior sanitizers:

```sh
cmake -S tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/host --output-on-failure
```

A manual GitHub Actions workflow, **S31 firmware build (manual)**, runs the
same pinned setup, complete build and ELF checks on Linux and uploads build
artifacts. Dispatch becomes available after the workflow is on the fork's default
branch. It is not automatically scheduled and was not run as part of the initial
local macOS validation.

## What remains

SDMMC/FAT mounting is implemented and compile-checked. Next implement a WAD
provisioning/upload path, followed by the board configuration, ST7789
scaling/transport, NAU8325 clocks/registers,
and logical controls. Keep unassigned pins and physical output disabled until
each driver has a deliberate configuration. Validate memory, timing, audio and
display behavior on the fabricated modules before calling this a playable port.
