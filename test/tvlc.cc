/// Test the VLC player class
///
///  ./tvlc --log_level=all


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
#include <memory>
#include <jsoncpp/json/json.h>

#include "logging.hpp"
#include "vlcplayer.hpp"
#include "config.hpp"
#include "fake_rsked.hpp"

namespace utf = boost::unit_test;
namespace bdata = boost::unit_test::data;
namespace fs = boost::filesystem;

/// HOME environment variable affects path resolution.
//
// char newhome[] = "HOME=/home/qrhacker";

namespace Main {
    // The (unique) rsked instance is sometimes needed by components to
    // retrieve environment information.  Do via Main::rsked->foo(...)
    std::unique_ptr<Rsked> rsked;

}

class Player_manager {
public:
    static bool inet_available();
};


bool Player_manager::inet_available() { return true; }


// proforma dtor
Player::~Player() { }


/// Simple test fixture that handles logging setup/teardown,
/// and creates an instance of the fake Rsked with its respathspec.
///
struct LogFixture {
    LogFixture() {
        init_logging("tvlc","tvlc_%2N.log",LF_FILE|LF_DEBUG |LF_CONSOLE);
        // putenv(newhome);
        Main::rsked = std::make_unique<Rsked>();
    }
    ~LogFixture() {
        finish_logging();
    }
};

BOOST_TEST_GLOBAL_FIXTURE(LogFixture);

/// Test sources in the test/Music/ directory
///
const char *Sources[6] ={
    // 0
    R"( { "encoding" : "ogg",   "medium" : "file",
            "repeat" : false,  "duration" : 43.5,
            "location" :  "Test/Oggfiles/1812.ogg" })",

    // 1
    R"( { "encoding" : "flac",   "medium" : "file",
            "repeat" : false,  "duration" : 5.6,
            "location" :  "Test/Flacfiles/biber_sonata.flac" })",

    // 2
    R"( { "encoding" : "mp3",   "medium" : "stream",
            "repeat" : true,  "duration" : 159,
            "location" :  "http://cms.stream.publicradio.org/cms.mp3" })",

    // 3
    R"( {"encoding" : "mp3", "medium" : "stream",
                  "location" : "https://stream.wqxr.org/wqxr-web",
                  "repeat" : true} )",
    // 4
    R"( {"encoding" : "mp3", "medium" : "playlist",
                  "location" : "begegnungen.m3u",
                  "repeat" : true} )",
    // 5
    R"( { "encoding" : "ogg",   "medium" : "directory",
            "repeat" : false,  "duration" : 35,
            "location" :  "Test/Dirtest" })",

};


/// Load a source from the Sources[] array of specifications
///
spSource load_test_source( unsigned k, const char*name )
{
    Json::CharReaderBuilder builder;
    Json::CharReader *reader = builder.newCharReader();
    std::string parserrs;
    Json::Value jvalue;
    BOOST_REQUIRE(reader->parse( Sources[k], Sources[k]+strlen(Sources[k]),
                                 &jvalue, &parserrs ));
    spSource spTestSrc = std::make_shared<Source>( name );
    BOOST_REQUIRE( spTestSrc );
    spTestSrc->load( jvalue );
    // next will throw a schedule error if the source is invalid
    spTestSrc->validate( *(Main::rsked->get_respathspec()) );
    return spTestSrc;
}

/// Verify src is being played for maxCycles (each 2 seconds)
///
void monitor_play( spPlayer vlcp, spSource src, unsigned maxCycles )
{
    for (unsigned i=0; i<maxCycles; i++ ) {
        sleep(2);
        if (not vlcp->currently_playing( src )) {
            LOG_WARNING(Lgr) << "Not playing " << src->name();
        }
    }
}

/// This should trigger a stall exception.
///
bool do_flac_stall( spPlayer vlcp )
{
    LOG_INFO(Lgr) << "Play a 6 second flac file for 20 seconds.";
    try {
        spSource spTestSrc1 { load_test_source(1,"testFlacFile") };
        spTestSrc1->set_quiet_okay(false);  // so we track progress
        vlcp->play( spTestSrc1 );
        monitor_play( vlcp, spTestSrc1, 10 );
    } catch( Player_media_exception & ) {
        LOG_INFO(Lgr) << "Caught expected exception.";
        vlcp->stop();
        return true;
    }
    vlcp->stop();
    return false;
}

