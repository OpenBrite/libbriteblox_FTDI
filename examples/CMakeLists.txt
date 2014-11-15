option(EXAMPLES "Build example programs" ON)

if (EXAMPLES)
    # Includes
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
            )

    message(STATUS "Building example programs.")

    # Targets
    add_executable(simple simple.c)
    add_executable(bitbang bitbang.c)
    add_executable(bitbang2 bitbang2.c)
    add_executable(bitbang_cbus bitbang_cbus.c)
    add_executable(bitbang_ft2232 bitbang_ft2232.c)
    add_executable(find_all find_all.c)
    add_executable(serial_test serial_test.c)
    add_executable(baud_test baud_test.c)
    add_executable(stream_test stream_test.c)
    add_executable(eeprom eeprom.c)

    # Linkage
    target_link_libraries(simple briteblox1)
    target_link_libraries(bitbang briteblox1)
    target_link_libraries(bitbang2 briteblox1)
    target_link_libraries(bitbang_cbus briteblox1)
    target_link_libraries(bitbang_ft2232 briteblox1)
    target_link_libraries(find_all briteblox1)
    target_link_libraries(serial_test briteblox1)
    target_link_libraries(baud_test briteblox1)
    target_link_libraries(stream_test briteblox1)
    target_link_libraries(eeprom briteblox1)

    # libbriteblox++ examples
    if(BRITEBLOX_BUILD_CPP)
        if(Boost_FOUND)
            message(STATUS "Building libbriteblox++ examples.")
            include_directories(BEFORE ${CMAKE_SOURCE_DIR}/britebloxpp
                                ${Boost_INCLUDE_DIRS})

            # Target
            add_executable(find_all_pp find_all_pp.cpp)

            # Linkage
            target_link_libraries(find_all_pp britebloxpp1)
        endif(Boost_FOUND)
    endif(BRITEBLOX_BUILD_CPP)

    # Source includes
    include_directories(BEFORE ${CMAKE_SOURCE_DIR}/src)

else(EXAMPLES)
    message(STATUS "Not building example programs.")
endif(EXAMPLES)
