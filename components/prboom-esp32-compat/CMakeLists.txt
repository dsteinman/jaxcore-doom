cmake_minimum_required(VERSION 3.5)

set(WAD_BACKEND_SOURCE "")
if(CONFIG_DOOM_WAD_FILESYSTEM)
  set(WAD_BACKEND_SOURCE "wad_file.c")
endif()

idf_component_register(
SRCS
  ${WAD_BACKEND_SOURCE}
  "gamepad.c"
  "i_main.c"
  "i_network.c"
  "i_system.c"
  "i_video.c"
  "psxcontroller.c"
  "spi_lcd.c"
  "tsinput.c"
  "sndhw.c"

INCLUDE_DIRS
  "include" "."

PRIV_REQUIRES
  prboom
  heap
  spi_flash
  xtensa
  spi_flash
  esp_partition
  driver
  es8311
)

#Need this otherwise linking fails because of some sort of link optimization for sndhw...
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sndhw_init")
