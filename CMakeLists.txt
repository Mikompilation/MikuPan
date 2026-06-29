cmake_minimum_required(VERSION 3.28.0)
project(MikuPan LANGUAGES C CXX)

if (NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration -fno-fast-math -fno-trapping-math -Wno-int-conversion")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "-static")
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/extern/icon.rc")
endif()

#add_compile_definitions(BUILD_EU_VERSION)

if(ANDROID)
    set(MIKUPAN_ANDROID_PAGE_SIZE "16384"
            CACHE STRING "Maximum Android ELF page size for native shared libraries.")
    set(MIKUPAN_ANDROID_PAGE_SIZE_LINK_FLAGS
            "-Wl,-z,max-page-size=${MIKUPAN_ANDROID_PAGE_SIZE}")
    foreach(linker_flag_var
            CMAKE_EXE_LINKER_FLAGS
            CMAKE_SHARED_LINKER_FLAGS
            CMAKE_MODULE_LINKER_FLAGS)
        set(${linker_flag_var}
                "${${linker_flag_var}} ${MIKUPAN_ANDROID_PAGE_SIZE_LINK_FLAGS}")
    endforeach()
endif()

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/MikuPan)

include(sdl3)
include(cimgui)
include(spdlog)
include(cglm)
include(ffmpeg)
include(xxhash)
include(inipp)
include(shadercross)

