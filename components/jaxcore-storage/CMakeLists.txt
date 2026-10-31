if(CONFIG_JAXCORE_STORAGE_SDMMC)
    set(storage_source "storage_sdmmc.c")
else()
    set(storage_source "storage_disabled.c")
endif()
idf_component_register(SRCS ${storage_source} INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES esp_common
                       # Requirements are expanded before Kconfig is loaded.
                       PRIV_REQUIRES fatfs sdmmc esp_driver_sdmmc)
