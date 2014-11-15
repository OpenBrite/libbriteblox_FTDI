/**@file
@brief Test basic BRITEBLOX functionality

@author Thomas Jarosch
*/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation;             *
 *                                                                         *
 ***************************************************************************/

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <briteblox.h>

BOOST_AUTO_TEST_SUITE(Basic)

BOOST_AUTO_TEST_CASE(SimpleInit)
{
    briteblox_context briteblox;

    int rtn_init = briteblox_init(&briteblox);
    BOOST_REQUIRE_EQUAL(0, rtn_init);

    briteblox_deinit(&briteblox);
}

BOOST_AUTO_TEST_SUITE_END()
