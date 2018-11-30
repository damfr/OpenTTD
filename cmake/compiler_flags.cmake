#This file sets global compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU") #GCC
	add_compile_options(-Wall -Wno-multichar -Wsign-compare -Wundef -Wwrite-strings -Wpointer-arith -W -Wno-unused-parameter -Wredundant-decls -Wformat=2 -Wformat-security)
	if (NOT ENABLE_ASSERT)
		# Do not warn about unused variables when building without asserts
		add_compile_options(-Wno-unused-variable)
		if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.6)
			# GCC 4.6 gives more warnings, disable them too
			add_compile_options(-Wno-unused-but-set-variable -Wno-unused-but-set-parameter)
		endif()
	endif()
	
	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 3.4)
		# Warn when a variable is used to initialise itself:
		# int a = a;
		add_compile_options(-Winit-self)
	endif()

	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.0)
		# GCC 4.0+ complains about that we break strict-aliasing.
		#  On most places we don't see how to fix it, and it doesn't
		#  break anything. So disable strict-aliasing to make the
		#  compiler all happy.
		add_compile_options(-fno-strict-aliasing)
		# Warn about casting-out 'const' with regular C-style cast.
		#  The preferred way is const_cast<>() which doesn't warn.
		add_compile_options(-Wcast-qual)
	endif()
	
	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.2)
		# GCC 4.2+ automatically assumes that signed overflows do
		# not occur in signed arithmetics, whereas we are not
		# sure that they will not happen. It furthermore complains
		# about its own optimized code in some places.
		add_compile_options(-fno-strict-overflow)
		# GCC 4.2 no longer includes -Wnon-virtual-dtor in -Wall.
		# Enable it in order to be consistent with older GCC versions.
		add_compile_options(-Wnon-virtual-dtor)
	endif()

#Should be already set if available by set_property(.. CXX_STANDARD ..)
#	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.3 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
		# Use gnu++0x mode so static_assert() is available.
		# Don't use c++0x, it breaks mingw (with gcc 4.4.0).
#		cxxflags="$cxxflags -std=gnu++0x"
#	fi
	if(CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 4.5)
		# Prevent optimisation supposing enums are in a range specified by the standard
		# For details, see http://gcc.gnu.org/PR43680
		add_compile_options(-fno-tree-vrp)
	endif()

	if(CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 4.7)
		# Disable -Wnarrowing which gives many warnings, such as:
		# warning: narrowing conversion of '...' from 'unsigned int' to 'int' inside { } [-Wnarrowing]
		# They are valid according to the C++ standard, but useless.
		add_compile_options(-Wno-narrowing)
	endif()

	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.7)
		# Disable bogus 'attempt to free a non-heap object' warning
		add_compile_options(-Wno-free-nonheap-object)
	endif()
	
	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
		# -flifetime-dse=2 (default since GCC 6) doesn't play
		# well with our custom pool item allocator
		add_compile_options(-flifetime-dse=1)
	endif()

	if(OS_OSX AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.1)
		# Apple's GCC 4.0 has a compiler bug for x86_64 with (higher) optimization,
		# wrongly optimizing ^= in loops. This disables the failing optimisation.
		add_compile_options(-fno-expensive-optimizations)
	endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") #Visual studio
	add_compile_options(/J /Zc:throwingNew)
	
endif()
	