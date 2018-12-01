#This file sets various variables related to installation of openttd
#Global data dir
set(GLOBAL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share/games/openttd CACHE STRING "Global data dir")
target_compile_definitions(openttd PRIVATE GLOBAL_DATA_DIR=\"${GLOBAL_DATA_DIR}\")
