#This file generates table/strings.h and all languages files (*.lng) using strgen

#strgen doesn't like paths with / on Windows, so we convert all paths to the native separator (\ on Windows)
file(TO_NATIVE_PATH "${LANG_DIR}" LANG_DIR_NATIVE)
file(TO_NATIVE_PATH "${OBJS_DIR}/lang/table" OBJS_LANG_TABLE_NATIVE)

add_custom_command(OUTPUT ${OBJS_DIR}/lang/table/strings.h
				COMMAND ${CMAKE_COMMAND} -E make_directory ${OBJS_DIR}/lang/table
				COMMAND strgen -s ${LANG_DIR_NATIVE} -d ${OBJS_LANG_TABLE_NATIVE}
				DEPENDS strgen
				DEPENDS ${LANG_DIR}/english.txt
				COMMENT "Generating table/strings.h : strgen -s ${LANG_DIR} -d table")
set_source_files_properties(${PROJECT_SOURCE_DIR}/objs/langs/table/strings.h PROPERTIES GENERATED 1)
#It is important to add a custom target to generate table/strings.h otherwise during a parallel build the above custom command
#will be called several times in parallel and overwrite each other's temporrary files (same problem below)
add_custom_target(gen_strings_h
					DEPENDS ${OBJS_DIR}/lang/table/strings.h)



file(TO_NATIVE_PATH "${OBJS_DIR}/lang" OBJS_LANG_DIR_NATIVE)
file(GLOB LANG_SRC_FILES ${LANG_DIR}/*.txt)

foreach(LANG_SRC ${LANG_SRC_FILES})
	#We build in LANG_LNG the path of the .lng file created by strgen
	get_filename_component(LANG_LNG ${LANG_SRC} NAME)
	string(REPLACE ".txt" ".lng" LANG_LNG ${LANG_LNG})
	list(APPEND LANG_LNG_FILES ${OBJS_DIR}/lang/${LANG_LNG})
	
	file(TO_NATIVE_PATH "${LANG_SRC}" LANG_SRC_NATIVE)
	
	#We make a bespoke working directory for each of the strgen commands, otherwise during parallel build they overwrite each
	#others tmp.xxx files 
	#See src/strgen/strgen.cpp line 290
	get_filename_component(LANG_SHORT ${LANG_SRC}  NAME)
	file(MAKE_DIRECTORY ${OBJS_DIR}/lang/temp/${LANG_SHORT})
	add_custom_command(OUTPUT ${OBJS_DIR}/lang/${LANG_LNG}
					COMMAND strgen -s ${LANG_DIR_NATIVE} -d ${OBJS_LANG_DIR_NATIVE} ${LANG_SRC_NATIVE}
					WORKING_DIRECTORY ${OBJS_DIR}/lang/temp/${LANG_SHORT}
					DEPENDS strgen gen_strings_h
					DEPENDS ${LANG_SRC}
					COMMENT "Building language : strgen -s ${LANG_DIR_NATIVE} -d ${OBJS_LANG_DIR_NATIVE} ${LANG_SRC_NATIVE}")
endforeach()


add_custom_target(gen_strings
				DEPENDS ${LANG_LNG_FILES})


set_source_files_properties(${PROJECT_SOURCE_DIR}/objs/langs/table/strings.h
							${PROJECT_SOURCE_DIR}/src/table/strings.h
							 PROPERTIES GENERATED 1)