/// Play a stream, trapping all exceptions.
///
bool do_mp3_stream( spPlayer vlcp, unsigned cycles )
{
    bool result = true;
    spSource spTestSrc { load_test_source(3,"testStreamQxr") };
    try {
        vlcp->play( spTestSrc );
        monitor_play( vlcp, spTestSrc, cycles );
    } catch( const std::exception &err ) {
        result = false;
    }
    vlcp->stop();
    return result;
}

/// Play a stream, trapping all exceptions.
///
bool do_playlist( spPlayer vlcp, unsigned cycles )
{
    bool result = true;
    spSource spTestSrc { load_test_source(4,"testPlaylist") };
    try {
        vlcp->play( spTestSrc );
        monitor_play( vlcp, spTestSrc, cycles );
    } catch( const std::exception &err ) {
        result = false;
    }
    vlcp->stop();
    return result;
}

/// Play a directory (all ogg), trapping all play time exceptions.
/// Return true if no issues were detected.
///
bool do_directory( spPlayer vlcp, unsigned cycles )
{
    bool result = true;
    spSource spTestSrc { load_test_source(5,"testDirectory") };
    try {
        vlcp->play( spTestSrc );
        monitor_play( vlcp, spTestSrc, cycles );
    } catch( const std::exception &err ) {
        result = false;
    }
    vlcp->stop();
    return result;
}



////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( vlc_playlist_test, * utf::disabled() )
{
    std::shared_ptr<const ResPathSpec> rps =Main::rsked->get_respathspec();
    BOOST_REQUIRE(rps);

    const char *confname = "../test/vlc.json";
    Config cfg(confname);
    cfg.read_config();

    spPlayer vlcp = std::make_shared<Vlc_player>();
    BOOST_REQUIRE( vlcp );
    vlcp->initialize( cfg, false );  // testp=false

    // Check status
    BOOST_TEST( vlcp->check() );

    // Playlist
    LOG_INFO(Lgr) << "Play a local playlist for 20 seconds.";
    BOOST_TEST( do_playlist(vlcp, 10) );

}

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( vlc_directory_test )
{
    std::shared_ptr<const ResPathSpec> rps =Main::rsked->get_respathspec();
    BOOST_REQUIRE(rps);

    const char *confname = "../test/vlc.json";
    Config cfg(confname);
    cfg.read_config();

    spPlayer vlcp = std::make_shared<Vlc_player>();
    BOOST_REQUIRE( vlcp );
    vlcp->initialize( cfg, false );  // testp=false

    // Check status
    BOOST_TEST( vlcp->check() );

    // Directory
    LOG_INFO(Lgr) << "Play a local directory for 36 seconds.";
    BOOST_TEST( do_directory( vlcp, 18 ) );

}

////////////////////////////////////////////////////////////////////////////////


BOOST_AUTO_TEST_CASE( vlc_play_pause_resume_test, * utf::disabled()   )
{
    std::shared_ptr<const ResPathSpec> rps =Main::rsked->get_respathspec();
    BOOST_REQUIRE(rps);

    const char *confname = "../test/vlc.json";
    Config cfg(confname);
    cfg.read_config();

    spPlayer vlcp = std::make_shared<Vlc_player>();
    BOOST_REQUIRE( vlcp );
    vlcp->initialize( cfg, false );  // testp=false

    // Check status
    BOOST_TEST( vlcp->check() );

    // Play 0 : OGG
    spSource spTestSrc0 { load_test_source(0,"testOgg") };
    spTestSrc0->set_quiet_okay(false);  // so we track progress

    LOG_INFO(Lgr) << "Play an Ogg file for 10 seconds.";
    vlcp->play( spTestSrc0 );
    monitor_play( vlcp, spTestSrc0, 5 );
    vlcp->pause();

    LOG_INFO(Lgr) << "Pause for 6 seconds";
    sleep(6);

    LOG_INFO(Lgr) << "Resume and play 10 seconds";
    vlcp->resume();
    monitor_play( vlcp, spTestSrc0, 5 );

    LOG_INFO(Lgr) << "Stop";
    vlcp->stop();

    // Play 2 : MP3 Stream
    LOG_INFO(Lgr) << "Play an internet stream for 40 seconds.";
    LOG_INFO(Lgr) << "You may try pulling the plug after it starts playing.";
    BOOST_TEST( do_mp3_stream(vlcp,20) );

    // Play 1 : FLAC
    BOOST_TEST( do_flac_stall(vlcp) );

    // Play 2 : MP3 Stream again
    LOG_INFO(Lgr) << "Play the internet stream again for 20 seconds.";
    BOOST_TEST( do_mp3_stream(vlcp,10) );

}
