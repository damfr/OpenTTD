#Building strgen, openttd's generator for languages files
cmake_minimum_required (VERSION 3.0)

set(SRC_DIR "..")
set(LANG_DIR ${SRC_DIR}/lang)

#Sources for strgen
list(APPEND SRC 
	${SRC_DIR}/strgen/strgen_base.cpp
	${SRC_DIR}/strgen/strgen.h
	${SRC_DIR}/table/control_codes.h
	${SRC_DIR}/table/strgen_tables.h
	${SRC_DIR}/safeguards.h
	${SRC_DIR}/strgen/strgen.cpp
	${SRC_DIR}/string.cpp
	${SRC_DIR}/misc/getoptdata.cpp
	${SRC_DIR}/misc/getoptdata.h
	${SRC_DIR}/safeguards.h
	${SRC_DIR}/core/alloc_func.cpp)


add_executable(strgen ${SRC})
target_compile_definitions(strgen PRIVATE STRGEN)
