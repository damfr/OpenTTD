#This CMake script is called at build time to update the revision in rev.cpp and os/windows/ottdres.rc.in
#Variable REVISION_FILE must be set to the file containing the configure_revision_file() function (normally cmake/revision.cmake)
#Variable SOURCE_ROOT_DIR must be set to CMAKE_SOURCE_DIR
include(${REVISION_FILE})
message(STATUS "Updating rev.cpp")
configure_revision_file(${SOURCE_ROOT_DIR})