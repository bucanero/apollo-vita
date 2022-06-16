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

set(VITA_APP_NAME "Apollo Save Tool")
set(VITA_TITLEID  "NP0APOLLO")
set(VITA_VERSION  "00.50")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -DAPOLLO_ENABLE_LOGGING")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(
  include
  ${VITASDK}/arm-vita-eabi/include/freetype2
)

link_directories(
  libs
)

add_executable(${PROJECT_NAME}
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
)

target_link_libraries(${PROJECT_NAME}
  SDL2::SDL2
  SceHttp_stub
  SceSsl_stub
  SceAppUtil_stub
  SceSqlite_stub
  SceRegistryMgr_stub
  SceVshBridge_stub
  SceNotificationUtil_stub
  VitaShellUser_stub_weak
  taihen_stub
  polarssl
  dbglogger
  apollo
  png
  freetype
  pthread
  unrar
  mzip
  un7zip
  stdc++
  c
  c
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
)
