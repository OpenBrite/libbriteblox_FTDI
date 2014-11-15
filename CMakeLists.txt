# Project
project(libbriteblox1)
set(MAJOR_VERSION 1)
set(MINOR_VERSION 1)
set(PACKAGE libbriteblox1)
set(VERSION_STRING ${MAJOR_VERSION}.${MINOR_VERSION})
set(VERSION ${VERSION_STRING})
set ( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )

# CMake
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
   set(CMAKE_BUILD_TYPE     RelWithDebInfo)
endif("${CMAKE_BUILD_TYPE}" STREQUAL "")
set(CMAKE_COLOR_MAKEFILE ON)
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

add_definitions(-Wall)

# Debug build
message("-- Build type: ${CMAKE_BUILD_TYPE}")
if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
   add_definitions(-DDEBUG)
endif(${CMAKE_BUILD_TYPE} STREQUAL Debug)

# find libusb
find_package ( USB1 REQUIRED )
include_directories ( ${LIBUSB_INCLUDE_DIR} )

# Find Boost (optional package)
find_package(Boost)

# Set components
set(CPACK_COMPONENTS_ALL sharedlibs staticlibs headers)
set(CPACK_COMPONENT_SHAREDLIBS_DISPLAY_NAME "Shared libraries")
set(CPACK_COMPONENT_STATICLIBS_DISPLAY_NAME "Static libraries")
set(CPACK_COMPONENT_HEADERS_DISPLAY_NAME "C++ Headers")

set(CPACK_COMPONENT_SHAREDLIBS_DESCRIPTION
"Shared library for general use.")
set(CPACK_COMPONENT_STATICLIBS_DESCRIPTION
"Static library, good if you want to embed libbriteblox1 in your application.")
set(CPACK_COMPONENT_HEADERS_DESCRIPTION
"C/C++ header files.")

set(CPACK_COMPONENT_SHAREDLIBS_GROUP "Development")
set(CPACK_COMPONENT_STATICLIBS_GROUP "Development")
set(CPACK_COMPONENT_HEADERS_GROUP    "Development")

option ( STATICLIBS "Build static libraries" ON )

# guess LIB_SUFFIX, don't take debian multiarch into account 
if ( NOT DEFINED LIB_SUFFIX )
  if( CMAKE_SYSTEM_NAME MATCHES "Linux"
      AND NOT CMAKE_CROSSCOMPILING
      AND NOT EXISTS "/etc/debian_version"
      AND NOT EXISTS "/etc/arch-release" )
    if ( "${CMAKE_SIZEOF_VOID_P}" EQUAL "8" )
      set ( LIB_SUFFIX 64 )
    endif ()
  endif ()
endif ()

if(NOT APPLE)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      SET(PACK_ARCH "")
    else(CMAKE_SIZEOF_VOID_P EQUAL 8)
      SET(PACK_ARCH .x86_64)
  endif(CMAKE_SIZEOF_VOID_P EQUAL 4)
else(NOT APPLE)
  SET(PACK_ARCH "")
endif(NOT APPLE)

# Package information
set(CPACK_PACKAGE_VERSION              ${VERSION_STRING})
set(CPACK_PACKAGE_CONTACT              "Intra2net AG <libbriteblox@developer.intra2net.com>")
set(CPACK_PACKAGE_DESCRIPTION          "libbriteblox1 library.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY  ${CPACK_PACKAGE_DESCRIPTION}
                                       )
# Package settings
if ( UNIX )
  set(CPACK_GENERATOR                    "DEB;RPM")
  set(CPACK_CMAKE_GENERATOR              "Unix Makefiles")
  set(CPACK_PACKAGE_NAME                 ${PROJECT_NAME})
  set(CPACK_PACKAGE_FILE_NAME            ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${PACK_ARCH})
endif ()

if ( WIN32 )
  set ( CPACK_GENERATOR                    "NSIS" )
  set ( CPACK_CMAKE_GENERATOR              "MinGW Makefiles" )
  set ( CPACK_PACKAGE_NAME                 "${PROJECT_NAME}" )
  set ( CPACK_PACKAGE_VENDOR               "" )
  set ( CPACK_PACKAGE_INSTALL_DIRECTORY    "libbriteblox1" )
  set ( CPACK_PACKAGE_FILE_NAME            "${PROJECT_NAME}-${VERSION_STRING}-win32")
  set ( CPACK_NSIS_DISPLAY_NAME            "libbriteblox1" )
  set ( CPACK_NSIS_MODIFY_PATH             ON )
endif ()

set(CPACK_RESOURCE_FILE_LICENSE        ${CMAKE_SOURCE_DIR}/LICENSE)

set(CPACK_SOURCE_GENERATOR             TGZ)
set(CPACK_SOURCE_IGNORE_FILES          "\\\\.git;~$;build/")
set(CPACK_SOURCE_PACKAGE_FILE_NAME     ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION})

# Subdirectories
if ( UNIX )
  set ( CPACK_SET_DESTDIR ON )
