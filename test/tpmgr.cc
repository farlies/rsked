/// Test the Player_manager class, run as:
///
///   tpmgr --log_level=all


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
#define BOOST_TEST_MODULE source_test
#ifndef BOOST_TEST_DYN_LINK
#define BOOST_TEST_DYN_LINK 1
#endif
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <cstring>
#include <memory>

#include "logging.hpp"
#include "playermgr.hpp"
#include "util/config.hpp"

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>


namespace bdata = boost::unit_test::data;

/// HOME environment variable affects path resolution.
//
// char newhome[] = "HOME=/home/qrhacker";

std::unique_ptr<ResPathSpec> Default_rps;

/// Simple test fixture that just handles logging setup/teardown.
///
struct LogFixture {
    LogFixture() {
        init_logging("tpmgr","tpmgr_%2N.log",LF_FILE|LF_DEBUG);  // |LF_CONSOLE);
        // putenv(newhome);
        Default_rps = std::make_unique<ResPathSpec>();
    }
    ~LogFixture() {
        finish_logging();
    }
};

BOOST_TEST_GLOBAL_FIXTURE(LogFixture);



/// Use string src_str to configure Source sp.
///
bool src_init( spSource &sp, const char* src_str )
{
    std::string parserrs;
    Json::Value jvalue;
    Json::CharReaderBuilder builder;
    Json::CharReader *reader = builder.newCharReader();
    size_t n = strlen( src_str );
    if (reader->parse( src_str, src_str+n, &jvalue, &parserrs )) {
        sp->load( jvalue );
        delete reader;
        return true;
    }
    LOG_ERROR(Lgr) << "parse error: " << parserrs << '\n';
    delete reader;
    return false;
}


//////////////////////////////////////////////////////////////////////////


BOOST_AUTO_TEST_CASE( config_pmgr_default )
{
    const char *confname = "../test/tpmgr.json";
    Config cfg(confname);
    cfg.read_config();      // might throw
    cfg.log_about();

    Player_manager pmgr {};
    pmgr.configure( cfg,  true ); // (testp) might throw

    /// Check annunciator access:
    {
        spPlayer pann = pmgr.get_annunciator();
        BOOST_TEST( pann );
        BOOST_TEST( pann->name() == "Annunciator" );
        BOOST_TEST( pann->has_cap(Medium::file,Encoding::ogg) );
        BOOST_TEST( pann->is_usable() );
    }

    /// Check precedence:  directory:ogg
    {
        std::string test_src {"OggDirSrc"};
        const char *src_json =
          R"( {"encoding" : "ogg", "location" : "Herman's Hermits/Retrospective",
             "medium": "directory", "repeat" : true, "duration": 3992.731} )";

        // 1. using default preferences we should get the Mpd player...
        spSource sp_src = std::make_shared<Source>(test_src);
        BOOST_TEST( src_init( sp_src, src_json ) );
        sp_src->describe();
        spPlayer player1 = pmgr.get_player(sp_src);
        BOOST_REQUIRE( player1 );
        BOOST_TEST( player1->has_cap(Medium::directory,Encoding::ogg) );
        BOOST_TEST( player1->is_usable() );
        BOOST_TEST( player1->name() == "Mpd_player" );

        // 2. Disable Mpd_player and fetch again: should get Ogg_player
        player1->set_enabled(false);
        BOOST_TEST( player1->is_enabled() == false );
        spPlayer player2 = pmgr.get_player(sp_src);
        BOOST_REQUIRE( player2 );
        BOOST_TEST( player2->has_cap(Medium::directory,Encoding::ogg) );
        BOOST_TEST( player2->is_usable() );
        BOOST_TEST( player2->name() == "Ogg_player" );

        // 3? Disable Ogg_player and fetch again: should get nullptr
        player2->set_enabled(false);
        BOOST_TEST( player2->is_enabled() == false );
        spPlayer player3 = pmgr.get_player(sp_src);
        BOOST_REQUIRE( ! player3 );

        // 1a. Reenable Mpd_player and fetch again: should get Mpd_player
        player1->set_enabled(true);
        BOOST_TEST( player1->is_enabled() );
        BOOST_TEST( player1->is_usable() );
        spPlayer player1a = pmgr.get_player(sp_src);
        BOOST_TEST( player1a == player1 );
    }

    // TODO : more tests
}


//////////////////////////////////////////////////////////////////////////

/// This has user preference customization.

BOOST_AUTO_TEST_CASE( config_pmgr_custom )
{
    const char *confname = "../test/tpmgr2.json";
    Config cfg(confname);
    cfg.read_config();      // might throw
    cfg.log_about();

    Player_manager pmgr {};
    pmgr.configure( cfg,  true ); // (testp=true); might throw

    /// Check precedence:  directory:ogg
    {
        std::string test_src {"OggDirSrc"};
        const char *src_json =
          R"( {"encoding" : "ogg", "location" : "Herman's Hermits/Retrospective",
             "medium": "directory", "repeat" : true, "duration": 3992.731} )";

        // 1. using custom preferences we should get the Ogg player...
        spSource sp_src = std::make_shared<Source>(test_src);
        BOOST_TEST( src_init( sp_src, src_json ) );
        sp_src->describe();
        spPlayer player1 = pmgr.get_player(sp_src);
        BOOST_REQUIRE( player1 );
        BOOST_TEST( player1->has_cap(Medium::directory,Encoding::ogg) );
        BOOST_TEST( player1->is_usable() );
        BOOST_TEST( player1->name() == "Ogg_player" );

        // 2. Disable Ogg_player and fetch again: should get Mpd_player
        player1->set_enabled(false);
        BOOST_TEST( player1->is_enabled() == false );
        spPlayer player2 = pmgr.get_player(sp_src);
        BOOST_REQUIRE( player2 );
        BOOST_TEST( player2->has_cap(Medium::directory,Encoding::ogg) );
        BOOST_TEST( player2->is_usable() );
        BOOST_TEST( player2->name() == "Mpd_player" );

        // 3? Disable Mpd_player and fetch again: should get nullptr
        player2->set_enabled(false);
        BOOST_TEST( player2->is_enabled() == false );
        spPlayer player3 = pmgr.get_player(sp_src);
        BOOST_REQUIRE( ! player3 );
    }

    // TODO : more tests
}

//////////////////////////////////////////////////////////////////////////

