# Jaxcore Doom firmware foundation

Target: Jaxcore Control S31 with 16 MB flash/PSRAM, SD-NAND over SDMMC,
240x284 ST7789 TFT, and NAU8325 stereo output. The module/carrier hardware is
not fabricated. This change establishes portable firmware interfaces and tests;
it does not claim a linked or bootable S31 firmware image.

## Implemented

### WAD files

`components/prboom-esp32-compat/wad_file.c` implements the engine's existing
`I_Open`, `I_Read`, `I_Lseek`, `I_Filelength`, and `I_Close` APIs through POSIX
file access. The same source runs on a desktop filesystem or ESP-IDF VFS.

- Relative names resolve below `CONFIG_DOOM_WAD_ROOT` (default `/sdcard/doom`).
  Absolute paths are also supported. The host test default is the current directory.
- Reads advance file position, retry interrupted/partial reads, and report
  unexpected EOF through `I_Error` instead of returning uninitialized data.
- Read-only regular files are supported, with seek and individual file size
  bounded by the engine's existing signed `int` API (`INT_MAX` bytes).
- Enable `CONFIG_DOOM_WAD_FILESYSTEM` to select this backend and bypass the old
  flash-partition checks. The upstream partition backend remains the default.
- The application must mount storage before `doom_main`. This patch does not
  mount, format, provision, or upload files to SD-NAND. Place both the chosen
  IWAD and the required `prboom.wad` in the configured directory once that exists.

WAD directory/lump structural validation is still inherited from upstream;
the new bounds checks cover file size, read completion, and DMX sound payloads.

### Stereo effects

`components/prboom/pcm_mixer.c` renders signed 16-bit interleaved stereo PCM.
All lengths at the render/transport boundary count **frames**, each containing
two samples, left followed by right.

- Up to 32 voices, matching PrBoom's existing configurable maximum.
- Independent volume and separation updates; separation 0 is left, 128 center,
  256 right. Linear gain distribution gives half gain on each side at center.
- Nearest-neighbor sample-rate conversion with 16 fractional position bits;
  pitch 128 is original and 64 steps change pitch by one octave. Higher-quality
  interpolation and comparison with vanilla pitch behavior can follow profiling.
- DMX format-3 parsing uses explicit little-endian fields, checks payload bounds,
  removes 16 padding bytes from each end, and centers unsigned PCM around 128.
  Tiny payloads (48 bytes or fewer) are rejected, following the conventional
  DMX playback treatment in Chocolate Doom's `src/i_sdlsound.c`.
- Wide accumulation with final signed saturation, including optional centered
  OPL music. Music remains the existing compiled IMF/OPL implementation at unity
  gain; WAD-driven music compatibility, timing, pause and volume remain follow-up work.
- Rendering performs no allocation, storage reads, or cache operations.

`i_audio.c` now uses this mixer for start, stop, update and playing-status APIs;
the corresponding stubs have been removed from `i_sound.c`.

### Ownership and tasks

All game-side sound/cache APIs must be called on the game task. The audio
callback and game-side voice changes share the existing transport mutex.

Each `W_CacheLumpNum` or `W_LockLumpNum` acquires one reference. Data is tagged
non-purgeable until the final matching `W_UnlockLumpNum`. This restores the
engine's existing paired cache/unlock contract, which the upstream heap-cache
modification did not maintain. Multiple voices may safely share a sound lump.

Sound loading happens on the game task before taking the audio mutex. A finished
voice retains its cache reference until the game task polls, stops, or replaces
it. This keeps the zone allocator out of the callback and prevents use-after-free
when gameplay allocates other assets. `I_AnySoundStillPlaying` also collects
finished voices. Normal gameplay must continue polling sound status.

### I2S transport

`sndhw.c` accepts stereo frames from the engine. Enable
`CONFIG_DOOM_AUDIO_STEREO` for two independent I2S slots. With it disabled,
transport averages the L/R pair for the original mono hardware. Partial writes
are completed before the output buffer is reused. The 560-frame chunk is a
buffer size, not an IMF tick or millisecond duration.

This patch retains the upstream I2S pins, sample rate (22050 Hz), WS inversion,
task affinity, and unused MCLK. It does not initialize NAU8325. These settings
must be replaced by the reviewed board/codec configuration before use on Jaxcore.
The desired final output rate is not yet chosen.

### OPL portability fixes

The real audio adapter test exercises OPL initialization. Sanitizers exposed
null-pointer member expressions used to compute offsets, signed overflow in
attack-rate multiplication, and signed high-bit shifts. Those operations now
use `offsetof`, 64-bit intermediate multiplication, and unsigned bit shifts.
This does not establish that the full legacy OPL music path is free of bugs.

## Verification

No ESP-IDF installation or WAD assets are needed for the desktop tests:

```sh
cmake -S tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/host --output-on-failure
```

Address and undefined-behavior sanitizers are enabled by default on GCC/Clang.
Tests keep assertions enabled in all build configurations. The four suites cover:

1. DMX bounds, silence midpoint, stereo panning, live updates, volume, pitch,
   resampling, exact sample end, 32 simultaneous voices and clipping.
2. WAD paths, sequential reads, relative/end seeks, EOF, read-only behavior,
   missing files, and the signed file-size boundary.
3. The actual Doom audio adapter, OPL initialization, callback output,
   shared sound pins, replacement, invalid sounds and shutdown.
4. The actual WAD cache with a simulated zone allocator, including shared
   ownership, final release, eviction and reload through the filesystem backend.

An optional compilation check uses an already-installed official S31 Arduino
SDK and RISC-V compiler:

```sh
python3 scripts/check_s31_objects.py \
  --gcc /path/to/esp-rv32/version/bin/riscv32-esp-elf-gcc \
  --sdk /path/to/esp32s31-libs/version
```

This compiles the changed engine/backend sources and both I2S modes to RISC-V
objects, using GNU C11 for legacy engine compatibility. It does **not** configure
or link the complete ESP-IDF project, exercise partition storage, verify task
timing, or run hardware. The initial local check used Arduino core 4.0.0-RC1's
S31 SDK, IDF `v6.1-803-g94639ab7251`, and Espressif RISC-V tools `2512`.

## Next changes

1. Set up and pin a standalone S31-capable ESP-IDF checkout. Migrate CMake,
   the Xtensa dependency, ROM/SOC includes and SDK configuration. Enforce the
   engine's C11 compatibility and build/link the whole application for S31.
2. Add a board component and diagnostics entry point. Keep unresolved pins
   explicitly unassigned; do not inherit S3 button/display pins into S31 defaults.
3. Mount SD-NAND over SDMMC, with an explicit provisioning/upload path and
   recoverable errors. Keep storage formatting a separate deliberate operation.
4. Separate the indexed renderer/scaler from the ST7789 transport; validate
   landscape/aspect ratio with desktop image fixtures before hardware tests.
5. Implement NAU8325 I2C initialization, MCLK/sample-format configuration and
   independent L/R diagnostic tones. Check its supported clocking/rates first.
6. Add logical input events and configurable button/serial backends.
7. Profile allocation, PSRAM placement, display bandwidth, music CPU cost,
   scheduling and audio underruns on the fabricated modules.

Savegames, broad WAD compatibility and alternative music/Beep backends remain
outside this first change. Existing upstream engine notices and assets are
retained; the repository's original asset presence is not a redistribution review.
