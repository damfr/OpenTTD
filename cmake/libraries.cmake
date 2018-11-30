#This file adds library dependencies to openttd
#Add our own modules to the cmake path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/libraries)


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
