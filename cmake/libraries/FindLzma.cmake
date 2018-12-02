#Finds lzma library
#Output paramaters: LZMA_FOUND LZMA_LIB LZMA_INCLUDE_DIR
#Output target: Lzma::Lzma
find_path(LZMA_INCLUDE_DIR NAMES lzma.h)

find_library(LZMA_LIB  NAMES lzma)

add_library(Lzma::Lzma STATIC IMPORTED)
		
set_target_properties(Lzma::Lzma PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES ${LZMA_INCLUDE_DIR}
						IMPORTED_LOCATION ${LZMA_LIB})


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LZMA LZMA_INCLUDE_DIR LZMA_LIB)