/// Test the ResPathSpec class
///
///    trespath  --log_level=all

/*   Part of the rsked package.
 *
 *   Copyright 2021 Steven A. Harp
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

/***   Note: tests  currently will only work for user 'sharp'
 ***/

/// Dynamically link boost test framework
#define BOOST_TEST_MODULE respath_test
#ifndef BOOST_TEST_DYN_LINK
#define BOOST_TEST_DYN_LINK 1
#endif
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


#include <iostream>
#include "logging.hpp"
#include "respath.hpp"

/// Simple test fixture that just handles logging setup/teardown.
///
struct LogFixture {
    LogFixture() {
        init_logging("trespath","trespath_%5N.log",LF_CONSOLE|LF_DEBUG);
    }
    ~LogFixture() {
        finish_logging();
    }
};

BOOST_TEST_GLOBAL_FIXTURE(LogFixture);

namespace fs = boost::filesystem;

//////////////////////////////////////////////////////////////////////////

/// Do some tests on test file. All of these should return normally,
/// none should throw.
///
BOOST_AUTO_TEST_CASE( Defaults_test )
{
    LOG_INFO(Lgr) << "Unit test: Defaults_test";

    ResPathSpec rps;

    fs::path lib_default { "/home/sharp/Music/" };

    BOOST_CHECK_EQUAL( rps.get_libpath(), lib_default );

}


/// Do some tests on test file. All of these should return normally,
/// none should throw.
///
BOOST_AUTO_TEST_CASE( Set_home_test )
{
    LOG_INFO(Lgr) << "Unit test: Set_home_test";

    ResPathSpec rps;

    // will set base paths to some real but innappropriate directories
    
    {
        fs::path path_intended { "/home/sharp/logs" };
        rps.set_library_base("~/logs/");
        BOOST_CHECK_EQUAL( rps.get_libpath(), path_intended );
    }

    {
        fs::path path_intended { "/home/sharp/logs_old" };
        rps.set_playlist_base("~/logs_old");
        BOOST_CHECK_EQUAL( rps.get_playlistpath(), path_intended );
    }

    {
        fs::path path_intended { "/home/sharp/bin" };
        rps.set_announcement_base("~/bin/");
        BOOST_CHECK_EQUAL( rps.get_announcepath(), path_intended );
    }
}

/// All of these should throw a fs::filesystem_error
///
BOOST_AUTO_TEST_CASE( Set_test_bad )
{
    LOG_INFO(Lgr) << "Unit test: Set_test_bad";
    ResPathSpec rps;

    BOOST_CHECK_THROW( rps.set_library_base("~/vomit1"),
        boost::filesystem::filesystem_error ); // bad

    BOOST_CHECK_THROW( rps.set_announcement_base("~/vomit2"),
        boost::filesystem::filesystem_error ); // bad

    BOOST_CHECK_THROW( rps.set_playlist_base("~/vomit3"),
        boost::filesystem::filesystem_error ); // bad
}


