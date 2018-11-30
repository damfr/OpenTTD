#This file provides a function that configures the 2 revision files rev.cpp and os/windows/ottdres.rc.in with the detected version (via Git or .ottdrev file)
#Parameter : SOURCE_ROOT_DIR : the root source dir, where the bin, objs, src folders are located (ususally ${CMAKE_SOURCE_DIR})
function(configure_revision_file SOURCE_ROOT_DIR)
	find_package(Git)
	if (GIT_FOUND AND EXISTS ${SOURCE_ROOT_DIR}/.git)
		#We are a git checkout
		#Refresh the index to make sure file stat info is in sync, then look for modifications
		execute_process(COMMAND ${GIT_EXECUTABLE} update-index --refresh OUTPUT_QUIET WORKING_DIRECTORY ${SOURCE_ROOT_DIR})
		execute_process(COMMAND ${GIT_EXECUTABLE} diff-index HEAD
						OUTPUT_VARIABLE GIT_DIFF_INDEX_OUTPUT  WORKING_DIRECTORY ${SOURCE_ROOT_DIR})
		if(GIT_DIFF_INDEX_OUTPUT)
			set(MODIFIED 2) #The source has been modified since last commit
		else()
			set(MODIFIED 0)
		endif()
		set(ENV{LC_ALL} C)
		execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
						OUTPUT_VARIABLE HASH OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${SOURCE_ROOT_DIR}) #Hash of the last commmit
		execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
						OUTPUT_VARIABLE SHORT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${SOURCE_ROOT_DIR}) #Short 8 character hash
		set(ENV{TZ} UTC)
		execute_process(COMMAND ${GIT_EXECUTABLE} show -s --date=format-local:%Y%m%d --format=%cd HEAD
						OUTPUT_VARIABLE ISODATE OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${SOURCE_ROOT_DIR}) #Date of the last commit, in the form YYYYmmdd
		
		execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
						OUTPUT_VARIABLE BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${SOURCE_ROOT_DIR}) #Branch of the last commit
		execute_process(COMMAND ${GIT_EXECUTABLE} name-rev --name-only --tags --no-undefined HEAD ERROR_QUIET
						OUTPUT_VARIABLE TAG OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${SOURCE_ROOT_DIR}) #Tag of the last commit, if it has one
		string(REGEX REPLACE "\\^0$" "" TAG "${TAG}") #Strip the tags/ at the front
		
		if(TAG)
			set(VERSION ${TAG})
		elseif(BRANCH STREQUAL "master")
			set(VERSION "${ISODATE}-g${SHORTHASH}")
		else()
			set(VERSION "${ISODATE}-${BRANCH}-g${SHORTHASH}")
		endif()
		
		if(MODIFIED EQUAL 2)
			set(VERSION "${VERSION}M")
		endif()
	
	elseif(EXISTS ${SOURCE_ROOT_DIR}/.ottdrev)
		#We are an exported source bundle
		#Assume the file .ottdrev is in the form $VERSION	$ISODATE	$MODIFIED	$HASH (tab-separated)
		file(READ ${SOURCE_ROOT_DIR}/.ottdrev OTTDREV_CONTENTS)
		
		string(REGEX MATCHALL "\t?([^\t]*)\t?" OUTPUT_REGEX OTTDREV_CONTENTS)
		set(VERSION ${CMAKE_MATCH_1})
		set(ISODATE ${CMAKE_MATCH_2})
		set(MODIFIED ${CMAKE_MATCH_3})
		set(HASH ${CMAKE_MATCH_4})
	else()
		#We don't know
		set(VERSION 0)
		set(ISODATE 0)
		set(MODIFIED 1)
		set(HASH 0)
	endif()

	configure_file(${SOURCE_ROOT_DIR}/src/rev.cpp.in ${SOURCE_ROOT_DIR}/src/rev.cpp)
	configure_file(${SOURCE_ROOT_DIR}/src/os/windows/ottdres.rc.in ${SOURCE_ROOT_DIR}/src/os/windows/ottdres.rc)
endfunction()


