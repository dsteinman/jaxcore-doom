# Jaxcore Doom: ESP32-S31 port in progress

This work starts from `pablosguajardo/doom-esp32` commit
`3a18aeb863415acbb9d96f3a70443c475f693053`.

The S31 profile now **builds and links a complete RISC-V firmware image** using
a pinned ESP-IDF revision. It contains the Doom engine, filesystem WAD adapter,
SDMMC/FAT mounting for fixed SD-NAND, stereo effects mixer and startup diagnostics.
Display and inputs are disabled; audio uses a paced software sink. WAD uploading,
ST7789 output and NAU8325 setup remain pending. No Jaxcore hardware has been
fabricated or tested.

Start with [the S31 build instructions](docs/S31_BUILD.md). The planned display
is **284x240 landscape RGB565**, separate from the **320x200 indexed renderer**.
See [storage bring-up](docs/STORAGE.md) for pin mapping, failure diagnostics and
the filesystem expected on SD-NAND. Startup never formats storage.

See [the firmware port guide](docs/JAXCORE_PORT.md) for implemented behavior,
desktop test commands, and the remaining peripheral work.

The original upstream instructions below describe the ESP32-S3 reference board.

# 🎮 ESP32-S3 DOOM Project

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2-blue)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-green)
![Status](https://img.shields.io/badge/Status-Working-success)

---
## Video:
[![VIDEO](https://i.ytimg.com/vi/qNEunaMxIuw/oardefault.jpg?sqp=-oaymwEoCM0CENAFSFqQAgHyq4qpAxcIARUAAIhC2AEB4gEKCBgQAhgGOAFAAQ==&rs=AOn4CLCoRj9TizZzFQM5cpn4D6y-Ckk-OA&usqp=CCk)](https://www.youtube.com/shorts/qNEunaMxIuw)

---
# 🚀 Overview

This project runs **DOOM (PrBoom)** on an **ESP32-S3**, with:

- 🎮 GPIO or Bluetooth joystick input  
- 📺 SPI display support  
- 🔊 I2S audio output  
- 💾 External PSRAM support  
- 📦 Flash-based WAD loading  

Fully built using **ESP-IDF v5.2**.

---

# 🧰 Prerequisites

| Tool | Version |
|------|----------|
| ESP-IDF | v5.2 |
| Python | 3.13.0 |
| Board | ESP32-S3 |
| WAD | DOOM1.WAD |

ESP-IDF installation guide:  
👉 https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

---

# 🖥️ ESP-IDF Environment

## Run ESP-IDF manually

```bat
C:\esp\v5.2\esp-idf\export.bat
```

---

# ⚙️ Configuration

Open menuconfig:

```bat
idf.py menuconfig
```

---

# 🔄 Dependency Changes

```bat
idf.py reconfigure
```

---

# 🏗️ Build

```bat
idf.py build
```

### 🧹 Clean build (recommended)

```bat
idf.py fullclean
```

Or both:

```bat
idf.py fullclean build
```

### 🎯 Set target (usually not required)

```bat
idf.py set-target esp32s3
```

---

# 🚀 Flashing & Deployment

## ✅ Step 1 – Flash Partition Table

```bat
idf.py partition-table
idf.py -p COM5 partition-table-flash
```

---

## ✅ Step 2 – Flash WAD Files

### CMD

```cmd
python "%IDF_PATH%\components\esptool_py\esptool\esptool.py" --chip esp32s3 --port COM5 --baud 230400 write_flash 0x200000 prboom.wad
python "%IDF_PATH%\components\esptool_py\esptool\esptool.py" --chip esp32s3 --port COM5 --baud 230400 write_flash 0x280000 DOOM1.WAD
```

### PowerShell

```powershell
python "$env:IDF_PATH\components\esptool_py\esptool\esptool.py" --chip esp32s3 --port COM5 --baud 230400 write_flash 0x200000 prboom.wad
python "$env:IDF_PATH\components\esptool_py\esptool\esptool.py" --chip esp32s3 --port COM5 --baud 230400 write_flash 0x280000 DOOM1.WAD
```

---

## ✅ Step 3 – Flash Firmware

```bat
idf.py -p COM5 flash
```

---

# ⚡ One-Click Flash Scripts

## 🪟 Windows (.bat)

```bat
@echo off
idf.py fullclean
idf.py build
idf.py -p COM5 flash
pause
```

## 💻 PowerShell (.ps1)

```powershell
idf.py fullclean
idf.py build
idf.py -p COM5 flash
```

---

# 🐞 Debugging

## 🔍 Serial Monitor

```bat
idf.py -p COM5 monitor
```

Exit with:  
`Ctrl + ]`

---

## 🧠 Decode Backtrace

```bat
xtensa-esp32s3-elf-addr2line -pfiaC -e build\esp-doom.elf <addresses>
```

---

# ⚠️ Watchdog

If needed, disable:

```
Component config
  → ESP System Settings
      → [ ] Interrupt watchdog
```

---

# 🎮 Controls

## 🔧 Select Input Mode

`CONFIG_HW_JOYSTICK_OPTION`
Default: 2

| Value | Mode |
|-------|------|
| 1 | GPIO buttons |
| 2 | Bluetooth joystick (BLE) |

 
if you set it to 1, it compiles to be used with GPIO buttons; 
if you set it to 2, it compiles to use a virtual "joystick" via Bluetooth.


Change via:

```bat
idf.py menuconfig
```

Path:

```
ESP32-Doom platform-specific configuration
  → (1/2) HW_JOYSTICK_OPTION
```

---

# 📡 Bluetooth (BLE Mode)

When `HW_JOYSTICK_OPTION = 2`:


✅ Uses NimBLE (BLE only)  
✅ Nordic UART style input  
✅ Supports HOLD and TAP input modes  

### Required menuconfig settings:

Run:
```bat
idf.py menuconfig
```

```
Component config
  → Bluetooth
     [*] Bluetooth
     Host → NimBLE - BLE only
     Controller → Enabled
```
## 🎮 BLE Joystick for Android
You can use the bacon application to create a generic virtual joystick:
👉 https://play.google.com/store/apps/details?id=com.jerameeldelosreyes.bacon

![alt tag](https://raw.githubusercontent.com/pablosguajardo/templates/main/img/doom/Bacon_joy.jpg)

### Button Mapping (Virtual Joystick Configuration)

In BLE mode, the ESP32 receives **single-character commands** via Nordic UART.
You must configure each button in the Bacon app to send the following characters.

### 🧭 Movement

| Action | Press | Release (HOLD mode only) |
|--------|--------|--------------------------|
| Forward | `F` | `f` |
| Backward | `T` | `t` |
| Left | `L` | `l` |
| Right | `R` | `r` |

---

### 🔫 Main Actions

| Action | Press | Release (HOLD mode only) |
|--------|--------|--------------------------|
| Fire | `D` | `d` |
| Use / Open | `C` | `c` |
| Weapon Toggle | `A` | `a` |
| Pause | `P` | `p` |
| Menu Enter | `S` | `s` |
| Escape | `B` | `b` |

---

### 🏃 Advanced Movement (HOLD mode only)

| Action | Press | Release |
|--------|--------|----------|
| Strafe Left | `X` | `x` |
| Strafe Right | `Y` | `y` |
| Run (Speed) | `Z` | `z` |

---

## ⚙️ HOLD vs TAP Mode

Defined in:

```
components/prboom-esp32-compat/gamepad.c
```

```
#define BT_INPUT_MODE_HOLD 1
#define BT_INPUT_MODE_TAP  2
```

### ✅ HOLD Mode
- Button sends keydown on uppercase
- Button sends keyup on lowercase
- Continuous movement while pressed
- Recommended for virtual joystick apps

### ✅ TAP Mode
- Uppercase triggers a short impulse
- No release character required
- Good for button-style input

Default is **HOLD mode**.

---

## 📱 How to Configure Bacon App

1. Create a button.
2. Set **Action Type → Send Text**
3. Enter the corresponding character (example: `F`)
4. For HOLD mode:
   - Configure "Button Down" → uppercase
   - Configure "Button Up" → lowercase

Example:
- Left button:
  - Down → `L`
  - Up → `l`
  
![alt tag](https://raw.githubusercontent.com/pablosguajardo/templates/main/img/doom/Bacon_config.jpg)

---


# 🎛️ GPIO Configuration (HW_JOYSTICK_OPTION = 1)

## 🧭 Directions

| Action | GPIO |
|--------|------|
| UP     | 8 |
| DOWN   | 9 |
| LEFT   | 10 |
| RIGHT  | 11 |

## 🔫 Main Actions

| Action | GPIO |
|--------|------|
| FIRE   | 12 |
| USE    | 2 |
| RUN    | 16 |
| STRAFE | 18 |

## 🖥️ System

| Action | GPIO |
|--------|------|
| ESC    | 13 |
| MAP    | 14 |
| ENTER  | 19 |
| PAUSE  | 20 |

---
# 🎛️ GPIO Configuration Audio/Display

# 🔊 Audio (I2S)
| MAX98357A | GPIO |
|------------|-------------|
| VCC | 3.3V |
| GND | GND |
| BCLK | 17 |
| LRC (WS) | 47 |
| DIN (DOUT)| 15 |
| SD | VCC |
| GAIN | GND |

Check the voltages if there is no sound:

Connect the multimeter to:

- GPIO17 → you should see an average of ~1V–2V (activity)
- GPIO47 → ~1V average

If both are solid at 0V → the task is not writing.


---

# 📺 Display (SPI)

| Signal | GPIO |
|--------|------|
| MOSI (SDA)   | 6 |
| CLK (SCL)   | 7 |
| CS     | 5 |
| DC     | 4 |
| RST    | 48 |
| BCKL   | 45 |

---

# 🧠 Notes

- ✅ Flash order matters: **partition → firmware → WAD**
- ✅ WAD must match partition offsets
- ✅ Avoid flash/PSRAM pins
- ✅ External power recommended for peripherals
- ✅ BLE mode supports TAP impulse input

---

# ⭐ Tips

- Use `idf.py flash -v` for verbose flashing
- Use `idf.py -p COM5 monitor` for debug, with `Ctrl+]` to exit
- Keep WAD flashing separate from firmware
- Use external power when using display + audio
- PSRAM is recommended for stable performance

---

# 🐞 Known Limitations

- ❌ Savegames are not supported
- ❌ Bluetooth Classic is NOT supported (ESP32-S3 is BLE only)

---

# 👥 Credits

- DOOM source released by **id Software (1999)** under GNU GPL  
- **PrBoom** authors credited in `components/prboom/AUTHORS`  
- ESP32 adaptations by **Espressif Systems** (Apache 2.0 License)

---

# 📜 License

This project is open source and freely available.

✅ You may use it  
✅ Modify it  
✅ Distribute it  
✅ Use it commercially  

Provided “as is” without warranty.

Contributions and improvements are welcome!

---

# 🚀 Ready to run DOOM on ESP32-S3

Let’s rip and tear 🔥
