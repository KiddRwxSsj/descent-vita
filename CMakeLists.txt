cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(descent C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Descent ディセント")
set(VITA_TITLEID  "DSCT00001")
set(VITA_VERSION  "01.50")

# The tree came from DOS, so every header is upper case on disk while the code
# includes them in lower case. Mirror them into one dir with lowercase links.
set(INC_SHIM ${CMAKE_BINARY_DIR}/inc)
file(MAKE_DIRECTORY ${INC_SHIM})
foreach(dir LIB INCLUDES MISC FIX CFILE 2D IFF DIV MEM VECMAT 3D TEXMAP BIOS MAIN VITA)
  file(GLOB hdrs ${CMAKE_SOURCE_DIR}/${dir}/*.H ${CMAKE_SOURCE_DIR}/${dir}/*.h)
  foreach(h ${hdrs})
    get_filename_component(n ${h} NAME)
    string(TOLOWER ${n} ln)
    file(CREATE_LINK ${h} ${INC_SHIM}/${ln} SYMBOLIC)
  endforeach()
endforeach()
file(MAKE_DIRECTORY ${INC_SHIM}/editor)
file(GLOB hdrs ${CMAKE_SOURCE_DIR}/MAIN/EDITOR/*.H)
foreach(h ${hdrs})
  get_filename_component(n ${h} NAME)
  string(TOLOWER ${n} ln)
  file(CREATE_LINK ${h} ${INC_SHIM}/editor/${ln} SYMBOLIC)
endforeach()

set(LIB_SRCS
  MISC/ERROR.C
  CFILE/CFILE.C
  IFF/IFF.C
  MEM/MEM.C
  FIX/FIX.C
  VECMAT/VECMAT.C
  DIV/DIV0.C
  2D/CANVAS.C 2D/BITMAP.C 2D/FONT.C 2D/BITBLT.C 2D/SCANLINE.C 2D/PIXEL.C
  2D/POLY.C 2D/DISC.C 2D/RECT.C 2D/BOX.C 2D/GPIXEL.C 2D/PALETTE.C 2D/LINE.C
  2D/SCALE.C 2D/CIRCLE.C 2D/IBITBLT.C 2D/RLE.C 2D/PCX.C
  2D/LINEAR.C 2D/LBITBLT.C 2D/SCALEA.C
  3D/GLOBVARS.C 3D/SETUP.C 3D/MATRIX.C 3D/POINTS.C 3D/DRAW.C 3D/CLIPPER.C
  3D/HORIZON.C 3D/INSTANCE.C 3D/ROD.C 3D/INTERP.C
  TEXMAP/NTMAP.C TEXMAP/TMAPFLAT.C TEXMAP/TMAPLOOP.C
)

set(GAME_SRCS
  MAIN/INFERNO.C MAIN/GAME.C MAIN/GAUGES.C MAIN/MGLOBAL.C MAIN/SCORES.C
  MAIN/OBJECT.C MAIN/LASER.C MAIN/PHYSICS.C MAIN/BM.C MAIN/MENU.C
  MAIN/GAMESAVE.C MAIN/SWITCH.C MAIN/EFFECTS.C MAIN/TEXMERGE.C MAIN/POLYOBJ.C
  MAIN/GAMEMINE.C MAIN/FIREBALL.C MAIN/AI.C MAIN/AIPATH.C MAIN/HOSTAGE.C
  MAIN/POWERUP.C MAIN/FUELCEN.C MAIN/RENDER.C MAIN/HASH.C
  MAIN/PIGGY.C MAIN/ARGS.C MAIN/GAMESEG.C MAIN/WALL.C MAIN/COLLIDE.C
  MAIN/LIGHTING.C MAIN/ROBOT.C MAIN/MORPH.C MAIN/VCLIP.C MAIN/WEAPON.C
  MAIN/FVI.C MAIN/NEWDEMO.C MAIN/TITLES.C MAIN/VFX.C MAIN/GAMESEQ.C
  MAIN/CONTROLS.C MAIN/AUTOMAP.C MAIN/TEXT.C MAIN/NEWMENU.C
  MAIN/VICTOR.C MAIN/GAMEFONT.C MAIN/JOYDEFS.C MAIN/HUD.C
  MAIN/PLAYSAVE.C MAIN/ENDLEVEL.C MAIN/TERRAIN.C MAIN/KCONFIG.C
  MAIN/CNTRLCEN.C MAIN/CREDITS.C MAIN/CONFIG.C
  MAIN/PAGING.C MAIN/MISSION.C MAIN/IGLASSES.C MAIN/SONGS.C MAIN/BMREAD.C
  MAIN/STATE.C MAIN/SLEW.C MAIN/DUMPMINE.C MAIN/NOCOMLIB.C
)

set(NET_SRCS
  MAIN/NETWORK.C MAIN/NETMISC.C MAIN/MODEM.C MAIN/MULTI.C MAIN/KMATRIX.C
  MAIN/MULTIBOT.C
)

set(VITA_SRCS
  VITA/VITAMAIN.C
  VITA/VIDEO.C
  VITA/IME.C
  VITA/MUSIC.C
  VITA/KEY.C
  VITA/MOUSE.C
  VITA/JOY.C
  VITA/TIMER.C
  VITA/SOUND.C
  VITA/DIGI.C
  VITA/DPMI.C
  VITA/DOSCOMPAT.C
  VITA/STUBS.C
)

set_source_files_properties(${LIB_SRCS} ${GAME_SRCS} ${NET_SRCS} ${VITA_SRCS} PROPERTIES LANGUAGE C)

# SHAREWARE changes too much of the game to flip at runtime, so the vpk carries
# both builds and the full one hands over to the shareware one when it sees that data
add_executable(descent ${LIB_SRCS} ${GAME_SRCS} ${NET_SRCS} ${VITA_SRCS})
target_compile_definitions(descent PRIVATE NETWORK)

add_executable(descent_sw ${LIB_SRCS} ${GAME_SRCS} ${VITA_SRCS})
# no IPX on the vita and the shareware net code never caught up with 1.5
target_compile_definitions(descent_sw PRIVATE SHAREWARE NO_NETWORK)

foreach(t descent descent_sw)
  target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/VITA/COMPAT ${INC_SHIM})

  target_compile_definitions(${t} PRIVATE RELEASE NDEBUG __VITA__)

  target_compile_options(${t} PRIVATE
    -x c -std=gnu11 -fpermissive -O2
    # Watcom's char is unsigned and the whole tree leans on it (fonts, key tables, text)
    -funsigned-char
    -fno-strict-aliasing -fwrapv -fno-common
    -include ${CMAKE_SOURCE_DIR}/VITA/COMPAT/prelude.h
    -Wno-implicit-int -Wno-implicit-function-declaration -Wno-int-conversion
    -Wno-incompatible-pointer-types -Wno-return-mismatch -Wno-unknown-pragmas
    -Wno-pointer-sign -Wno-builtin-declaration-mismatch -Wno-multichar
    -Wno-address-of-packed-member
  )

  target_link_libraries(${t}
    vita2d
    m
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    SceTouch_stub
    SceAudio_stub
    ScePower_stub
    SceAppMgr_stub
    SceAppUtil_stub
    SceCommonDialog_stub
    SceSysmodule_stub
    pthread
  )
endforeach()

vita_create_self(eboot.bin descent)
vita_create_self(shareware.bin descent_sw)
vita_create_vpk(descent.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_SOURCE_DIR}/VITA/sce_sys sce_sys
  FILE ${CMAKE_BINARY_DIR}/shareware.bin shareware.bin
)
