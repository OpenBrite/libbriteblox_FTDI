
option(BRITEBLOX_EEPROM "Build briteblox_eeprom" ON)

if ( BRITEBLOX_EEPROM )
  find_package ( Confuse )
  find_package ( Libintl )
else(BRITEBLOX_EEPROM)
  message(STATUS "briteblox_eeprom build is disabled")
endif ()


if ( CONFUSE_FOUND )
  message(STATUS "Building briteblox_eeprom")

  include_directories ( ${CONFUSE_INCLUDE_DIRS} )
  list ( APPEND libs ${CONFUSE_LIBRARIES} )
  
  if ( LIBINTL_FOUND )
    include_directories ( ${LIBINTL_INCLUDE_DIR} )
    list ( APPEND libs ${LIBINTL_LIBRARIES} )
  endif ()

  
  # Version defines
  set ( EEPROM_MAJOR_VERSION 0 )
  set ( EEPROM_MINOR_VERSION 17 )
  set ( EEPROM_VERSION_STRING ${EEPROM_MAJOR_VERSION}.${EEPROM_MINOR_VERSION} )

  include_directories ( BEFORE ${CMAKE_SOURCE_DIR}/src )
  include_directories ( BEFORE ${CMAKE_CURRENT_BINARY_DIR} )

  configure_file(
    briteblox_eeprom_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/briteblox_eeprom_version.h
  )

  add_executable ( briteblox_eeprom main.c )
  target_link_libraries ( briteblox_eeprom briteblox1 ${CONFUSE_LIBRARIES} )
  if ( LIBINTL_FOUND )
    target_link_libraries ( briteblox_eeprom ${LIBINTL_LIBRARIES} )
  endif ()
  install ( TARGETS briteblox_eeprom DESTINATION bin )
else ()
  message ( STATUS "libConfuse not found, won't build briteblox_eeprom" )
endif ()

