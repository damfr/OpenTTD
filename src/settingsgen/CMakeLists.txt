#This file generates sttingsgen, and then creaates a target to generates settings.h using settingsgen
cmake_minimum_required (VERSION 3.0)

set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)

list(APPEND SRC
${SRC_DIR}/settingsgen/settingsgen.cpp
${SRC_DIR}/string_func.h
${SRC_DIR}/strings_type.h
${SRC_DIR}/ini_type.h
${SRC_DIR}/ini_load.cpp
${SRC_DIR}/core/smallvec_type.hpp
${SRC_DIR}/core/alloc_func.cpp
${SRC_DIR}/core/mem_func.hpp
${SRC_DIR}/safeguards.h
${SRC_DIR}/misc/getoptdata.cpp
${SRC_DIR}/misc/getoptdata.h
${SRC_DIR}/string.cpp)


add_executable(settingsgen ${SRC})
target_compile_definitions(settingsgen PRIVATE SETTINGSGEN)


file(GLOB INI_SETTINGS_FILES ../table/*.ini)

add_custom_command(OUTPUT ${PROJECT_SOURCE_DIR}/objs/setting/table/settings.h
				COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/objs/setting/table
				COMMAND settingsgen -o ${PROJECT_SOURCE_DIR}/objs/setting/table/settings.h -b ${SRC_DIR}/table/settings.h.preamble -a ${SRC_DIR}/table/settings.h.postamble ${INI_SETTINGS_FILES}
				DEPENDS settingsgen
						${SRC_DIR}/table/settings.h.preamble ${SRC_DIR}/table/settings.h.preamble ${INI_SETTINGS_FILE}
				COMMENT "Generating table/settings.h")

add_custom_target(gen_settings
				DEPENDS ${PROJECT_SOURCE_DIR}/objs/setting/table/settings.h)

				