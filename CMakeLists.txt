cmake_minimum_required (VERSION 3.0)

project(OpenTTD C CXX)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(LANG_DIR ${SRC_DIR}/lang)
set(OBJS_DIR ${CMAKE_SOURCE_DIR}/objs)


#Adding sources for openttd to SOURCES_LIST
macro(add_source NAME) #Additionnal parameter COMPILE_FLAGS
	list(APPEND SOURCES_LIST src/${NAME})
	if(${ARGC} GREATER_EQUAL 2)
		message(STATUS "Adding compile flag ${ARGV1} to src/${NAME}")
		set_property(SOURCE src/${NAME} APPEND PROPERTY COMPILE_FLAGS ${ARGV1})
	endif()
endmacro()

include(source.list)


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
