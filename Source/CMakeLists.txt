cmake_minimum_required( VERSION 3.19 FATAL_ERROR )

set( CMAKE_CONFIGURATION_TYPES    "Release" )
set( CMAKE_SKIP_INSTALL_RULES     TRUE )
set( CMAKE_CXX_STANDARD           20 )
set( CMAKE_CXX_STANDARD_REQUIRED  YES )

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake" )

project( ReDefine LANGUAGES CXX )

if( DEFINED ENV{CLANG_TIDY} )
	message( STATUS "CMAKE_CXX_CLANG_TIDY = $ENV{CLANG_TIDY}" )
	set( CMAKE_CXX_CLANG_TIDY $ENV{CLANG_TIDY} )
endif()

add_library( ReDefineLib STATIC )
target_sources( ReDefineLib
	PUBLIC
		${CMAKE_CURRENT_LIST_DIR}/ReDefine.h

	PRIVATE
		${CMAKE_CURRENT_LIST_FILE}

		Defines.cpp
		Functions.cpp
		Log.cpp
		Operators.cpp
		Raw.cpp
		ReDefine.cpp
		Script.cpp
		Text.cpp
		Variables.cpp

		# FOClassic
		Ini.cpp
		Ini.h
)

add_executable( ReDefine "" )
target_sources( ReDefine
	PRIVATE
		${CMAKE_CURRENT_LIST_FILE}
		Executable/Main.cpp

		# FOClassic
		Executable/CommandLine.cpp
		Executable/CommandLine.h
)

if( EXISTS "${CMAKE_CURRENT_LIST_DIR}/Parser.cpp" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/Parser.h" )
	target_sources( ReDefine
		PUBLIC
			${CMAKE_CURRENT_LIST_DIR}/Parser.h
		PRIVATE
			Parser.cpp
	)
	target_compile_definitions( ReDefine    PRIVATE HAVE_PARSER )
	target_compile_definitions( ReDefineLib PRIVATE HAVE_PARSER )
endif()

target_link_libraries( ReDefine PRIVATE ReDefineLib )

if( MSVC )
	set( pdb "ReDefine.pdb" )
	set( pdb_public "ReDefine.pdb.PUBLIC" )

	foreach( target IN ITEMS ReDefine ReDefineLib )
		set_property( TARGET ${target} APPEND_STRING PROPERTY COMPILE_FLAGS "/Zi " )

		set_property( TARGET ${target} APPEND_STRING PROPERTY COMPILE_FLAGS "/MT " )
		set_property( TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS "/OPT:REF " )
	endforeach()
	set_property( TARGET ReDefine APPEND_STRING PROPERTY LINK_FLAGS "/DEBUG /PDBSTRIPPED:$(ConfigurationName)/${pdb_public} /PDBALTPATH:${pdb} " )
else() # assumes UNIX
	set(CMAKE_BUILD_TYPE RelWithDebInfo)

	include( CheckCXXCompilerFlag )
	check_cxx_compiler_flag( -Wall      COMPILER_FLAG_WALL )
	check_cxx_compiler_flag( -Werror    COMPILER_FLAG_WERROR )
	check_cxx_compiler_flag( -Wextra    COMPILER_FLAG_WEXTRA )
	check_cxx_compiler_flag( -Wpedantic COMPILER_FLAG_WPEDANTIC )

	foreach( target IN ITEMS ReDefine ReDefineLib )
		if( COMPILER_FLAG_WALL )
			target_compile_options( ${target} PRIVATE -Wall )
		endif()
		if( COMPILER_FLAG_WERROR )
			target_compile_options( ${target} PRIVATE -Werror )
		endif()
		if( COMPILER_FLAG_WEXTRA )
			target_compile_options( ${target} PRIVATE -Wextra )
		endif()
		if( COMPILER_FLAG_WPEDANTIC )
			target_compile_options( ${target} PRIVATE -Wpedantic )
		endif()
	endforeach()

	check_cxx_compiler_flag( -static-libgcc    COMPILER_FLAG_STATIC_LIBGCC )
	check_cxx_compiler_flag( -static-libstdc++ COMPILER_FLAG_STATIC_LIBSTDCPP )

	if( COMPILER_FLAG_STATIC_LIBGCC )
		target_link_libraries( ReDefine PRIVATE -static-libgcc )
	endif()
	if( COMPILER_FLAG_STATIC_LIBSTDCPP )
		target_link_libraries( ReDefine PRIVATE -static-libstdc++ )
	endif()
endif()

add_subdirectory( Test )

# Prettify IDE

set_property( GLOBAL PROPERTY USE_FOLDERS ON )
set_property( GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake" )

source_group( " "         REGULAR_EXPRESSION "\.([CcHh]|[CcHh][Pp][Pp])$" )
source_group( "CMake"     REGULAR_EXPRESSION "[Cc][Mm][Aa][Kk][Ee]" )
