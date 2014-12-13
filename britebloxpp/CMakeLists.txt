# Check
set(BRITEBLOX_BUILD_CPP False PARENT_SCOPE)

option ( BRITEBLOXPP "Build C++ binding library libbriteblox1++" ON )

# Targets
set(cpp_sources   ${CMAKE_CURRENT_SOURCE_DIR}/briteblox.cpp CACHE INTERNAL "List of cpp sources" )
set(cpp_headers   ${CMAKE_CURRENT_SOURCE_DIR}/briteblox.hpp CACHE INTERNAL "List of cpp headers" )
        
if (BRITEBLOXPP)

  if(Boost_FOUND)

    # Includes
    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}
                        ${CMAKE_CURRENT_SOURCE_DIR}
                        ${CMAKE_SOURCE_DIR}/src)

    include_directories(${Boost_INCLUDE_DIRS})



    set(BRITEBLOX_BUILD_CPP True PARENT_SCOPE)
    message(STATUS "Building libbriteblox1++")

    # Shared library
    add_library(britebloxpp1 SHARED ${cpp_sources})

    math(EXPR VERSION_FIXUP "${MAJOR_VERSION} + 1")    # Compatiblity with previous releases
    set_target_properties(britebloxpp1 PROPERTIES VERSION ${VERSION_FIXUP}.${MINOR_VERSION}.0 SOVERSION 2)

    # Prevent clobbering each other during the build
    set_target_properties(britebloxpp1 PROPERTIES CLEAN_DIRECT_OUTPUT 1)

    # Dependencies
    target_link_libraries(britebloxpp1 briteblox1 ${LIBUSB_LIBRARIES} ${BOOST_LIBRARIES})

    
    install ( TARGETS britebloxpp1
              RUNTIME DESTINATION bin
              LIBRARY DESTINATION lib${LIB_SUFFIX}
              ARCHIVE DESTINATION lib${LIB_SUFFIX}
            )
            
    # Static library
    if ( STATICLIBS )
      add_library(britebloxpp1-static STATIC ${cpp_sources})
      set_target_properties(britebloxpp1-static PROPERTIES OUTPUT_NAME "britebloxpp1")
      set_target_properties(britebloxpp1-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

      install ( TARGETS britebloxpp1-static
                ARCHIVE DESTINATION lib${LIB_SUFFIX}
                COMPONENT staticlibs
              )
    endif ()

    install ( FILES ${cpp_headers}
              DESTINATION include/${PROJECT_NAME}
              COMPONENT headers
            )

  else ()
    message(STATUS "Boost not found, won't build libbriteblox1++")
  endif ()

else ()
  message(STATUS "Not building libbriteblox1++")
endif ()
