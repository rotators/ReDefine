cmake_minimum_required( VERSION 3.14 FATAL_ERROR )

file( GLOB_RECURSE found_tests LIST_DIRECTORIES false RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS "*.test" )

enable_testing()
foreach( file IN LISTS found_tests )
    get_filename_component( name_dir "${file}" DIRECTORY )
    get_filename_component( name_wle "${file}" NAME_WLE )
    set( name "${name_dir}/${name_wle}" )
#    string( REGEX REPLACE "/ReDefine\\.test$" "" name "${file}" )
    message( STATUS "Add test... ${name}" )
    add_test( NAME ${name}
        COMMAND ${CMAKE_COMMAND} -DREDEFINE=$<TARGET_FILE:ReDefine> -DTEST_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${file} -P ${CMAKE_CURRENT_SOURCE_DIR}/Run.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

endforeach()

add_custom_target( ReDefine.Test
    DEPENDS ReDefine ${found_tests}
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
