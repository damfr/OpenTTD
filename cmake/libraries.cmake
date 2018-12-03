#This file adds library dependencies to openttd
#Add our own modules to the cmake path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/libraries)

if(OS_MINGW OR OS_CYGWIN)
	#On MinGW, installing libraries via mingw-get installs headers into C:\MinGW\include (in case of the default install path)
	#This causes the FindXxx.cmake package finders to add to INTERFACE_INCLUDE_DIRECTORIES C:\MinGW\include (or equivalent)
	#CMake then adds -isystem C:\MinGW\include to the compile options
	#This prevents gcc from finding cstdlib (error message : fatal error : stdlib.h : No such file or directory)
	#Adding -I C:\MinGW\include to the compile options does not cause this problem for some reason
	#So we tell CMake to use -I instead of -isystem when adding imported include directories from targets
	set_property(TARGET openttd PROPERTY NO_SYSTEM_FROM_IMPORTED "ON")
endif()

function(create_static_option PACKAGE_NAME)
	string(TOUPPER ${PACKAGE_NAME} PACKAGE_NAME_UPPER)
	if(OS_WIN32 OR OS_MINGW OR OS_CYGWIN)
		#We are on Windows, we cannot differentiate between shared and static libs by their extension
		set(DESCRIPTION "Link ${PACKAGE_NAME} statically (Beware, CMake will not warn you if this setting does not match the library found (static or shared) : linking will most likely fail in this case)")
	else()
		set(DESCRIPTION "Link ${PACKAGE_NAME} statically")
	endif()
	set(${PACKAGE_NAME_UPPER}_STATIC "INHERIT_FROM_ENABLE_STATIC" CACHE STRING ${DESCRIPTION})
	set_property(CACHE ${PACKAGE_NAME_UPPER}_STATIC PROPERTY STRINGS "INHERIT_FROM_ENABLE_STATIC" "FORCE_STATIC" "FORCE_SHARED")
endfunction()

