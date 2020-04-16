set( PWD "${CMAKE_CURRENT_BINARY_DIR}" )

if( NOT REDEFINE )
	message( FATAL_ERROR "REDEFINE not set" )
elseif( NOT TEST_FILE )
	message( FATAL_ERROR "TEST_FILE not set" )
endif()

get_filename_component( TEST_DIR "${TEST_FILE}" DIRECTORY )
set( REDEFINE_CFG "${TEST_DIR}/ReDefine.cfg" )
file( STRINGS "${TEST_FILE}" content )

list( APPEND T_REQUIRED ORIGIN )
list( APPEND T_REQUIRED EXPECT )

list( APPEND T_LIST CONFIG )
list( APPEND T_LIST SCRIPT )

foreach( line IN LISTS content )
	string( REGEX MATCH "^[A-Z]+" var "${line}" )
	if( NOT "${var}" STREQUAL "" )

		list( FIND T_LIST "${var}" idx )
		if( "${idx}" GREATER_EQUAL 0 )
			set( var_is_list TRUE )
		else()
			set( var_is_list FALSE )
		endif()
		unset( idx )

		if( TEST_${var} AND NOT var_is_list )
			message( FATAL_ERROR "Variable <TEST_${var}> cannot be set" )
		endif()

		string( REGEX REPLACE "^${var}[\\t ]*" "" line "${line}" )
		if( "${line}" STREQUAL "" )
			message( FATAL_ERROR "Variable <TEST_${var}> is empty" )
		endif()

		if( var_is_list )
			message( STATUS "TEST_${var} += ${line}" )
			list( APPEND TEST_${var} "${line}" )
		else()
			message( STATUS "TEST_${var} = ${line}" )
			set( TEST_${var} "${line}" )
		endif()
	endif()
endforeach()

foreach( var IN LISTS T_REQUIRED )
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
	file( APPEND "${PWD}/Run/ReDefine.cfg" "[Defines]\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "DUMMY = ReDefine.cfg DUMMY\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "[ReDefine]\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "HeadersDir = .\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "ScriptsDir = .\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "### TEST CONFIGURATION ###\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "\n" )
	foreach( line IN LISTS TEST_CONFIG )
		file( APPEND "${PWD}/Run/ReDefine.cfg" "${line}\n" )
	endforeach()
endif()

if( TEST_SCRIPT )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "\n" )
	file( APPEND "${PWD}/Run/ReDefine.cfg" "[Script]\n" )
	foreach( line IN LISTS TEST_SCRIPT )
		file( APPEND "${PWD}/Run/ReDefine.cfg" "${line}\n" )
	endforeach()
endif()

message( "" )
message( STATUS "ReDefine run" )
message( "" )
file( WRITE "${PWD}/Run/ReDefine.ssl" "${TEST_ORIGIN}" )

execute_process(
	COMMAND ${REDEFINE} --debug-changes 1
	WORKING_DIRECTORY "${PWD}/Run"
	RESULT_VARIABLE exitcode
)

file( STRINGS "${PWD}/Run/ReDefine.ssl" lines )
list( GET lines 0 result )

if( NOT exitcode EQUAL 0 )
	find_program( GDB gdb )
	if( GDB )
		message( "" )
		message( STATUS "ReDefine+GDB run" )
		message( "" )
		file( WRITE "${PWD}/Run/ReDefine.ssl" "${TEST_ORIGIN}" )

		execute_process(
			COMMAND ${GDB} -return-child-result -ex "catch throw" -ex "run" -ex "backtrace full" -ex "quit" --args ${REDEFINE} --debug-changes 2
			WORKING_DIRECTORY "${PWD}/Run"
		)

		file( STRINGS "${PWD}/Run/ReDefine.ssl" lines )
		list( GET lines 0 result )
	endif()
endif()

if( NOT "${result}" STREQUAL "${TEST_EXPECT}" )
	message( "" )
	message( STATUS "EXPECT  ${TEST_EXPECT}" )
	message( STATUS "RESULT  ${result}" )
	message( "" )
	message( FATAL_ERROR "TEST FAILED" )
endif()
file( REMOVE_RECURSE "Run" )