set(MIKUPAN_SOURCES
        resources/shaders/textured_mesh_lighted.frag
        resources/shaders/bounding_box.vert
        resources/shaders/mesh_0x2.vert
        resources/shaders/mesh_0x12.vert
        resources/shaders/mesh_0xA.vert
        resources/shaders/sprite.frag
        resources/shaders/sprite.vert
        resources/shaders/untextured_coloured_sprite.frag
        resources/shaders/untextured_coloured_sprite.vert
        resources/shaders/normals_debug.geom
        resources/shaders/normals_debug.frag
        "src/mikupan/mikupan_file.cpp"
        "src/mikupan/mikupan_first_person.cpp"
        "src/common/ul_math.cpp"
        "src/common/utility.cpp"
        "src/edit/map_data.cpp"
        "src/edit/mde_disp.cpp"
        "src/edit/mde_file.cpp"
        "src/edit/mde_fmnu.cpp"
        "src/edit/mde_main.cpp"
        "src/edit/mde_mdsp.cpp"
        "src/edit/mde_menu.cpp"
        "src/edit/test_2d.cpp"
        "src/graphics/graph2d/compress.cpp"
        "src/graphics/graph2d/effect.cpp"
        "src/graphics/graph2d/effect_ene.cpp"
        "src/graphics/graph2d/effect_obj.cpp"
        "src/graphics/graph2d/effect_oth.cpp"
        "src/graphics/graph2d/effect_rdr.cpp"
        "src/graphics/graph2d/effect_scr.cpp"
        "src/graphics/graph2d/effect_sub.cpp"
        "src/graphics/graph2d/effect_sub2.cpp"
        "src/graphics/graph2d/g2d_debug.cpp"
        "src/graphics/graph2d/g2d_main.cpp"
        "src/graphics/graph2d/message.cpp"
        "src/graphics/graph2d/number.cpp"
        "src/graphics/graph2d/rare_ene.cpp"
        "src/graphics/graph2d/snapshot.cpp"
        "src/graphics/graph2d/subtitles.cpp"
        "src/graphics/graph2d/takep_pe.cpp"
        "src/graphics/graph2d/tim2.cpp"
        "src/graphics/graph2d/tim2_new.cpp"
        "src/graphics/graph3d/gra3d.cpp"
        "src/graphics/graph3d/leditor.cpp"
        "src/graphics/graph3d/load3d.cpp"
        "src/graphics/graph3d/mirror.cpp"
        "src/graphics/graph3d/object.cpp"
        "src/graphics/graph3d/sce_pp3.cpp"
        "src/graphics/graph3d/sgcam.cpp"
        "src/graphics/graph3d/sgdma.cpp"
        "src/graphics/graph3d/sglib.cpp"
        "src/graphics/graph3d/sglight.cpp"
        "src/graphics/graph3d/sgsgd.cpp"
        "src/graphics/graph3d/sgsu.cpp"
        "src/graphics/graph3d/sgsup.cpp"
        "src/graphics/graph3d/shadow.cpp"
        "src/graphics/motion/accessory.cpp"
        "src/graphics/motion/acs_dat.cpp"
        "src/graphics/motion/anicode.cpp"
        "src/graphics/motion/mdlact.cpp"
        "src/graphics/motion/mdldat.cpp"
        "src/graphics/motion/mdlwork.cpp"
        "src/graphics/motion/mime.cpp"
        "src/graphics/motion/mim_dat.cpp"
        "src/graphics/motion/motion.cpp"
        "src/graphics/mov/movie.cpp"
        "src/graphics/scene/fod.cpp"
        "src/graphics/scene/scene.cpp"
        "src/ingame/camera/camera.cpp"
        "src/ingame/enemy/ene_ctl.cpp"
        "src/ingame/enemy/move_ctl.cpp"
        "src/ingame/entry/ap_bgost.cpp"
        "src/ingame/entry/ap_dgost.cpp"
        "src/ingame/entry/ap_fgost.cpp"
        "src/ingame/entry/fgst_dat.cpp"
        "src/ingame/entry/ap_ggost.cpp"
        "src/ingame/entry/ap_pgost.cpp"
        "src/ingame/entry/ap_rgost.cpp"
        "src/ingame/entry/rgst_dat.cpp"
        "src/ingame/entry/ap_sgost.cpp"
        "src/ingame/entry/ap_zgost.cpp"
        "src/ingame/entry/entry.cpp"
        "src/ingame/event/ev_load.cpp"
        "src/ingame/event/ev_main.cpp"
        "src/ingame/event/ev_spcl.cpp"
        "src/ingame/event/mv_cmnd.cpp"
        "src/ingame/event/wan_dat.cpp"
        "src/ingame/event/wan_soul.cpp"
        "src/ingame/info/inf_disp.cpp"
        "src/ingame/map/furn_spe/adj_valg.cpp"
        "src/ingame/map/furn_spe/fsla_main.cpp"
        "src/ingame/map/furn_spe/fspe_acs.cpp"
        "src/ingame/map/furn_spe/furn_spe.cpp"
        "src/ingame/map/doortype.cpp"
        "src/ingame/map/door_conte.cpp"
        "src/ingame/map/door_ctl.cpp"
        "src/ingame/map/find_ctl.cpp"
        "src/ingame/map/furn_ctl.cpp"
        "src/ingame/map/furn_dat.cpp"
        "src/ingame/map/furn_eff.cpp"
        "src/ingame/map/item_ctl.cpp"
        "src/ingame/map/map_area.cpp"
        "src/ingame/map/map_ctrl.cpp"
        "src/ingame/map/map_htck.cpp"
        "src/ingame/map/rotd_ctl.cpp"
        "src/ingame/menu/gameover.cpp"
        "src/ingame/ig_glob.cpp"
        "src/ingame/menu/ig_album.cpp"
        "src/ingame/menu/ig_camra.cpp"
        "src/ingame/menu/ig_file.cpp"
        "src/ingame/menu/ig_glst.cpp"
        "src/ingame/menu/ig_map.cpp"
        "src/ingame/menu/ig_menu.cpp"
        "src/ingame/menu/ig_option.cpp"
        "src/ingame/menu/ig_rank.cpp"
        "src/ingame/menu/ig_spd_menu.cpp"
        "src/ingame/menu/item.cpp"
        "src/ingame/menu/item_get.cpp"
        "src/ingame/menu/pause.cpp"
        "src/ingame/menu/sp_menu.cpp"
        "src/ingame/photo/photo.cpp"
        "src/ingame/photo/pht_main.cpp"
        "src/ingame/photo/pht_make.cpp"
        "src/ingame/plyr/plyr_ctl.cpp"
        "src/ingame/plyr/time_ctl.cpp"
        "src/ingame/plyr/unit_ctl.cpp"
        "src/ingame/ig_init.cpp"
        "src/ingame/ingame.cpp"
        "src/iop/iopmain.cpp"
        "src/iop/adpcm/iopadpcm.cpp"
        "src/iop/adpcm/ia_cmd.cpp"
        "src/iop/adpcm/ia_func.cpp"
        "src/iop/se/iopse.cpp"
        "src/iop/se/sedat.cpp"
        "src/iop/se/voice.cpp"
        "src/iop/cdvd/iopcdvd.cpp"
        "src/main/gamemain.cpp"
        "src/main/glob.cpp"
        "src/main/main.cpp"
        "src/mc/mc_acs.cpp"
        "src/mc/mc_album.cpp"
        "src/mc/mc_at.cpp"
        "src/mc/mc_disp.cpp"
        "src/mc/mc_exec.cpp"
        "src/mc/mc_main.cpp"
        "src/mc/mc_start.cpp"
        "src/mc/mc_icon.cpp"
        "src/os/eeiop/adpcm/ea_autog.cpp"
        "src/os/eeiop/adpcm/ea_btlmode.cpp"
        "src/os/eeiop/adpcm/ea_cmd.cpp"
        "src/os/eeiop/adpcm/ea_ctrl.cpp"
        "src/os/eeiop/adpcm/ea_dat.cpp"
        "src/os/eeiop/adpcm/ea_event.cpp"
        "src/os/eeiop/adpcm/ea_furn.cpp"
        "src/os/eeiop/adpcm/ea_gameover.cpp"
        "src/os/eeiop/adpcm/ea_gdead.cpp"
        "src/os/eeiop/adpcm/ea_ghost.cpp"
        "src/os/eeiop/adpcm/ea_hiso.cpp"
        "src/os/eeiop/adpcm/ea_magatoki.cpp"
        "src/os/eeiop/adpcm/ea_map.cpp"
        "src/os/eeiop/adpcm/ea_puzzle.cpp"
        "src/os/eeiop/adpcm/ea_scene.cpp"
        "src/os/eeiop/adpcm/ea_shinkan.cpp"
        "src/os/eeiop/adpcm/ea_soul.cpp"
        "src/os/eeiop/adpcm/ea_tape.cpp"
        "src/os/eeiop/cdvd/eecdvd.cpp"
        "src/os/eeiop/eeiop.cpp"
        "src/os/eeiop/eese.cpp"
        "src/os/eeiop/sd_room.cpp"
        "src/os/eeiop/se_cmd.cpp"
        "src/os/eeiop/se_data.cpp"
        "src/os/eeiop/se_door.cpp"
        "src/os/eeiop/se_ev.cpp"
        "src/os/eeiop/se_event.cpp"
        "src/os/eeiop/se_foot.cpp"
        "src/os/eeiop/se_plyr.cpp"
        "src/os/eeiop/se_srund.cpp"
        "src/os/eeiop/se_trans.cpp"
        "src/os/backup.cpp"
        "src/os/fileload.cpp"
        "src/os/key_cnf.cpp"
        "src/os/pad.cpp"
        "src/os/system.cpp"
        "src/outgame/btl_mode/btl_menu.cpp"
        "src/outgame/btl_mode/btl_mode.cpp"
        "src/outgame/btl_mode/btl_dat.cpp"
        "src/outgame/memory_album.cpp"
        "src/outgame/mode_slct.cpp"
        "src/outgame/mot_menu.cpp"
        "src/outgame/mot_test.cpp"
        "src/outgame/outgame.cpp"
        "src/outgame/scn_test.cpp"
        "src/outgame/snd_test.cpp"
        "src/outgame/sound_test.cpp"
        "src/outgame/title.cpp"
        "sdk/sce/libvu0.cpp"
        "sdk/sce/sifrpc.cpp"
        "sdk/ee/kernel.cpp"
        "sdk/sce/libgraph.cpp"
        "sdk/sce/sifdev.cpp"
        "sdk/sce/libdma.cpp"
        "sdk/sce/misc/diei.cpp"
        "sdk/sce/libsdr.cpp"
        "sdk/sce/libsd.cpp"
        "sdk/ee/eekernel.cpp"
        "sdk/sce/libgifpk.cpp"
        "sdk/sce/libcdvd.cpp"
        "sdk/sce/devvif0.cpp"
        "sdk/sce/devvif1.cpp"
        "sdk/sce/libpc.cpp"
        "sdk/sce/libpad.cpp"
        "sdk/iop_host.cpp"
        "sdk/sce/sif.cpp"
        "sdk/sce/devvu0.cpp"
        "sdk/sce/libmc.cpp"
        "sdk/sce/mpeg/libmpeg.cpp"
        "src/graphics/scene/scene_dat.cpp"
        "src/mc/mc.cpp"
        "src/mc/mc_at.cpp"
        "src/graphics/graph3d/vu_program.cpp"
        "src/mikupan/mikupan_memory.cpp"
        "src/mikupan/av/mikupan_video_decoder.cpp"
        "src/mikupan/av/mikupan_video_decoder_c.cpp"
        "src/mikupan/av/mikupan_audio_decoder.h"
        "src/mikupan/av/mikupan_audio_decoder.cpp"
        "src/mikupan/av/mikupan_audio_decoder_c.h"
        src/mikupan/av/mikupan_audio_decoder_c.cpp
        src/mikupan/ui/mikupan_ui.cpp
        src/mikupan/ui/mikupan_ui_c.cpp
        src/mikupan/rendering/mikupan_profiler.cpp
        src/mikupan/rendering/mikupan_gpu.cpp
        src/mikupan/rendering/mikupan_camera.cpp
        src/mikupan/rendering/mikupan_debug_draw.cpp
        src/mikupan/rendering/mikupan_lighting.cpp
        src/mikupan/rendering/mikupan_mesh_render.cpp
        src/mikupan/rendering/mikupan_renderer.cpp
        src/mikupan/rendering/mikupan_shadow.cpp
        src/mikupan/rendering/mikupan_shader.cpp
        src/mikupan/rendering/mikupan_sprite.cpp
        src/mikupan/rendering/mikupan_texture.cpp
        src/mikupan/gs/mikupan_gs.cpp
        src/mikupan/gs/mikupan_texture_manager.cpp
        src/mikupan/mikupan_logging.cpp
        src/graphics/graph3d/libsg.cpp
        src/ingame/map/find_dat.cpp
        src/mikupan/rendering/mikupan_pipeline.cpp
        src/mikupan/rendering/mikupan_meshcache.cpp
        src/mikupan/mikupan_graph3d_compat.cpp
        src/mikupan/mikupan_effect_compat.cpp
        src/mikupan/mikupan_photo_capture.cpp
        "src/mikupan/mikupan_utils.cpp"
        src/ingame/map/furn_spe/fsla_dat.cpp
        src/mikupan/mikupan_config.cpp
        src/mikupan/mikupan_config_ini.cpp
        src/mikupan/mikupan_config.h
        src/mikupan/mikupan_controller.cpp
        src/mikupan/mikupan_controller.h
        src/mikupan/mikupan_screenshot.cpp
        src/mikupan/mikupan_screenshot.h
        src/mikupan/mikupan_rng.cpp
        src/mikupan/mikupan_rng.h
        src/mikupan/ui/mikupan_framegraph.cpp
        src/mikupan/ui/mikupan_framegraph.h
        src/mikupan/ui/mikupan_ui_cheats.cpp
        src/mikupan/ui/mikupan_ui_cheats.h
        src/mikupan/ui/mikupan_ui_debug.cpp
        src/mikupan/ui/mikupan_ui_debug.h
        src/mikupan/ui/mikupan_ui_settings.cpp
        src/mikupan/ui/mikupan_ui_settings.h
        src/mikupan/ui/mikupan_ui_theme.cpp
        src/mikupan/ui/mikupan_ui_theme.h
)

