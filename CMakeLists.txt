cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(apollo-vita)
include("${VITASDK}/share/vita.cmake" REQUIRED)

find_package(SDL2 REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

set(VITA_APP_NAME "Apollo Save Tool")
set(VITA_TITLEID  "NP0APOLLO")
set(VITA_VERSION  "01.10")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(
  include
  ${VITASDK}/arm-vita-eabi/include/freetype2
)

link_directories(
  libs
)

# From vita shell CMakeLists - credits TheFlow
FUNCTION(ADD_RESOURCES out_var)
    SET(result)
    FOREACH(in_f ${ARGN})
        SET(out_f "${CMAKE_CURRENT_BINARY_DIR}/${in_f}.o")
        GET_FILENAME_COMPONENT(out_dir ${out_f} DIRECTORY)
        ADD_CUSTOM_COMMAND(OUTPUT ${out_f}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
                COMMAND ${CMAKE_LINKER} -r -b binary -o ${out_f} ${in_f}
                DEPENDS ${in_f}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Building resource ${out_f}"
                VERBATIM
                )
        LIST(APPEND result ${out_f})
    ENDFOREACH()
    SET(${out_var} "${result}" PARENT_SCOPE)
ENDFUNCTION()

file(GLOB res_files RELATIVE
  ${CMAKE_SOURCE_DIR}
  data/*.png
  data/*.jpg
  data/*.s3m
)
add_resources(apollo_res ${res_files})

add_executable(${PROJECT_NAME}
  ${apollo_res}
  source/common.c
  source/config.c
  source/dialog.c
  source/draw.c
  source/exec_cmd.c
  source/http.c
  source/libfont.c
  source/main.c
  source/menu_about.c
  source/menu_cheats.c
  source/menu_options.c
  source/saves.c
  source/settings.c
  source/sfo.c
  source/sqlite3.c
  source/ttf_fonts.c
  source/util.c
  source/zip_util.c
  source/kirk_engine.c
  source/psp_decrypter.c
  source/vmp_resign.c
)

target_link_libraries(${PROJECT_NAME}
  SDL2::SDL2
  ${CURL_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  SceAppUtil_stub
  SceSqlite_stub
  SceRegistryMgr_stub
  SceVshBridge_stub
  SceNotificationUtil_stub
  VitaShellUser_stub_weak
  taihen_stub
  apollo
  polarssl
  dbglogger
  freetype
  png
  pthread
  unrar
  mzip
  un7zip
  stdc++
  xmp-lite
  z
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE data/appdata.zip cache/appdata.zip
  FILE libs/user.suprx sce_module/user.suprx
  FILE libs/kernel.skprx sce_module/kernel.skprx
)

add_custom_target(createzip
  COMMAND @echo "Creating appdata.zip ..."
  COMMAND @rm -fr ${CMAKE_SOURCE_DIR}/data/appdata.zip
  COMMAND @cd ${CMAKE_SOURCE_DIR}/appdata && zip ../data/appdata.zip *.savepatch
)
