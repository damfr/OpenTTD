#This file defines compile and link options to the openttd target
link_libraries(stdc++)

if(ENABLE_NETWORK)
	target_compile_definitions(openttd PRIVATE ENABLE_NETWORK)
	if (OS_BEOS)
		target_link_libraries(openttd PRIVATE bind socket)
	endif()
	if(OS_HAIKU)
		target_link_libraries(openttd PRIVATE network)
	endif()
	if (OS_SUNOS)
		target_link_libraries(openttd PRIVATE nsl socket)
	endif()
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	if (DEBUG_LEVEL EQUAL 0)
		if (OS_MORPHOS)
			target_include_directories(openttd PRIVATE "/gg/os-include")
			target_compile_options(openttd PRIVATE -noixemul -fstrict-aliasing -fexpensive-optimizations -mcpu=604 -fno-inline -mstring -mmultiple)
			set_property(TARGET openttd APPEND LINK_OPTIONS -noixemul)
		endif()
		
		if (NOT ENABLE_PROFILING)
			# -fomit-frame-pointer and -pg do not go well together (gcc errors they are incompatible)
			target_compile_options(openttd PRIVATE -fomit-frame-pointer)
		endif()
		
		target_compile_options(openttd PRIVATE -O2)
		
	else()
		target_compile_definitions(openttd PRIVATE _DEBUG)
		target_compile_options(openttd PRIVATE -g)
	endif()
	
	if(DEBUG_LEVEL GREATER_EQUAL 2)
		target_compile_options(openttd PRIVATE -fno-inline)
	endif()
	if (DEBUG_LEVEL EQUAL 3)
		target_compile_options(openttd PRIVATE -O0)
	else()
		target_compile_options(openttd PRIVATE -O2)
	endif()
	
	if (DEBUG_LEVEL LESS_EQUAL 2 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# Define only when compiling with GCC. Some GLIBC versions use GNU
		# extensions in a way that breaks build with at least ICC.
		# This requires -O1 or more, so debug level 3 (-O0) is excluded.
		target_compile_definitions(openttd PRIVATE _FORTIFY_SOURCE=2)
	endif()
	
	if(ENABLE_PROFILING)
		target_compile_options(openttd PRIVATE -pg)
		target_link_libraries(openttd PRIVATE -pg)
	endif()
endif()

if (NOT WITH_THREADS OR OS_DOS)
	target_compile_definitions(openttd PRIVATE NO_THREADS)
endif()

#Detecting SSE
#We do not try to detect it hwile cross-compiling, as try_run would not work as intended
if(NOT CMAKE_CROSSCOMPILING AND WITH_SSE)
	file(WRITE ${OBJS_DIR}/tmp.sse.cpp
"#define _SQ64 1
#include <xmmintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>
int main() { return 0; }")
	try_run(SSE_RUN_RESULT SSE_COMPILE_RESULT
			${OBJS_DIR} ${OBJS_DIR}/tmp.sse.cpp
			COMPILE_DEFINITIONS "-msse4.1")
	if(NOT ${RUN_RESULT_VAR} EQUAL 0)
		message(SEND_ERROR "The build was configured with SSE support, but SSE was not found (Compile result:${SSE_COMPILE_RESULT}, run result:${SSE_RUN_RESULT})")
	endif()
	
	file(REMOVE ${OBJS_DIR}/tmp.sse.cpp)
endif()
if(WITH_SSE)
	target_compile_definitions(openttd PRIVATE WITH_SSE)
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	if(OS_CYGWIN)
		target_compile_options(openttd PRIVATE -mwin32)
		target_link_libraries(openttd PRIVATE -mwin32)
	endif()
	if(OS_MINGW OR OS_CYGWIN)
		#GCC on Windows
		if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.6)
			target_compile_options(openttd PRIVATE -mno-cygwin)
			target_link_libraries(openttd PRIVATE -mno-cygwin)
		endif()
		
		if(ENABLE_CONSOLE)
			target_link_libraries(openttd PRIVATE -Wl,--subsystem,console)
		else()
			target_link_libraries(openttd PRIVATE -Wl,--subsystem,windows)
		endif()
		
		target_link_libraries(openttd PRIVATE ws2_32 winmm gdi32 dxguid ole32 imm32)

		if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4)
			target_link_libraries(openttd PRIVATE -static-libgcc -static-libstdc++)
		endif()
		if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.7)
			target_compile_options(openttd PRIVATE -mno-ms-bitfields)
		endif()
	endif()
endif()

if(NOT OS_WIN32)
	if((NOT OS_CYGWIN) AND (NOT OS_HAIKU) AND (NOT OS_OPENBSD) AND (NOT OS_MINGW) AND (NOT OS_MORPHOS) AND (NOT OS_OSX) AND (NOT OS_DOS) AND (NOT OS_OS2))
		target_link_libraries(openttd PRIVATE pthread)
	endif()
	
	if((NOT OS_CYGWIN) AND (NOT OS_HAIKU) AND (NOT OS_MINGW) AND (NOT OS_DOS))
		target_link_libraries(openttd PRIVATE c)
	endif()
endif()

if(OS_MORPHOS)
	# -Wstrict-prototypes generates much noise because of system headers
	target_compile_options(openttd PRIVATE --Wno-strict-prototypes)
endif()

if(OS_OPENBSD)
	target_link_libraries(openttd PRIVATE pthread)
endif()

if(OS_WIN32 AND ENABLE_NETWORK)
	target_link_libraries(openttd PRIVATE winmm.lib ws2_32.lib)
endif()

#if  "$os" = "OS_OSX" ]; then
#		LDFLAGS="$LDFLAGS -framework Cocoa"
#
#		# Add macports include dir which is not always set a default system dir. This avoids zillions of bogus warnings.
#		CFLAGS="$CFLAGS -isystem/opt/local/include"
#
#		if  "$enable_dedicated" = "0" ] && ( "$cpu_type" = "32" ] ||  "$enable_universal" != "0" ]); then
#			LIBS="$LIBS -framework QuickTime"
#		else
#			CFLAGS="$CFLAGS -DNO_QUICKTIME"
#		fi
#
#		if  "$enable_universal" = "0" ]; then
#			# Universal builds set this elsewhere
#			CFLAGS="$OS_OSX_SYSROOT $CFLAGS"
#			LDFLAGS="$OS_OSX_LD_SYSROOT $LDFLAGS"
#		fi
#	fi

if (OS_BEOS OR OS_HAIKU)
	target_link_libraries(openttd PRIVATE midi be)
endif()

# Most targets act like UNIX, just with some additions
if(OS_UNIX OR OS_BEOS OR OS_HAIKU OR OS_OSX OR OS_MORPHOS OR OS_FREEBSD OR OS_DRAGONFLY OR OS_OPENBSD OR OS_NETBSD OR OS_HPUX OR OS_SUNOS OR OS_OS2)
	add_definitions(-DUNIX)
endif()
# And others like Windows
if(OS_MINGW OR OS_CYGWIN)
	add_definitions(-DWIN)
endif()

# 64bit machines need -D_SQ64
#TODO: does not work for cross-compiling (detects host rather than target)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(openttd PRIVATE _SQ64)
endif()

	