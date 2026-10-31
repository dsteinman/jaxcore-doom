# Fixed SD-NAND bring-up

The `jaxcore-storage` component initializes the soldered SD-NAND through native
SDMMC, discovers its reported capacity and mounts an existing FAT filesystem.
It is wired into startup before the existing WAD file checks. This code has
desktop control-flow tests and a complete S31 firmware build; it has not run
against a fabricated Jaxcore board or MKDV4GCL-ABF device.

## Bus and configuration

The mapping comes from the supplied 2026-10-30 hardware handoff and also matches
the pinned SDK's S31 slot defaults. It is explicit in `jaxcore_storage.h`:

| Signal | GPIO |
| --- | ---: |
| DAT0 | 20 |
| DAT1 | 21 |
| DAT2 | 22 |
| DAT3 | 23 |
| CLK | 24 |
| CMD | 25 |

- SDMMC Slot 0, four data lines, 3.3 V signaling.
- External CMD/DAT pull-ups are expected on the Control module. Internal
  pull-ups are disabled; there are no card-detect or write-protect pins.
- Eight-bit transfers, DDR and UHS slot mode are disabled. No power-control
  driver or 1.8 V switch is installed.
- `CONFIG_JAXCORE_STORAGE_MAX_FREQ_KHZ` defaults to 10000. The SDK performs
  initial probing at its normal 400 kHz, then negotiates within this ceiling.
  Supported configured ceilings are 400-20000 kHz. Performance remains unmeasured.
- `CONFIG_JAXCORE_STORAGE_SDMMC` enables this component on S31 with filesystem
  WAD access. Disabling it excludes storage initialization from startup.
- This component exclusively owns the SDMMC controller. No external SD bus
  sharing is intended on this board variant.

## Initialization sequence

1. Initialize the SDMMC host and Slot 0 GPIOs.
2. Initialize the SD device and report its CID/CSD details and capacity. The
   capacity is derived from reported sector count and sector size, not the BOM.
3. Allocate a free FatFS drive and register its SDMMC disk-I/O adapter.
4. Register `/sdcard` with ESP-IDF VFS, allowing 16 simultaneous open files.
5. Mount an existing FAT volume with `f_mount`.
6. Startup checks for `/sdcard/doom/DOOM1.WAD` and
   `/sdcard/doom/prboom.wad`, then starts the engine only if both open.

The explicit sequence uses the pinned SDK's public APIs; it does not call its
private convenience helpers. The media must already expose a partition/volume
layout recognized by this FatFS configuration. Its factory layout is unknown.
Mounting does not prove file content, read speed, or gameplay compatibility.

## Formatting and uploading

There is no automatic-format flag or formatting API in the component. No files
or directories are created at startup. The WAD adapter opens files read-only;
the FAT volume itself is not globally write-protected, so a future provisioning
tool can write to it deliberately.

An unformatted or unrecognized volume reports `FR_NO_FILESYSTEM` and stops
startup. Do not treat a mount failure as authorization to format the device.
An explicit provisioning/format operation and a host-to-device WAD uploader
remain to be implemented; this change does not make soldered NAND appear as
a removable USB drive or transfer the repository's WADs onto a device.

## Failures and ownership

`jaxcore_storage_mount()` returns an `esp_err_t`. Its status snapshot records:

- The failed stage: host, slot, media enumeration, drive allocation, VFS
  registration, or FAT mount.
- The primary error and any cleanup error.
- Raw FatFS result (`-1` if mounting was not attempted).
- Reported capacity, if media enumeration completed.

This distinguishes a bus/device problem from a chip that enumerates but has no
recognized FAT filesystem. A missing WAD is reported only after mounting succeeds.

Initialization failures unwind only resources acquired by this component.
An existing VFS registration owned by another caller is never unregistered.
Successful repeated mount calls are idempotent. Cleanup failures retain the
remaining resources; call `jaxcore_storage_unmount()` to retry cleanup before
another mount, or reboot when diagnosing hardware. No retry reformats storage.

All mount/unmount/status calls belong to a serialized startup/control task.
Before unmounting, stop Doom/upload activity and close every open file. There
is no hot-plug support or concurrent mount/unmount contract for this fixed chip.

## Validation

The fifth desktop suite, `storage_sdmmc_lifecycle`, compiles the actual component
against a small fake SDK surface and injects failures at each initialization
stage. It verifies cleanup order, retry, repeated calls, preservation of another
VFS owner's registration, and cleanup-failure recovery. It checks the exact
pin/configuration values. It does not simulate the SD protocol or a FAT volume.

The full S31 build uses the real pinned SDK headers and libraries. The ELF
checker requires the storage mount, SDMMC enumeration and FAT/VFS symbols when
storage is enabled, and rejects linked formatting/partition-creation functions.

```sh
cmake --build build/host
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/host --output-on-failure
scripts/idf.sh build
python3 scripts/check_s31_elf.py
```

Hardware acceptance still requires device enumeration, real capacity reporting,
mounting without writes, host/device WAD checksum agreement, sequential reads
and random seeks, repeat boots and error recovery. Keep those results separate
from compile/link and fake-SDK tests.
