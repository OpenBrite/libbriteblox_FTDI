# Includes
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}
                     ${CMAKE_CURRENT_SOURCE_DIR}
                     )

# Version information
set(SNAPSHOT_VERSION "unknown")
execute_process(COMMAND git describe
                OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
                RESULT_VARIABLE GIT_DESCRIBE_RESULT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )
if(${GIT_DESCRIBE_RESULT} STREQUAL 0)
  set(SNAPSHOT_VERSION ${GIT_DESCRIBE_OUTPUT})
endif ()
message(STATUS "Detected git snapshot version: ${SNAPSHOT_VERSION}")

configure_file(briteblox_version_i.h.in "${CMAKE_CURRENT_BINARY_DIR}/briteblox_version_i.h" @ONLY)

# Targets
set(c_sources     ${CMAKE_CURRENT_SOURCE_DIR}/briteblox.c ${CMAKE_CURRENT_SOURCE_DIR}/briteblox_stream.c CACHE INTERNAL "List of c sources" )
set(c_headers     ${CMAKE_CURRENT_SOURCE_DIR}/briteblox.h CACHE INTERNAL "List of c headers" )

add_library(briteblox1 SHARED ${c_sources})

math(EXPR VERSION_FIXUP "${MAJOR_VERSION} + 1")    # Compatiblity with previous releases
set_target_properties(briteblox1 PROPERTIES VERSION ${VERSION_FIXUP}.${MINOR_VERSION}.0 SOVERSION 2)
# Prevent clobbering each other during the build
set_target_properties ( briteblox1 PROPERTIES CLEAN_DIRECT_OUTPUT 1 )


# Dependencies
target_link_libraries(briteblox1 ${LIBUSB_LIBRARIES})

install ( TARGETS briteblox1
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib${LIB_SUFFIX}
          ARCHIVE DESTINATION lib${LIB_SUFFIX}
        )

if ( STATICLIBS )
  add_library(briteblox1-static STATIC ${c_sources})
  target_link_libraries(briteblox1-static ${LIBUSB_LIBRARIES})
  set_target_properties(briteblox1-static PROPERTIES OUTPUT_NAME "briteblox1")
  set_target_properties(briteblox1-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
  install ( TARGETS briteblox1-static
            ARCHIVE DESTINATION lib${LIB_SUFFIX}
            COMPONENT staticlibs
          )
endif ()

install ( FILES ${c_headers}
          DESTINATION include/${PROJECT_NAME}
          COMPONENT headers
        )
