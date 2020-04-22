function( StdFilesystem target )
	if( NOT CMAKE_CXX_STANDARD )
		message( FATAL_ERROR "CMAKE_CXX_STANDARD not set" )
	endif()

	set( include_dir "${CMAKE_CURRENT_BINARY_DIR}/StdFilesystem" )
	if( EXISTS "${include_dir}/StdFilesystem.h" )
		target_sources( ${target} PRIVATE "${include_dir}/StdFilesystem.h" )
		target_include_directories( ${target} PRIVATE "${include_dir}" )
		return()
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

						if( NOT "${define}" STREQUAL "" )
							message( STATUS "     define:    ${define}" )
						endif()

						if( NOT "${library}" STREQUAL "" )
							message( STATUS "     library:   ${library}" )
							target_link_libraries( ${target} ${library} )
						endif()

						if( NOT "${define}" STREQUAL "" )
							set( STD_FILESYSTEM_DEFINE    "#define ${define} 1" )
						endif()
						set( STD_FILESYSTEM_INCLUDE   "${include}" )
						set( STD_FILESYSTEM_NAMESPACE "${namespace}" )
						configure_file( "${CMAKE_CURRENT_LIST_DIR}/CMake/StdFilesystem.h" "${include_dir}/StdFilesystem.h" @ONLY NEWLINE_STYLE LF )

						target_sources( ${target} PRIVATE "${include_dir}/StdFilesystem.h" )
						target_include_directories( ${target} PRIVATE "${include_dir}" )

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
