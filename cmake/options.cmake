#Static
if(MINGW OR CYGWIN OR MORPHOS OR DOS)
	option(ENABLE_STATIC "Create a static build" ON)
else()
	option(ENABLE_STATIC "Create a static build" OFF)
endif()

if (ENABLE_STATIC AND NOT (MINGW OR CYGWIN OR MORPHOS OR DOS OR APPLE))
	message(WARNING "Static is only known to work on Windows, DOS, MacOSX and MorphOS\nUse static at your own risk on this platform")
endif()

#Debug level
set(DEBUG_LEVEL 0 CACHE STRING "Select debug level for compilation. [0-3]. For multi-configurations environments (IDE for example) you must also set the appropriate configuration (Debug or Release)")
set_property(CACHE DEBUG_LEVEL PROPERTY STRINGS 0 1 2 3)

#Desync debug
option(DEBUG_DESYNC "Enable debug desync mode. WARNING: desync debug functions slow down the game considerably. Use only when you are instructed to do so or when you know what you are doing." OFF)
if (DEBUG_DESYNC)
	add_compile_definitions(RANDOM_DEBUG)
endif()

#Profiling
option(ENABLE_PROFILING "Enable profiling (not on MSVC)" OFF)

#Assert
option(ENABLE_ASSERT "Enable assertions" ON)

#Threads
option(WITH_THREADS "Enable thread support" ON)

#SSE
option(WITH_SSE "Enable SSE" ON)

#OS
set(OS ${OS} CACHE STRING "OS (target) (UNIX, OSX, FREEBSD, DRAGONFLY, OPENBSD, NETBSD, HPUX, MORPHOS, BEOS, SUNOS, CYGWIN, MINGW, OS2, and DOS)")

option(ENABLE_NETWORK "Enable network" ON)

set(BINARY_NAME "openttd" CACHE STRING "Binary output name")

#Libraries
if (OS_WIN32)
	set(OPENTTD_USEFUL_PATH "" CACHE PATH "Path to the (extracted) openttd-useful archive (pointing to the \"OpenTTD essentials\" directory) for pre-compiled binaries for Windows Visual studio")
endif()





