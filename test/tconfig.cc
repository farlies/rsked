/// Test the Config class
///
///    tconfig  --log_level=all

/*   Part of the rsked package.
 *
 *   Copyright 2020 Steven A. Harp
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


/// Dynamically link boost test framework
#define BOOST_TEST_MODULE config_test
#ifndef BOOST_TEST_DYN_LINK
#define BOOST_TEST_DYN_LINK 1
#endif
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


#include <iostream>
#include "logging.hpp"
#include "config.hpp"

/// Simple test fixture that just handles logging setup/teardown.
///
struct LogFixture {
    LogFixture() {
        init_logging("tconfig","tconfig_%5N.log",LF_FILE|LF_DEBUG);
    }
    ~LogFixture() {
        finish_logging();
    }
};

//////////////////////////////////////////////////////////////////////////

/// Do some tests on test file. All of these should return normally,
/// none should throw.
///
BOOST_AUTO_TEST_CASE( Valid_params_test )
{
    namespace fs = boost::filesystem;
    LogFixture lf;

    LOG_INFO(Lgr) << "Unit test: Valid_params_test";

    const char *confname = "../test/tconfig.json";
    Config cfg(confname);
    cfg.read_config();

    // Bool
    bool sdr_player_enabled {false};
    BOOST_TEST(cfg.get_bool("Sdr_player","enabled", sdr_player_enabled));
    BOOST_TEST(sdr_player_enabled);

    bool mp3_player_enabled {true};
    BOOST_TEST(cfg.get_bool("Mp3_player","enabled",mp3_player_enabled));
    BOOST_TEST(mp3_player_enabled == false);

    // Int
    int mpd_port=6600;
    BOOST_TEST(cfg.get_int("Mpd_player","mpd_port",mpd_port));
    BOOST_TEST(mpd_port == 6666);

    // Unsigned
    unsigned umpd_port=6600;
    BOOST_TEST(cfg.get_unsigned("Mpd_player","mpd_port",umpd_port));
    BOOST_CHECK_EQUAL(umpd_port,6666);

    unsigned unexpected_port = 1313;
    BOOST_TEST(not cfg.get_unsigned("Sdr_player","gqrx_port",unexpected_port));
    BOOST_CHECK_EQUAL(unexpected_port,1313);

    // Double
    double low_s=13.0;
    BOOST_TEST(cfg.get_double("Sdr_player","gqrx_low_s",low_s));
    BOOST_CHECK_EQUAL(low_s, -20.0);

    // Pathnames
    fs::path path1_expected  { "/usr/bin/whoami" };
    BOOST_TEST( fs::exists(path1_expected) );
    fs::path path1 {"/foo/bar"};
    BOOST_TEST(cfg.get_pathname("Ogg_player","ogg_bin_path",
                                FileCond::MustExist, path1));
    BOOST_CHECK_EQUAL(path1,path1_expected);

    fs::path path2_default("/bin/bash"); // this file should exist 
    BOOST_TEST( fs::exists(path2_default) );
    fs::path path2 {path2_default};
    BOOST_TEST(not cfg.get_pathname("NA_player","well_trodden_path",
                                    FileCond::MustExist, path2));
    BOOST_CHECK_EQUAL(path2,path2_default);

    // this path exists but must not; the key is not present in the JSON.
    fs::path validpath("tconfig");
    BOOST_CHECK_THROW(cfg.get_pathname("NA_player","well_trodden_path",
                                       FileCond::MustNotExist, validpath),
                      Config_path_error);

    //  the path specified in the JSON does not exist, should throw
    fs::path invalidpath {"/tmp"};
    BOOST_CHECK_THROW(cfg.get_pathname("NA_player","secret_path",
                                       FileCond::MustExist, invalidpath),
                      Config_path_error );
}


/// Parse the player_preference nested object in tconfig.json
///
BOOST_AUTO_TEST_CASE( Json_parse_test )
{
    LogFixture lf;
    LOG_INFO(Lgr) << "Unit test: Json_parse_test";
    const char *confname = "../test/tconfig.json";
    Config cfg(confname);
    cfg.read_config();

    Json::Value jppref = cfg.get_root()["player_preference"];
    BOOST_TEST(not jppref.isNull());
    BOOST_TEST( jppref.isObject() );
    for (auto medname : jppref.getMemberNames()) {
        LOG_INFO(Lgr) << medname << " :"; // * validate medium here
        auto jmed = jppref[medname];
        BOOST_TEST( jmed.isObject() );
        for (auto encname : jmed.getMemberNames()) {
            LOG_INFO(Lgr) << "    " << encname << ":"; // * validate enc here
            auto jenc = jmed[encname];
            BOOST_TEST( jenc.isArray() );
            unsigned i=0;
            for (auto j : jenc) {
                LOG_INFO(Lgr) << "       (" << i++ << ") " << j.asString();
            }
        }
    }
}