endif ()

# "make dist" target
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${VERSION_STRING})
add_custom_target(dist
    COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
            | bzip2 > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})


# Documentation
option ( DOCUMENTATION "Generate API documentation with Doxygen" ON )


find_package ( Doxygen )
if ( DOCUMENTATION AND DOXYGEN_FOUND )

   # Find doxy config
   message(STATUS "Doxygen found.")

   # Copy doxy.config.in
   set(top_srcdir ${CMAKE_SOURCE_DIR})
   configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile )
   configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.xml.in ${CMAKE_BINARY_DIR}/Doxyfile.xml )

   # Run doxygen
   add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/html/index.html
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
    DEPENDS ${c_headers};${c_sources};${cpp_sources};${cpp_headers}
   )

   add_custom_target(docs ALL DEPENDS ${CMAKE_BINARY_DIR}/doc/html/index.html)

   message(STATUS "Generating API documentation with Doxygen")
else(DOCUMENTATION AND DOXYGEN_FOUND)
   message(STATUS "Not generating API documentation")
endif(DOCUMENTATION AND DOXYGEN_FOUND)

add_subdirectory(src)
add_subdirectory(britebloxpp)
add_subdirectory(python)
add_subdirectory(briteblox_eeprom)
add_subdirectory(examples)
add_subdirectory(packages)
add_subdirectory(test)

# PkgConfig
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX}/bin)
set(includedir  ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME})

if(${UNIX})
   set(libdir      ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})
endif(${UNIX})
if(${WIN32})
   set(libdir      ${CMAKE_INSTALL_PREFIX}/bin)
endif(${WIN32})

configure_file(${CMAKE_SOURCE_DIR}/libbriteblox1.spec.in ${CMAKE_BINARY_DIR}/libbriteblox1.spec @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/libbriteblox1.pc.in ${CMAKE_BINARY_DIR}/libbriteblox1.pc @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/libbritebloxpp1.pc.in ${CMAKE_BINARY_DIR}/libbritebloxpp1.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/libbriteblox1.pc ${CMAKE_BINARY_DIR}/libbritebloxpp1.pc
        DESTINATION lib${LIB_SUFFIX}/pkgconfig)

if (UNIX OR MINGW)
  configure_file ( libbriteblox1-config.in ${CMAKE_CURRENT_BINARY_DIR}/libbriteblox1-config @ONLY )
  install ( PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/libbriteblox1-config
            DESTINATION bin )
endif ()

# config script install path
if ( NOT DEFINED LIBBRITEBLOX_CMAKE_CONFIG_DIR )
  set ( LIBBRITEBLOX_CMAKE_CONFIG_DIR lib${LIB_SUFFIX}/cmake/libbriteblox1 )
endif ()

set ( LIBBRITEBLOX_INCLUDE_DIR ${includedir} )
set ( LIBBRITEBLOX_INCLUDE_DIRS ${LIBBRITEBLOX_INCLUDE_DIR} )
set ( LIBBRITEBLOX_LIBRARY briteblox1 )
set ( LIBBRITEBLOX_LIBRARIES ${LIBBRITEBLOX_LIBRARY} )
list ( APPEND LIBBRITEBLOX_LIBRARIES ${LIBUSB_LIBRARIES} )
set ( LIBBRITEBLOX_STATIC_LIBRARY briteblox1.a )
set ( LIBBRITEBLOX_STATIC_LIBRARIES ${LIBBRITEBLOX_STATIC_LIBRARY} )
list ( APPEND LIBBRITEBLOX_STATIC_LIBRARIES ${LIBUSB_LIBRARIES} )
if (BRITEBLOX_BUILD_CPP)
  set ( LIBBRITEBLOXPP_LIBRARY briteblox1pp )
  set ( LIBBRITEBLOXPP_LIBRARIES ${LIBBRITEBLOXPP_LIBRARY} )
  list ( APPEND LIBBRITEBLOXPP_LIBRARIES ${LIBUSB_LIBRARIES} )
endif ()
set ( LIBBRITEBLOX_LIBRARY_DIRS ${libdir} )
set ( LIBBRITEBLOX_ROOT_DIR ${prefix} )
set ( LIBBRITEBLOX_VERSION_STRING ${VERSION_STRING} )
set ( LIBBRITEBLOX_VERSION_MAJOR ${MAJOR_VERSION} )
set ( LIBBRITEBLOX_VERSION_MINOR ${MINOR_VERSION} )

set ( LIBBRITEBLOX_USE_FILE ${CMAKE_INSTALL_PREFIX}/${LIBBRITEBLOX_CMAKE_CONFIG_DIR}/UseLibBriteBlox1.cmake )
configure_file ( cmake/LibBriteBlox1Config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/LibBriteBlox1Config.cmake @ONLY )
install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/LibBriteBlox1Config.cmake cmake/UseLibBriteBlox1.cmake
          DESTINATION ${LIBBRITEBLOX_CMAKE_CONFIG_DIR}
        )

include(CPack)