# The legacy game + SDK code (originally .c, now renamed to .cpp) is decompiled C
# compiled as C++, so it needs the same C-ism relaxations on every compiler. The handful
# of files below were authored as conforming C++ and are kept OUT of those relaxations.
set(MIKUPAN_NATIVE_CXX_SOURCES
        "src/mikupan/av/mikupan_audio_decoder.cpp"
        "src/mikupan/av/mikupan_audio_decoder_c.cpp"
        "src/mikupan/av/mikupan_video_decoder.cpp"
        "src/mikupan/av/mikupan_video_decoder_c.cpp"
        "src/mikupan/gs/mikupan_gs.cpp"
        "src/mikupan/gs/mikupan_texture_manager.cpp"
        "src/mikupan/mikupan_config_ini.cpp"
        "src/mikupan/mikupan_file.cpp"
        "src/mikupan/mikupan_logging.cpp"
        "src/mikupan/ui/mikupan_ui.cpp"
)
set(MIKUPAN_CXX_SOURCES)
foreach(mikupan_source IN LISTS MIKUPAN_SOURCES)
    if(mikupan_source MATCHES "^(src|sdk)/.*\\.cpp$"
            AND NOT mikupan_source IN_LIST MIKUPAN_NATIVE_CXX_SOURCES)
        list(APPEND MIKUPAN_CXX_SOURCES "${mikupan_source}")
    endif()
