cmake_minimum_required ( VERSION 2.8 )

project ( example C )

find_package ( LibBriteBlox1 NO_MODULE REQUIRED )
include ( ${LIBBRITEBLOX_USE_FILE} )

add_executable ( example main.c )
target_link_libraries( example ${LIBBRITEBLOX_LIBRARIES} )

install ( TARGETS example
          DESTINATION bin )
          