#It tries pkg-config in priority
#PACKAGE_NAME : package_name for pkg-config (ex:liblzma)
#INCLUDE_FILES : the include files needed for the library (ex:lzma.h) (for CMake find_path)
#LIBRARY_NAME : library name (ex:lzma) (for CMake find_library)
#VERSION_STRING : string to append to PACKAGE_NAME to specify version (ex: >=1.2)
function(find_library_static_or_shared PACKAGE_NAME TARGET_NAME INCLUDE_FILES LIBRARY_NAME VERSION_STRING)
	set(INCLUDE_DIRS_COMMENT "${PACKAGE_NAME} include dirs (where ${INCLUDE_FILES} is/are located [guessed]")
	set(${PACKAGE_NAME_UPPER}_INCLUDE_DIRS "" CACHE PATH ${INCLUDE_DIRS_COMMENT})
	set(LIBRARIES_COMMENT "${PACKAGE_NAME} library(ies) file(s) (where (lib)${LIBRARY_NAME}.so/.a/.lib is located) [guessed]")
	set(${PACKAGE_NAME_UPPER}_LINK_LIBRARIES "" CACHE PATH ${LIBRARIES_COMMENT})
	
	
	string(TOUPPER ${PACKAGE_NAME} PACKAGE_NAME_UPPER)
	
	if(PACKAGE_NAME_UPPER_STATIC STREQUAL "FORCE_STATIC" OR (PACKAGE_NAME_UPPER STREQUAL "INHERIT_FROM_ENABLE_STATIC" AND ENABLE_STATIC))
		set(LINK_STATIC "ON")
	else()
		set(LINK_STATIC "OFF")
	endif()
	
	include(FindPkgConfig)
	#If found, we use pkg-config in priority, except when a library was already set (either by the user or from a previous configuration)
	if(PKG_CONFIG_FOUND AND (NOT ${PACKAGE_NAME_UPPER}_LINK_LIBRARIES)
		pkg_check_modules(${PACKAGE_NAME_UPPER} ${PACKAGE_NAME}${VERSION_STRING})
		if(LINK_STATIC)
			set(STATIC_SUFFIX "_STATIC")
		endif()
		if(${PACKAGE_NAME_UPPER}${STATIC_SUFFIX}_FOUND)
			add_library(${TARGET_NAME} UNKNOWN IMPORTED)
			
			set_target_properties(${TARGET_NAME} PROPERTIES
								INTERFACE_INCLUDE_DIRECTORIES ${${PACKAGE_NAME_UPPER}${STATIC_SUFFIX}_INCLUDE_DIRS}
								INTERFACE_LINK_LIBRARIES ${${PACKAGE_NAME_UPPER}${STATIC_SUFFIX}_LINK_LIBRARIES}
								INTERFACE_COMPILE_OPTIONS ${${PACKAGE_NAME_UPPER}${STATIC_SUFFIX}_CFLAGS})
			set(${PACKAGE_NAME_UPPER}_FOUND "ON" PARENT_SCOPE)
			return()
		endif()
		#message("${PACKAGE_NAME} could not be found using pkg-config, attempting CMake built-in search")
	endif()
	find_path(${PACKAGE_NAME_UPPER}_INCLUDE_DIRS NAMES ${INCLUDE_FILES} DOC ${INCLUDE_DIRS_COMMENT})
	
	if(LINK_STATIC)
		message("You chose to link ${PACKAGE_NAME} statically, but pkg-config was not found. You may have to add dependent libraries of ${PACKAGE_NAME} to ${PACKAGE_NAME_UPPER}_LINK_LIBRARIES manually")
		#Choosing the appropriate suffix for a static library
		#Windows causes problems as the same suffix is used in both cases
		if(OS_WIN32 OR OS_MINGW OR OS_CYGWIN)
			set(LIB_SUFFIX ".lib")
		else()
			set(LIB_SUFFIX ".a")
		endif()
	else()
		#Choosing the appropriate suffix for a shared library
		if(OS_WIN32 OR OS_MINGW OR OS_CYGWIN)
			set(LIB_SUFFIX ".lib")
		else()
			set(LIB_SUFFIX ".so")
		endif()
	endif()
	find_library(${PACKAGE_NAME_UPPER}_LINK_LIBRARIES NAMES ${LIBRARY_NAME}${LIB_SUFFIX} DOC ${LIBRARIES_COMMENT})
	
	if(${PACKAGE_NAME_UPPER}_LINK_LIBRARIES)
		add_library(${TARGET_NAME} UNKNOWN IMPORTED)
		
		set_target_properties(${TARGET_NAME} PROPERTIES
							INTERFACE_INCLUDE_DIRECTORIES ${${PACKAGE_NAME_UPPER}_INCLUDE_DIRS}
							INTERFACE_LINK_LIBRARIES ${${PACKAGE_NAME_UPPER}_LINK_LIBRARIES})
		set(${PACKAGE_NAME_UPPER}_FOUND "ON" PARENT_SCOPE)
	endif()

endfunction()

function(check_load_library PACKAGE_NAME TARGET_NAME OPTION_DESC)
	string(TOUPPER ${PACKAGE_NAME} PACKAGE_NAME_UPPER)
	if(WITH_${PACKAGE_NAME_UPPER} OR (WITH_${PACKAGE_NAME_UPPER} STREQUAL "AUTO_DETECT"))
		find_package(${PACKAGE_NAME})
		
		if (${PACKAGE_NAME_UPPER}_FOUND)
			message(STATUS "Using ${PACKAGE_NAME}")
			set(WITH_${PACKAGE_NAME_UPPER} "ON" CACHE BOOL ${OPTION_DESC} FORCE)
			target_link_libraries(openttd PRIVATE ${TARGET_NAME})
			target_compile_definitions(openttd PRIVATE -DWITH_${PACKAGE_NAME_UPPER})
			
		elseif((NOT ${PACKAGE_NAME_UPPER}_FOUND) AND (WITH_${PACKAGE_NAME_UPPER} STREQUAL "AUTO_DETECT"))
			message(STATUS "${PACKAGE_NAME} not found")
			set(WITH_${PACKAGE_NAME_UPPER} "OFF" CACHE BOOL ${OPTION_DESC} FORCE)
			
		else()
			message(SEND_ERROR "${PACKAGE_NAME} was not found")
		endif()
	endif()
endfunction(check_load_library)

if (EXISTS ${OPENTTD_USEFUL_PATH})
	message("Found openttd-useful")
	set(WINDOWS_ARCHITECTURE 64)
	if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86")
		set(WINDOWS_ARCHITECTURE 32)
	endif()
	
	list(APPEND CMAKE_LIBRARY_PATH ${OPENTTD_USEFUL_PATH}/win${WINDOWS_ARCHITECTURE}/library)
	list(APPEND CMAKE_INCLUDE_PATH ${OPENTTD_USEFUL_PATH}/shared/include
									${OPENTTD_USEFUL_PATH}/win${WINDOWS_ARCHITECTURE}/include)
	
	#CMake supplied FinZLIB script does not find zlibstat, so override it
	set(ZLIB_LIBRARY ${OPENTTD_USEFUL_PATH}/win${WINDOWS_ARCHITECTURE}/library/zlibstat.lib)
	message(${CMAKE_LIBRARY_PATH})
endif()

#SDL
set(WITH_SDL "AUTO_DETECT" CACHE STRING "Use SDL")
if (APPLE AND WITH_COCOA)
	set(WITH_SDL "OFF" CACHE STRING "Use SDL" FORCE)
endif()
set_property(CACHE WITH_SDL PROPERTY STRINGS "ON" "OFF" "AUTO_DETECT")

option(SDL_STATIC "Link SDL statically (only known to work on Windows, DOS, MacOSX and MorphOS)" OFF)
check_load_library(SDL SDL::SDL "Use SDL")

#lzo2
set(WITH_LZO2 "AUTO_DETECT" CACHE STRING "Use lzo2 (for (de)compressing of old (pre 0.3.0) savegames)")
set_property(CACHE WITH_LZO2 PROPERTY STRINGS "ON" "OFF" "AUTO_DETECT")
check_load_library(Lzo2 Lzo::Lzo2 "Use lzo2 (for (de)compressing of old (pre 0.3.0) savegames)")

#zlib
set(WITH_ZLIB "AUTO_DETECT" CACHE STRING "Use zlib (for (de)compressing of old (0.3.0-1.0.5) savegames, content downloads, heightmaps)")
set_property(CACHE WITH_ZLIB PROPERTY STRINGS "ON" "OFF" "AUTO_DETECT")
check_load_library(ZLIB ZLIB::ZLIB "Use zlib (for (de)compressing of old (0.3.0-1.0.5) savegames, content downloads, heightmaps)")

#lzma
set(WITH_LZMA "AUTO_DETECT" CACHE STRING "Use lzma (for (de)compressing of savegames (1.1.0 and later))")
set_property(CACHE WITH_LZMA PROPERTY STRINGS "ON" "OFF" "AUTO_DETECT")
check_load_library(Lzma Lzma::Lzma "Use lzma (for (de)compressing of savegames (1.1.0 and later))")
