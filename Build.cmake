# ReDefine build script
# Based on FOClassic setup
cmake_minimum_required( VERSION 3.7.2 FATAL_ERROR )

#> AutomatedBuild.cmake <#

# Prepare build directory
function( CreateBuildDirectory dir generator platform tool file )

	# use full path
	file( TO_CMAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/${dir}" dir )
	file( TO_NATIVE_PATH "${dir}" dir_native )

	message( STATUS "Checking ${dir_native}" )
	if( NOT EXISTS "${dir}" )
		message( STATUS "Creating ${dir_native}" )
		file( MAKE_DIRECTORY "${dir}" )
	endif()

	if( NOT EXISTS "${dir}/${file}" )
		if( UNIX AND tool )
			set( info ", toolchain: ${tool}" )
			set( toolchain -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_LIST_DIR}/Misc/CMake/${tool}.cmake )
		elseif( WIN32 AND tool )
			set( info ", toolset: ${tool}" )
			set( toolset -T ${tool} )
		endif()
		if( platform )
			string( APPEND info ", platform: ${platform}" )
			set( platform -A ${platform} )
		endif()
		message( STATUS "Starting generator (${generator}${info})" )
		execute_process(
			COMMAND ${CMAKE_COMMAND} ${toolchain} -G "${generator}" ${platform} ${toolset} -S "${CMAKE_CURRENT_LIST_DIR}/Source"
			RESULT_VARIABLE result
			WORKING_DIRECTORY "${dir}"
		)
		if( NOT result EQUAL 0 )
			list( APPEND BUILD_FAIL "${dir}" )
			set( BUILD_FAIL "${BUILD_FAIL}" PARENT_SCOPE )
			return()
		endif()
	endif()

	if( EXISTS "${dir}/${file}" )
		list( APPEND BUILD_DIRS "${dir}" )
		set( BUILD_DIRS "${BUILD_DIRS}" PARENT_SCOPE )
	else()
		list( APPEND BUILD_FAIL "${dir}" )
		set( BUILD_FAIL "${BUILD_FAIL}" PARENT_SCOPE )
	endif()

endfunction()

function( RunAllBuilds )

	foreach( dir IN LISTS BUILD_DIRS )
		file( TO_NATIVE_PATH "${dir}" dir_native )
		message( STATUS "Starting build (${dir_native})" )
		execute_process(
			COMMAND ${CMAKE_COMMAND} --build "${dir}" --config Release
			RESULT_VARIABLE result
		)
		if( NOT result EQUAL 0 )
			list( APPEND BUILD_FAIL "${dir}" )
			set( BUILD_FAIL "${BUILD_FAIL}" PARENT_SCOPE )
		endif()
	endforeach()

endfunction()

#> FormatSource.cmake <#

function( FormatSource filename )

	if( NOT EXISTS "${filename}" )
	    return()
	endif()

	set( root "." )
	set( uncrustemp "${root}/FormatSource.tmp" )
	set( uncrustify "${root}/Misc/SourceTools/uncrustify" )
	set( uncrustcfg "${root}/Misc/SourceTools/uncrustify.cfg" )

	if( UNCRUSTIFY_EXECUTABLE )
		set( uncrustify "${UNCRUSTIFY_EXECUTABLE}" )
	endif()

	# CMAKE_EXECUTABLE_SUFFIX is not reliable
	if( WIN32 AND NOT "${uncrustify}" MATCHES "\.[Ee][Xx][Ee]$" )
		string( APPEND uncrustify ".exe" )
	endif()

	# in case of cancelled FormatSource runs
	if( EXISTS "${uncrustemp}" )
		file( REMOVE "${uncrustemp}" )
	endif()

	if( EXISTS "${uncrustify}" )
		execute_process( COMMAND "${uncrustify}" -c "${uncrustcfg}" -l CPP -f "${filename}" -o "${uncrustemp}" -q --if-changed )

		if( EXISTS "${uncrustemp}" )
			file( RENAME "${uncrustemp}" "${filename}" )
		endif()
	endif()

endfunction()

#> ReDefine <#

