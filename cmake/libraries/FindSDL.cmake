#This file tries to find SDL on the system
#Input paramaters : STATIC_LIBRARY_SUFFIX
#Ouptut paramaters : SDL_FOUND
#Output target : SDL::SDL (if found)
include(FindPkgConfig)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(SDL sdl>=1.2)
else()
	set(SDL_CFLAGS "")
endif()

if (SDL_STATIC)
	set(CMAKE_IMPORT_LIBRARY_SUFFIX ${STATIC_LIBRARY_SUFFIX})
endif()


find_path(SDL_INCLUDE_DIR SDL.h
		HINTS ${SDL_INCLUDE_DIR} ${SDL_STATIC_INCLUDE_DIRS}
		DOC "SDL include dirs (where SDL.h is located) [guessed]")

#Todo: static linking see CMAKE_IMPORT_LIBRARY_SUFFIX
find_library(SDL_LIBRARY
				NAMES SDL ${SDL_LIBRARIES} ${SDL_STATIC_LIBRARIES}
				HINTS ${SDL_LIBRARY_DIRS}
				DOC "SDL library file [guessed]")


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL SDL_INCLUDE_DIR SDL_LIBRARY)

if(SDL_FOUND)
	add_library(SDL::SDL STATIC IMPORTED)
			
	set_target_properties(SDL::SDL PROPERTIES
							INTERFACE_INCLUDE_DIRECTORIES ${SDL_INCLUDE_DIR}
									IMPORTED_LOCATION ${SDL_LIBRARY}
							INTERFACE_COMPILE_OPTIONS ${SDL_CFLAGS})
	if (SDL_STATIC)
		set_target_properties(SDL::SDL PROPERTIES
								INTERFACE_LINK_LIBRARIES ${SDL_STATIC_LIBRARIES})
	endif()
endif()
