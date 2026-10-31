cmake_minimum_required(VERSION 3.16)
project(jaxcore_doom_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
enable_testing()
option(DOOM_SANITIZERS "Enable address and undefined behavior sanitizers" ON)
if(DOOM_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
add_compile_options(-Wall -Wextra -Werror -UNDEBUG)
set(ENGINE "${CMAKE_CURRENT_LIST_DIR}/../../components/prboom")
set(PLATFORM "${CMAKE_CURRENT_LIST_DIR}/../../components/prboom-esp32-compat")

add_library(pcm_mixer STATIC "${ENGINE}/pcm_mixer.c")
target_include_directories(pcm_mixer PUBLIC "${ENGINE}/include")
target_link_libraries(pcm_mixer PUBLIC m)
add_library(wad_file STATIC "${PLATFORM}/wad_file.c")
target_include_directories(wad_file PUBLIC "${PLATFORM}/include")

add_executable(test_pcm test_pcm.c)
target_link_libraries(test_pcm PRIVATE pcm_mixer)
add_test(NAME pcm_mixer COMMAND test_pcm)

add_executable(test_wad test_wad.c)
target_link_libraries(test_wad PRIVATE wad_file)
add_test(NAME wad_filesystem COMMAND test_wad)

# Compile the real engine adapters/cache, with only hardware and zone allocation
# replaced by deterministic fakes. Legacy engine headers have old-style APIs.
add_executable(test_audio test_audio.c "${ENGINE}/i_audio.c"
               "${ENGINE}/i_sound.c" "${ENGINE}/dbopl.c")
target_include_directories(test_audio PRIVATE "${ENGINE}" "${ENGINE}/include")
target_compile_options(test_audio PRIVATE -Wno-unused-parameter
    -Wno-unused-variable -Wno-sign-compare -Wno-deprecated-non-prototype)
set_source_files_properties("${ENGINE}/dbopl.c" PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-but-set-variable")
target_compile_definitions(test_audio PRIVATE HAVE_CONFIG_H)
target_link_libraries(test_audio PRIVATE pcm_mixer)
add_test(NAME engine_audio_adapter COMMAND test_audio)

add_executable(test_cache test_cache.c "${ENGINE}/w_mmap.c")
target_include_directories(test_cache PRIVATE "${ENGINE}/include")
target_compile_definitions(test_cache PRIVATE HAVE_CONFIG_H)
target_link_libraries(test_cache PRIVATE wad_file)
add_test(NAME wad_cache_pins COMMAND test_cache)

set(STORAGE "${CMAKE_CURRENT_LIST_DIR}/../../components/jaxcore-storage")
add_executable(test_storage_sdmmc test_storage_sdmmc.c "${STORAGE}/storage_sdmmc.c")
target_include_directories(test_storage_sdmmc PRIVATE storage_fakes "${STORAGE}/include")
add_test(NAME storage_sdmmc_lifecycle COMMAND test_storage_sdmmc)
