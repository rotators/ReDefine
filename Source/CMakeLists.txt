cmake_minimum_required( VERSION 3.7.2 FATAL_ERROR ) # cannot go higher due to nuremberg setup

set( CMAKE_CONFIGURATION_TYPES    "Release" )
set( CMAKE_CXX_STANDARD           14 )
set( CMAKE_CXX_STANDARD_REQUIRED  YES )
set( CMAKE_SKIP_INSTALL_RULES     TRUE )

project( ReDefine )

add_executable( ReDefine "" )
target_sources( ReDefine
	PUBLIC
		${CMAKE_CURRENT_LIST_DIR}/ReDefine.h

	PRIVATE
		${CMAKE_CURRENT_LIST_FILE}
		Main.cpp

		Defines.cpp
		Functions.cpp
		Log.cpp
		Operators.cpp
		Raw.cpp
		ReDefine.cpp
		Script.cpp
		StdFilesystem.h
		Text.cpp
		Variables.cpp

		# FOClassic
		FOClassic/CommandLine.cpp
		FOClassic/CommandLine.h
		FOClassic/Ini.cpp
		FOClassic/Ini.h
)

if( EXISTS "${CMAKE_CURRENT_LIST_DIR}/Parser.cpp" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/Parser.h" )
	target_sources( ReDefine
		PUBLIC
			${CMAKE_CURRENT_LIST_DIR}/Parser.h
		PRIVATE
			Parser.cpp
	)
	target_compile_definitions( ReDefine PRIVATE HAVE_PARSER )
endif()

if( MSVC )
	set( pdb "ReDefine.pdb" )
	set( pdb_public "ReDefine.pdb.PUBLIC" )

	set_property( TARGET ReDefine APPEND_STRING PROPERTY COMPILE_FLAGS "/Zi " )
	set_property( TARGET ReDefine APPEND_STRING PROPERTY LINK_FLAGS "/DEBUG /PDBSTRIPPED:$(ConfigurationName)/${pdb_public} /PDBALTPATH:${pdb} " )

	set_property( TARGET ReDefine APPEND_STRING PROPERTY COMPILE_FLAGS "/MT " )
	set_property( TARGET ReDefine APPEND_STRING PROPERTY LINK_FLAGS "/OPT:REF " )
else() # assumes UNIX
	set(CMAKE_BUILD_TYPE RelWithDebInfo)

	include( CheckCXXCompilerFlag )
	check_cxx_compiler_flag( -Wall      COMPILER_FLAG_WALL )
	check_cxx_compiler_flag( -Werror    COMPILER_FLAG_WERROR )
	check_cxx_compiler_flag( -Wextra    COMPILER_FLAG_WEXTRA )
	check_cxx_compiler_flag( -Wpedantic COMPILER_FLAG_WPEDANTIC )

	if( COMPILER_FLAG_WALL )
		target_compile_options( ReDefine PRIVATE -Wall )
	endif()
	if( COMPILER_FLAG_WERROR )
		target_compile_options( ReDefine PRIVATE -Werror )
	endif()
	if( COMPILER_FLAG_WEXTRA )
		target_compile_options( ReDefine PRIVATE -Wextra )
	endif()
	if( COMPILER_FLAG_WPEDANTIC )
		target_compile_options( ReDefine PRIVATE -Wpedantic )
	endif()

	check_cxx_compiler_flag( -static-libgcc COMPILER_FLAG_STATIC_LIBGCC )
	check_cxx_compiler_flag( -static-libstdc++ COMPILER_FLAG_STATIC_LIBSTDCPP )

	if( COMPILER_FLAG_STATIC_LIBGCC )
		target_link_libraries( ReDefine -static-libgcc )
	endif()
	if( COMPILER_FLAG_STATIC_LIBSTDCPP )
		target_link_libraries( ReDefine -static-libstdc++ )
	endif()
endif()

function( StdFilesystem target )
	if( NOT CMAKE_CXX_STANDARD )
		message( FATAL_ERROR "CMAKE_CXX_STANDARD not set" )
	endif()

	message( STATUS "Find std::filesystem..." )
	include( CheckIncludeFileCXX )
	include( CheckCXXSourceCompiles )

	unset( _std_filesystem_include_ CACHE )
	unset( _std_filesystem_namespace_ CACHE )

	set( CMAKE_REQUIRED_QUIET true )
	set( CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CMAKE_CXX${CMAKE_CXX_STANDARD}_STANDARD_COMPILE_OPTION}" )

	foreach( include IN ITEMS filesystem experimental/filesystem )
		# CMAKE_REQUIRED_LIBRARIES
		# see https://cmake.org/cmake/help/latest/policy/CMP0075.html
		unset( CMAKE_REQUIRED_LIBRARIES )

		check_include_file_cxx( ${include} _std_filesystem_include_ )
		if( NOT _std_filesystem_include_ )
			unset( _std_filesystem_include_ CACHE )
			continue()
		endif()

		# _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
		# see https://github.com/microsoft/STL/blob/b72c0a609f00b92aedbb18d09faf72d48ceb9b4f/stl/inc/experimental/filesystem#L27-L31
		foreach( define IN ITEMS "" _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING )
			if( "${define}" STREQUAL "" )
				unset( CMAKE_REQUIRED_DEFINITIONS )
			else()
				set( CMAKE_REQUIRED_DEFINITIONS -D${define} )
			endif()

			foreach( namespace IN ITEMS std::filesystem std::experimental::filesystem std::experimental::filesysystem::v1 std::tr2::sys std::__fs::filesystem )
				foreach( library IN ITEMS "" stdc++fs c++fs c++experimental )
					set( CMAKE_REQUIRED_LIBRARIES ${library} )

					check_cxx_source_compiles( "#include <${include}>
					int main(){ ${namespace}::exists(\"file\"); }"
					_std_filesystem_namespace_ )

					if( _std_filesystem_namespace_ )
						message( STATUS "Find std::filesystem... found" )
						message( STATUS "     include:   ${include}" )
						message( STATUS "     namespace: ${namespace}" )

						if( "${include}" STREQUAL "filesystem" )
							target_compile_definitions( ${target} PRIVATE HAVE_FILESYSTEM )
							target_compile_definitions( ${target} PRIVATE HAVE_FILESYSTEM_NAMESPACE=${namespace} )

						else()
							target_compile_definitions( ${target} PRIVATE HAVE_EXPERIMENTAL_FILESYSTEM )
							target_compile_definitions( ${target} PRIVATE HAVE_EXPERIMENTAL_FILESYSTEM_NAMESPACE=${namespace} )
						endif()

						if( NOT "${define}" STREQUAL "" )
							message( STATUS "     define:    ${define}" )
							target_compile_definitions( ${target} PRIVATE ${define} )
						endif()

						if( NOT "${library}" STREQUAL "" )
							message( STATUS "     library:   ${library}" )
							target_link_libraries( ${target} ${library} )
						endif()

						return()
					else()
						unset( _std_filesystem_namespace_ CACHE )
					endif()
				endforeach()
			endforeach()
		endforeach()
		unset( _std_filesystem_include_ CACHE )
	endforeach()

	message( STATUS "Find std::filesystem... not found" )
endfunction()

StdFilesystem( ReDefine )

# Prettify IDE

set_property( GLOBAL PROPERTY USE_FOLDERS ON )
set_property( GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake" )

source_group( " "         REGULAR_EXPRESSION "\.([CcHh]|[CcHh][Pp][Pp])$" )
source_group( "CMake"     REGULAR_EXPRESSION "[Cc][Mm][Aa][Kk][Ee]" )
source_group( "FOClassic" REGULAR_EXPRESSION "FOClassic" )
