#This file sets the target OS cache variable
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(OS "UNIX" CACHE STRING "Target OS")
	set(OS_UNIX "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(OS "OSX" CACHE STRING "Target OS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if (CYGWIN)
		set(OS "CYGWIN" CACHE STRING "Target OS")
	elseif(MINGW)
		set(OS "MINGW" CACHE STRING "Target OS")
	else()
		#Warning WIN32 is set regardless for Windows 32bits as well as 64bits
		set(OS "WIN32" CACHE STRING "Target OS")
	endif()
else()
	message(WARNING "OS not recognized : ${CMAKE_SYSTEM}")
endif()

set(OS_${OS} "ON")


