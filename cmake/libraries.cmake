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
