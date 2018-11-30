cmake_minimum_required (VERSION 3.0)

project(OpenTTD C CXX)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(LANG_DIR ${SRC_DIR}/lang)
set(OBJS_DIR ${CMAKE_SOURCE_DIR}/objs)

#Setting OS variable
include("cmake/os.cmake")

#Setting compiler-specific global flags (also apply to strgen and settingsgen)
include("cmake/compiler_flags.cmake")



#Adding sources for openttd to SOURCES_LIST
macro(add_source NAME) #Additionnal parameter COMPILE_FLAGS
	list(APPEND SOURCES_LIST src/${NAME})
	if(${ARGC} GREATER_EQUAL 2)
		message(STATUS "Adding compile flag ${ARGV1} to src/${NAME}")
		set_property(SOURCE src/${NAME} APPEND PROPERTY COMPILE_FLAGS ${ARGV1})
	endif()
endmacro()

include(source.list)


#Adding string generation
add_subdirectory(src/strgen)

#Adding languages building
include(cmake/strings.cmake)

#Adding settings generation
add_subdirectory(src/settingsgen)
set_source_files_properties(${PROJECT_SOURCE_DIR}/objs/setting/table/settings.h PROPERTIES GENERATED 1)

#Adding rev.cpp file configuration
include(cmake/revision.cmake)
#We configure rev.cpp at configure time for convenience (IDEs won't complain about missing definitions) but it will be reconfigured at build time
configure_revision_file(${CMAKE_SOURCE_DIR})
add_custom_target(revision ALL
				COMMAND ${CMAKE_COMMAND} -D REVISION_FILE=${CMAKE_SOURCE_DIR}/cmake/revision.cmake -D SOURCE_ROOT_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/revision_compile_time.cmake)


#Adding openttd build target
add_executable(openttd ${SOURCES_LIST})
#Set the location where the openttd executable will be created
#First in the case of single-configuration build systems (make, ...)
set_target_properties(openttd PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
#Then for each configuration for multi-configuration build systems (Visual Studio, XCode)
#to avoid these systems appending Debug or Release to the output path
foreach(OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUT_CONFIG} OUTPUT_CONFIG)
    set_target_properties(openttd PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUT_CONFIG} ${CMAKE_SOURCE_DIR}/bin)
endforeach()
add_dependencies(openttd gen_settings gen_strings_h gen_strings revision)
target_include_directories(openttd PRIVATE ${OBJS_DIR}/lang ${OBJS_DIR}/setting ${SRC_DIR}/3rdparty/squirrel/include)