if( DEFINED ENV{CI} )
	# AppVeyor
	if( DEFINED ENV{APPVEYOR_BUILD_WORKER_IMAGE} )
		if( UNIX AND "$ENV{PLATFORM}" STREQUAL "x32" )
			set( BUILD_TOOL "Linux32" )
		elseif( UNIX AND "$ENV{PLATFORM}" STREQUAL "x64" )
		elseif( WIN32 AND "$ENV{PLATFORM}" STREQUAL "x32" )
		elseif( WIN32 AND "$ENV{PLATFORM}" STREQUAL "x64" )
			set( BUILD_GENERATOR_SUFFIX " Win64" )
		else()
			message( FATAL_ERROR "Unknown platform : $ENV{PLATFORM}" )
		endif()

		if( "$ENV{APPVEYOR_BUILD_WORKER_IMAGE}" MATCHES "^Ubuntu" )
			set( BUILD_FILE "Makefile" )
			set( BUILD_GENERATOR "Unix Makefiles" )
		elseif( "$ENV{APPVEYOR_BUILD_WORKER_IMAGE}" STREQUAL "Visual Studio 2017" )
			set( BUILD_FILE "ReDefine.sln" )
			set( BUILD_GENERATOR "Visual Studio 15 2017${BUILD_GENERATOR_SUFFIX}" )
		else()
			message( FATAL_ERROR "Unknown AppVeyor image ('$ENV{APPVEYOR_BUILD_WORKER_IMAGE}')" )
		endif()
	elseif( DEFINED ENV{GITHUB_ACTIONS} )
		if( UNIX AND "$ENV{MATRIX_PLATFORM}" STREQUAL "x32" )
			set( BUILD_TOOL "Linux32" )
		elseif( UNIX AND "$ENV{MATRIX_PLATFORM}" STREQUAL "x64" )
		elseif( WIN32 AND "$ENV{MATRIX_PLATFORM}" STREQUAL "x32" )
		elseif( WIN32 AND "$ENV{MATRIX_PLATFORM}" STREQUAL "x64" )
			set( BUILD_GENERATOR_SUFFIX " Win64" )
		else()
			message( FATAL_ERROR "Unknown platform : $ENV{MATRIX_PLATFORM}" )
		endif()

		if( "$ENV{MATRIX_OS}" MATCHES  "^ubuntu-" )
			set( BUILD_FILE "Makefile" )
			set( BUILD_GENERATOR "Unix Makefiles" )
		elseif( "$ENV{MATRIX_OS}" STREQUAL "windows-2016" )
			set( BUILD_FILE "ReDefine.sln" )
			set( BUILD_GENERATOR "Visual Studio 15 2017${BUILD_GENERATOR_SUFFIX}" )
		elseif( "$ENV{MATRIX_OS}" STREQUAL "windows-2019" )
			set( BUILD_FILE "ReDefine.sln" )
			set( BUILD_GENERATOR "Visual Studio 16 2019" )
			if( "$ENV{MATRIX_PLATFORM}" STREQUAL "x32" )
				set( BUILD_PLATFORM "Win32" )
			elseif( "$ENV{MATRIX_PLATFORM}" STREQUAL "x64" )
				set( BUILD_PLATFORM "x64" )
			else()
				message( FATAL_ERROR "Unknown platform ('$ENV{MATRIX_PLATFORM}')" )
			endif()
		else()
			message( FATAL_ERROR "Unknown GitHub Actions image ('$ENV{MATRIX_OS}')" )
		endif()
	else()
		message( FATAL_ERROR "Unknown CI" )
	endif()
elseif( UNIX )
	set( BUILD_FILE      "Makefile" )
	set( BUILD_GENERATOR "Unix Makefiles" )
#	set( BUILD_TOOL      "Linux32" )
elseif( WIN32 )
	set( BUILD_FILE      "ReDefine.sln" )
	set( BUILD_GENERATOR "Visual Studio 15 2017" )
endif()

FormatSource( "Source/Defines.cpp" )
FormatSource( "Source/Functions.cpp" )
FormatSource( "Source/Log.cpp" )
FormatSource( "Source/Main.cpp" )
FormatSource( "Source/Operators.cpp" )
FormatSource( "Source/Parser.cpp" )
FormatSource( "Source/Parser.h" )
FormatSource( "Source/Raw.cpp" )
FormatSource( "Source/ReDefine.cpp" )
FormatSource( "Source/ReDefine.h" )
FormatSource( "Source/Script.cpp" )
FormatSource( "Source/StdFilesystem.h" )
FormatSource( "Source/Text.cpp" )
FormatSource( "Source/Variables.cpp" )

CreateBuildDirectory( "Build" "${BUILD_GENERATOR}" "${BUILD_PLATFORM}" "${BUILD_TOOL}" "${BUILD_FILE}" )
if( BUILD_FAIL )
	message( FATAL_ERROR "Build error" )
endif()

RunAllBuilds()
if( BUILD_FAIL )
	message( FATAL_ERROR "Build error" )
endif()
