set( PWD "${CMAKE_CURRENT_BINARY_DIR}" )

if( NOT REDEFINE )
	message( FATAL_ERROR "REDEFINE not set" )
elseif( NOT TEST_FILE )
	message( FATAL_ERROR "TEST_FILE not set" )
endif()

get_filename_component( TEST_DIR "${TEST_FILE}" DIRECTORY )
set( REDEFINE_CFG "${TEST_DIR}/ReDefine.cfg" )
file( STRINGS "${TEST_FILE}" content )

foreach( line IN LISTS content )
	string( REGEX MATCH "^[A-Z]+" var "${line}" )
	if( NOT "${var}" STREQUAL "" )
		if( TEST_${var} AND NOT "${var}" STREQUAL "SCRIPT" )
			message( FATAL_ERROR "Variable <TEST_${var}> cannot be set" )
		endif()

		string( REGEX REPLACE "^${var}[\\t ]*" "" line "${line}" )
		if( "${line}" STREQUAL "" )
			message( FATAL_ERROR "Variable <TEST_${var}> is empty" )
		endif()

		message( STATUS "TEST_${var} = ${line}" )
		if( "${var}" STREQUAL "SCRIPT" )
			list( APPEND TEST_${var} "${line}" )
		else()
			set( TEST_${var} "${line}" )
		endif()
	endif()
endforeach()

foreach( var IN ITEMS ORIGIN EXPECT )
	if( NOT TEST_${var} )
		message( FATAL_ERROR "Variable <TEST_${var}> missing" )
	endif()
endforeach()

file( REMOVE_RECURSE "${PWD}/Run" )
file( MAKE_DIRECTORY "${PWD}/Run" )

if( EXISTS "${REDEFINE_CFG}" )
	message( STATUS "Using custom ReDefine.cfg" )
	file( COPY "${REDEFINE_CFG}" DESTINATION "${PWD}/Run" )
else()
	message( STATUS "Using dummy ReDefine.cfg" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "[ReDefine]\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "HeadersDir = .\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "ScriptsDir = .\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "[Defines]\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "DUMMY = ReDefine.cfg DUMMY\n" )
endif()

if( TEST_SCRIPT )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "[Script]\n" )
	foreach( line IN LISTS TEST_SCRIPT )
		file( APPEND "${PWD}/Run/ReDefine.cfg" "${line}\n" )
	endforeach()
endif()

file( WRITE "${PWD}/Run/ReDefine.ssl" "${TEST_ORIGIN}" )

execute_process(
	COMMAND ${REDEFINE} --debug-changes 1
	WORKING_DIRECTORY "${PWD}/Run"
)

file( STRINGS "Run/ReDefine.ssl" lines )
list( GET lines 0 result )

if( NOT "${result}" STREQUAL "${TEST_EXPECT}" )
	message( "" )
	message( STATUS "EXPECT  ${TEST_EXPECT}" )
	message( STATUS "RESULT  ${result}" )
	message( "" )
	message( FATAL_ERROR "TEST FAILED" )
endif()
file( REMOVE_RECURSE "Run" )