endforeach()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${MIKUPAN_CXX_SOURCES} PROPERTIES
            COMPILE_OPTIONS "-fpermissive;-Wno-narrowing;-Wno-write-strings")
elseif(MSVC)
    # /permissive eases braced narrowing etc.; /Zc:strictStrings- re-allows
    # string-literal -> char* (hundreds of fields in src/data/*.h struct tables).
    # NOTE: void* -> T* has no MSVC flag — those are cast in code.
    set_source_files_properties(${MIKUPAN_CXX_SOURCES} PROPERTIES
            COMPILE_OPTIONS "/permissive;/Zc:strictStrings-")
endif()

# --- Build version info (git tag + commit) -------------------------------------
set(MIKUPAN_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
set(MIKUPAN_VERSION_HEADER ${MIKUPAN_GENERATED_DIR}/mikupan_version.h)
file(MAKE_DIRECTORY ${MIKUPAN_GENERATED_DIR})

# Generate once at configure time so the header always exists for compilation.
execute_process(
        COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/cmake/mikupan_version.h.in
        -DDST=${MIKUPAN_VERSION_HEADER}
        -DGIT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake
)

# Regenerate before every build so it reflects the actual built commit even when
# CMake isn't re-run. configure_file inside the script no-ops if nothing changed.
add_custom_target(mikupan_version
        COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/cmake/mikupan_version.h.in
        -DDST=${MIKUPAN_VERSION_HEADER}
        -DGIT_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake
        BYPRODUCTS ${MIKUPAN_VERSION_HEADER}
        COMMENT "Updating MikuPan build version info"
)

if(ANDROID)
    add_library(MikuPan SHARED ${MIKUPAN_SOURCES})
    set_target_properties(MikuPan PROPERTIES OUTPUT_NAME main)
else()
    add_executable(MikuPan ${MIKUPAN_SOURCES} ${APP_ICON_RESOURCE_WINDOWS})
endif()
add_dependencies(MikuPan mikupan_version)
add_dependencies(MikuPan compile_shaders)
if(TARGET ffmpeg_build)
    add_dependencies(MikuPan ffmpeg_build)
endif()

target_include_directories(MikuPan PRIVATE
        "sdk/"
        "src"
        "src/iop"
        "extern/"
        "extern/glad/include/"
        ${MIKUPAN_GENERATED_DIR}
        ${SDL3_INCLUDE_DIR}
        "${FFMPEG_ROOT}/include"
)

target_link_libraries(MikuPan PRIVATE
        cimgui
        SDL3::SDL3
        spdlog::spdlog
        cglm
        avcodec
        swscale
        avutil
        swresample
        avformat
        xxHash::xxhash
        inipp::inipp
)

if(NOT MSVC)
    target_link_libraries(MikuPan PRIVATE m)
endif()

if(ANDROID)
    target_link_libraries(MikuPan PRIVATE log)
endif()

if(NOT ANDROID)
    add_custom_command(TARGET MikuPan POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:MikuPan>/resources
    )
    if(WIN32)
        add_custom_command(TARGET MikuPan POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FFMPEG_ROOT}/bin"
                $<TARGET_FILE_DIR:MikuPan>
        )
    endif()
endif()

include(mikupan_android)
