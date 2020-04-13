set( PWD "${CMAKE_CURRENT_BINARY_DIR}" )

if( NOT REDEFINE )
	message( FATAL_ERROR "REDEFINE not set" )
elseif( NOT TEST_FILE )
	message( FATAL_ERROR "TEST_FILE not set" )
endif()

get_filename_component( TEST_DIR "${TEST_FILE}" DIRECTORY )
set( REDEFINE_CFG "${TEST_DIR}/ReDefine.cfg" )

#message( STATUS "TEST_DIR  = ${TEST_DIR}" )
#message( STATUS "TEST_FILE = ${TEST_FILE}" )

file( STRINGS "${TEST_FILE}" lines )
#foreach( line IN LISTS lines )
#	message( STATUS "${line}" )
#endforeach()

list( GET lines 0 script )
list( GET lines 1 origin )
list( GET lines 2 expect )

#message( STATUS "SCRIPT=${script} ORIGIN=${origin} EXPECT=${expect}" )

file( REMOVE_RECURSE "Run" )
file( MAKE_DIRECTORY "Run" )

if( EXISTS "${REDEFINE_CFG}" )
	file( COPY "${REDEFINE_CFG}" DESTINATION "Run" )
endif()

if( NOT "${script}" STREQUAL "-" )
	file( APPEND "Run/ReDefine.cfg" "\n" )
	file( APPEND "Run/ReDefine.cfg" "[Script:Test]\n" )
	file( APPEND "Run/ReDefine.cfg" "Run = ${script}\n" )
endif()

file( WRITE "Run/ReDefine.ssl" "${origin}" )

execute_process(
	COMMAND ${REDEFINE} --debug-changes 1
	WORKING_DIRECTORY ${PWD}/Run
)

file( STRINGS "Run/ReDefine.ssl" lines )
list( GET lines 0 result )

if( NOT "${expect}" STREQUAL "${result}" )
	message( "" )
	message( STATUS "EXPECT  ${expect}" )
	message( STATUS "RESULT  ${result}" )
	message( "" )
	message( FATAL_ERROR "TEST FAILED" )
endif()
file( REMOVE_RECURSE "Run" )
