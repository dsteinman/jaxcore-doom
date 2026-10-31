set(main_sources "app_main.c")
set(main_dependencies prboom prboom-esp32-compat esp_psram esp_system
                      esp_partition spi_flash nvs_flash heap)
if(CONFIG_DOOM_LEGACY_INPUT AND CONFIG_HW_JOYSTICK_OPTION EQUAL 2)
    list(APPEND main_sources "bt_manager.c")
    list(APPEND main_dependencies bt)
endif()
idf_component_register(SRCS ${main_sources} INCLUDE_DIRS "."
                       PRIV_REQUIRES ${main_dependencies})
