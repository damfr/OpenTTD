#Finds lzo2 library
#Output paramaters: LZO2_FOUND LZO2_LIB LZO2_INCLUDE_DIR
#Output target: Lzo::Lzo2
find_path(LZO2_INCLUDE_DIR NAMES lzo/lzoconf.h)

find_library(LZO2_LIB  NAMES lzo2)

add_library(Lzo::Lzo2 STATIC IMPORTED)
		
set_target_properties(Lzo::Lzo2 PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES ${LZO2_INCLUDE_DIR}
						IMPORTED_LOCATION ${LZO2_LIB})


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LZO2 LZO2_INCLUDE_DIR LZO2_LIB)