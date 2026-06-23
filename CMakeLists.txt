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
set(CMAKE_CXX_STANDARD 17)
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
        "src/mikupan/mikupan_first_person.c"
        "src/common/ul_math.c"
        "src/common/utility.c"
        "src/edit/test_2d.c"
        "src/graphics/graph2d/compress.c"
        "src/graphics/graph2d/effect.c"
        "src/graphics/graph2d/effect_ene.c"
        "src/graphics/graph2d/effect_obj.c"
        "src/graphics/graph2d/effect_oth.c"
        "src/graphics/graph2d/effect_rdr.c"
        "src/graphics/graph2d/effect_scr.c"
        "src/graphics/graph2d/effect_sub.c"
        "src/graphics/graph2d/effect_sub2.c"
        "src/graphics/graph2d/g2d_debug.c"
        "src/graphics/graph2d/g2d_main.c"
        "src/graphics/graph2d/message.c"
        "src/graphics/graph2d/number.c"
        "src/graphics/graph2d/rare_ene.c"
        "src/graphics/graph2d/snapshot.c"
        "src/graphics/graph2d/subtitles.c"
        "src/graphics/graph2d/takep_pe.c"
        "src/graphics/graph2d/tim2.c"
        "src/graphics/graph2d/tim2_new.c"
        "src/graphics/graph3d/gra3d.c"
        "src/graphics/graph3d/leditor.c"
        "src/graphics/graph3d/load3d.c"
        "src/graphics/graph3d/mirror.c"
        "src/graphics/graph3d/object.c"
        "src/graphics/graph3d/sce_pp3.c"
        "src/graphics/graph3d/sgcam.c"
        "src/graphics/graph3d/sgdma.c"
        "src/graphics/graph3d/sglib.c"
        "src/graphics/graph3d/sglight.c"
        "src/graphics/graph3d/sgsgd.c"
        "src/graphics/graph3d/sgsu.c"
        "src/graphics/graph3d/sgsup.c"
        "src/graphics/graph3d/shadow.c"
        "src/graphics/motion/accessory.c"
        "src/graphics/motion/acs_dat.c"
        "src/graphics/motion/anicode.c"
        "src/graphics/motion/mdlact.c"
        "src/graphics/motion/mdldat.c"
        "src/graphics/motion/mdlwork.c"
        "src/graphics/motion/mime.c"
        "src/graphics/motion/mim_dat.c"
        "src/graphics/motion/motion.c"
        "src/graphics/mov/movie.c"
        "src/graphics/scene/fod.c"
        "src/graphics/scene/scene.c"
        "src/ingame/camera/camera.c"
        "src/ingame/enemy/ene_ctl.c"
        "src/ingame/enemy/move_ctl.c"
        "src/ingame/entry/ap_bgost.c"
        "src/ingame/entry/ap_dgost.c"
        "src/ingame/entry/ap_fgost.c"
        "src/ingame/entry/fgst_dat.c"
        "src/ingame/entry/ap_ggost.c"
        "src/ingame/entry/ap_pgost.c"
        "src/ingame/entry/ap_rgost.c"
        "src/ingame/entry/rgst_dat.c"
        "src/ingame/entry/ap_sgost.c"
        "src/ingame/entry/ap_zgost.c"
        "src/ingame/entry/entry.c"
        "src/ingame/event/ev_load.c"
        "src/ingame/event/ev_main.c"
        "src/ingame/event/ev_spcl.c"
        "src/ingame/event/mv_cmnd.c"
        "src/ingame/event/wan_dat.c"
        "src/ingame/event/wan_soul.c"
        "src/ingame/info/inf_disp.c"
        "src/ingame/map/furn_spe/adj_valg.c"
        "src/ingame/map/furn_spe/fsla_main.c"
        "src/ingame/map/furn_spe/fspe_acs.c"
        "src/ingame/map/furn_spe/furn_spe.c"
        "src/ingame/map/doortype.c"
        "src/ingame/map/door_conte.c"
        "src/ingame/map/door_ctl.c"
        "src/ingame/map/find_ctl.c"
        "src/ingame/map/furn_ctl.c"
        "src/ingame/map/furn_dat.c"
        "src/ingame/map/furn_eff.c"
        "src/ingame/map/item_ctl.c"
        "src/ingame/map/map_area.c"
        "src/ingame/map/map_ctrl.c"
        "src/ingame/map/map_htck.c"
        "src/ingame/map/rotd_ctl.c"
        "src/ingame/menu/gameover.c"
        "src/ingame/ig_glob.c"
        "src/ingame/menu/ig_album.c"
        "src/ingame/menu/ig_camra.c"
        "src/ingame/menu/ig_file.c"
        "src/ingame/menu/ig_glst.c"
        "src/ingame/menu/ig_map.c"
        "src/ingame/menu/ig_menu.c"
        "src/ingame/menu/ig_option.c"
        "src/ingame/menu/ig_rank.c"
        "src/ingame/menu/ig_spd_menu.c"
        "src/ingame/menu/item.c"
        "src/ingame/menu/item_get.c"
        "src/ingame/menu/pause.c"
        "src/ingame/menu/sp_menu.c"
        "src/ingame/photo/photo.c"
        "src/ingame/photo/pht_main.c"
        "src/ingame/photo/pht_make.c"
        "src/ingame/plyr/plyr_ctl.c"
        "src/ingame/plyr/time_ctl.c"
        "src/ingame/plyr/unit_ctl.c"
        "src/ingame/ig_init.c"
        "src/ingame/ingame.c"
        "src/iop/iopmain.c"
        "src/iop/adpcm/iopadpcm.c"
        "src/iop/adpcm/ia_cmd.c"
        "src/iop/adpcm/ia_func.c"
        "src/iop/se/iopse.c"
        "src/iop/se/sedat.c"
        "src/iop/se/voice.c"
        "src/iop/cdvd/iopcdvd.c"
        "src/main/gamemain.c"
        "src/main/glob.c"
        "src/main/main.c"
        "src/mc/mc_acs.c"
        "src/mc/mc_album.c"
        "src/mc/mc_at.c"
        "src/mc/mc_disp.c"
        "src/mc/mc_exec.c"
        "src/mc/mc_main.c"
        "src/mc/mc_start.c"
        "src/mc/mc_icon.c"
        "src/os/eeiop/adpcm/ea_autog.c"
        "src/os/eeiop/adpcm/ea_btlmode.c"
        "src/os/eeiop/adpcm/ea_cmd.c"
        "src/os/eeiop/adpcm/ea_ctrl.c"
        "src/os/eeiop/adpcm/ea_dat.c"
        "src/os/eeiop/adpcm/ea_event.c"
        "src/os/eeiop/adpcm/ea_furn.c"
        "src/os/eeiop/adpcm/ea_gameover.c"
        "src/os/eeiop/adpcm/ea_gdead.c"
        "src/os/eeiop/adpcm/ea_ghost.c"
        "src/os/eeiop/adpcm/ea_hiso.c"
        "src/os/eeiop/adpcm/ea_magatoki.c"
        "src/os/eeiop/adpcm/ea_map.c"
        "src/os/eeiop/adpcm/ea_puzzle.c"
        "src/os/eeiop/adpcm/ea_scene.c"
        "src/os/eeiop/adpcm/ea_shinkan.c"
        "src/os/eeiop/adpcm/ea_soul.c"
        "src/os/eeiop/adpcm/ea_tape.c"
        "src/os/eeiop/cdvd/eecdvd.c"
        "src/os/eeiop/eeiop.c"
        "src/os/eeiop/eese.c"
        "src/os/eeiop/sd_room.c"
        "src/os/eeiop/se_cmd.c"
        "src/os/eeiop/se_data.c"
        "src/os/eeiop/se_door.c"
        "src/os/eeiop/se_ev.c"
        "src/os/eeiop/se_event.c"
        "src/os/eeiop/se_foot.c"
        "src/os/eeiop/se_plyr.c"
        "src/os/eeiop/se_srund.c"
        "src/os/eeiop/se_trans.c"
        "src/os/backup.c"
        "src/os/fileload.c"
        "src/os/key_cnf.c"
        "src/os/pad.c"
        "src/os/system.c"
        "src/outgame/btl_mode/btl_menu.c"
        "src/outgame/btl_mode/btl_mode.c"
        "src/outgame/btl_mode/btl_dat.c"
        "src/outgame/memory_album.c"
        "src/outgame/mode_slct.c"
        "src/outgame/outgame.c"
        "src/outgame/scn_test.c"
        "src/outgame/sound_test.c"
        "src/outgame/title.c"
        "sdk/sce/libvu0.c"
        "sdk/sce/sifrpc.c"
        "sdk/ee/kernel.c"
        "sdk/sce/libgraph.c"
        "sdk/sce/sifdev.c"
        "sdk/sce/libdma.c"
        "sdk/sce/misc/diei.c"
        "sdk/sce/libsdr.c"
        "sdk/sce/libsd.c"
        "sdk/ee/eekernel.c"
        "sdk/sce/libgifpk.c"
        "sdk/sce/libcdvd.c"
        "sdk/sce/devvif0.c"
        "sdk/sce/devvif1.c"
        "sdk/sce/libpc.c"
        "sdk/sce/libpad.c"
        "sdk/iop_host.c"
        "sdk/sce/sif.c"
        "sdk/sce/devvu0.c"
        "sdk/sce/libmc.c"
        "sdk/sce/mpeg/libmpeg.c"
        "src/graphics/scene/scene_dat.c"
        "src/mc/mc.c"
        "src/mc/mc_at.c"
        "src/graphics/graph3d/vu_program.c"
        "src/mikupan/mikupan_memory.c"
        "src/mikupan/av/mikupan_video_decoder.cpp"
        "src/mikupan/av/mikupan_video_decoder_c.cpp"
        "src/mikupan/av/mikupan_audio_decoder.h"
        "src/mikupan/av/mikupan_audio_decoder.cpp"
        "src/mikupan/av/mikupan_audio_decoder_c.h"
        src/mikupan/av/mikupan_audio_decoder_c.cpp
        src/mikupan/ui/mikupan_ui.cpp
        src/mikupan/ui/mikupan_ui.c
        src/mikupan/rendering/mikupan_profiler.c
        src/mikupan/rendering/mikupan_gpu.c
        src/mikupan/rendering/mikupan_camera.c
        src/mikupan/rendering/mikupan_debug_draw.c
        src/mikupan/rendering/mikupan_lighting.c
        src/mikupan/rendering/mikupan_mesh_render.c
        src/mikupan/rendering/mikupan_renderer.c
        src/mikupan/rendering/mikupan_shadow.c
        src/mikupan/rendering/mikupan_shader.c
        src/mikupan/rendering/mikupan_sprite.c
        src/mikupan/rendering/mikupan_texture.c
        src/mikupan/gs/mikupan_gs.cpp
        src/mikupan/gs/mikupan_texture_manager.cpp
        src/mikupan/mikupan_logging.cpp
        src/graphics/graph3d/libsg.c
        src/ingame/map/find_dat.c
        src/mikupan/rendering/mikupan_pipeline.c
        src/mikupan/rendering/mikupan_meshcache.c
        "src/mikupan/mikupan_utils.c"
        src/ingame/map/furn_spe/fsla_dat.c
        src/mikupan/mikupan_config.c
        src/mikupan/mikupan_config_ini.cpp
        src/mikupan/mikupan_config.h
        src/mikupan/mikupan_controller.c
        src/mikupan/mikupan_controller.h
        src/mikupan/mikupan_screenshot.c
        src/mikupan/mikupan_screenshot.h
        src/mikupan/mikupan_rng.c
        src/mikupan/mikupan_rng.h
        src/mikupan/ui/mikupan_framegraph.c
        src/mikupan/ui/mikupan_framegraph.h
        src/mikupan/ui/mikupan_ui_cheats.c
        src/mikupan/ui/mikupan_ui_cheats.h
        src/mikupan/ui/mikupan_ui_debug.c
        src/mikupan/ui/mikupan_ui_debug.h
        src/mikupan/ui/mikupan_ui_settings.c
        src/mikupan/ui/mikupan_ui_settings.h
        src/mikupan/ui/mikupan_ui_theme.c
        src/mikupan/ui/mikupan_ui_theme.h
)

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
