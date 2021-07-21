/// Test the Source class, run as:
///
///   tsrc --log_level=all


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
#include "source.hpp"

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>


namespace bdata = boost::unit_test::data;

//char newhome[] = "HOME=/home/qrhacker";

std::unique_ptr<ResPathSpec> Default_rps;

/// Simple test fixture that just handles logging setup/teardown.
///
struct LogFixture {
    LogFixture() {
        init_logging("tsrc","tsrc_%2N.log",LF_FILE|LF_DEBUG|LF_CONSOLE);
        // putenv(newhome);
        Default_rps = std::make_unique<ResPathSpec>();
    }
    ~LogFixture() {
        finish_logging();
    }
};

BOOST_TEST_GLOBAL_FIXTURE(LogFixture);


//////////////////////////////////////////////////////////////////////////


BOOST_AUTO_TEST_CASE( resolve_lib_path )
{
    namespace fs = boost::filesystem;
    {
        fs::path mpl_in { "Jon Hassell/Fascinoma/06-Secretly Happy.ogg" };
        fs::path mpl_out {};
        Default_rps->resolve_library(mpl_in, mpl_out);
        LOG_INFO(Lgr) << "resolve library " << mpl_in << "-> " << mpl_out;
        BOOST_TEST( mpl_out.is_absolute() );
    }
    //
    {
        fs::path apl_in { "/opt/Jon Hassell/Fascinoma/06-Secretly Happy.ogg" };
        fs::path apl_out {};
        Default_rps->resolve_playlist(apl_in, apl_out);
        LOG_INFO(Lgr) << "resolve library: " << apl_in << " -> " << apl_out;
        BOOST_TEST( apl_in == apl_out );
    }
}


BOOST_AUTO_TEST_CASE( alter_lib_path )
{
    namespace fs = boost::filesystem;
    LOG_INFO(Lgr) << "TEST alter_lib_path";
    const fs::path relpath {"motd.ogg"};
    fs::path newbase {"../resource"};
    fs::path abspath;
    ResPathSpec rps {};
    rps.set_library_base(newbase);
    rps.resolve_library( relpath, abspath );
    LOG_INFO(Lgr) << relpath << " => " << abspath;
    fs::path answer {"/home/sharp/projects/rsked/resource/motd.ogg"};
    BOOST_TEST( abspath == answer );
}



BOOST_AUTO_TEST_CASE( resolve_pl_path )
{
    namespace fs = boost::filesystem;
    {
        fs::path mpl_in { "master.m3u" };
        fs::path mpl_out {};
        Default_rps->resolve_playlist(mpl_in, mpl_out);
        LOG_INFO(Lgr) << "resolve playlist " << mpl_in << "-> " << mpl_out;
        BOOST_TEST( mpl_out.is_absolute() );
    }
    //
    {
        fs::path apl_in { "/opt/plists/master.m3u" };
        fs::path apl_out {};
        Default_rps->resolve_playlist(apl_in, apl_out);
        LOG_INFO(Lgr) << "resolve playlist: " << apl_in << " -> " << apl_out;
        BOOST_TEST( apl_in == apl_out );
    }
}

BOOST_AUTO_TEST_CASE( resolve_ann_path )
{
    namespace fs = boost::filesystem;
    {
        fs::path ran_in { "resource/snooze-1hr.ogg" };
        fs::path ran_out {};
        Default_rps->resolve_announcement(ran_in, ran_out);
        LOG_INFO(Lgr) << "resolve announcement " << ran_in << "-> " << ran_out;
        BOOST_TEST( ran_out.is_absolute() );
    }
    //
    {
        fs::path ann_in { "/opt/resource/snooze-1hr.ogg" };
        fs::path ann_out {};
        Default_rps->resolve_announcement(ann_in, ann_out);
        LOG_INFO(Lgr) << "resolve announcement: " << ann_in << " -> " << ann_out;
        BOOST_TEST( ann_in == ann_out );
    }
}

//////////////////////////////////////////////////////////////////////////

/// Valid parsable Sources as JSON strings.
///
const char* good_sources[] = {
    R"( {"encoding" : "wfm", "medium": "radio", "location" : 88.5,
         "alternate" : "master"} )",

    R"( {"encoding" : "mixed", "medium": "playlist", "repeat" : true,
         "location" : "master.m3u", "duration": 38253.213} )",

    R"( {"encoding" : "ogg", "location" : "Herman's Hermits/Retrospective",
         "medium": "directory", "repeat" : true, "duration": 3992.731} )",

    R"( {"encoding" : "ogg", "medium": "file", "duration": 1,
         "announcement" : true, "text" : "resuming program",
         "location" :  "resource/resuming.ogg" } )",
    
    R"( {"encoding" : "ogg", "medium": "file", "duration": 1,
         "announcement" : true, "text" : "snooze for one hour",
         "location" : "resource/snooze-1hr.ogg" }, )",

    R"( {"encoding" : "mp3", "medium": "stream",  "duration": 3600,
         "repeat" : false, "alternate" : "nis", "dynamic" : true,
         "location" : "https://traffic.libsyn.com/democracynow/dn%Y-%m%d.mp3"})",

    R"( {"encoding" : "hd1fm", "medium": "radio", "location" : 91.1,
         "alternate" : "master"} )",
    
    R"( {"encoding" : "hd2fm", "medium": "radio", "location" : 93.0,
         "alternate" : "master"} )",
    
    R"( {"encoding" : "hd3fm", "medium": "radio", "location" : 99.5,
         "alternate" : "master"} )",
    
    R"( {"encoding" : "hd4fm", "medium": "radio", "location" : 101.2,
         "alternate" : "master"} )",

    R"( {"encoding" : "nfm", "medium": "radio", "location" : 114.26,
         "alternate" : "none"} )"
};

/// Parse the string as a JSON Source spec. Pass the value to a new
/// Source, tstsrc to load. Describe it. Should not throw. Return true.
///
bool test_create( const char* src_str )
{
    namespace fs = boost::filesystem;
    std::string parserrs;
    Json::Value jvalue;
    Json::CharReaderBuilder builder;
    Json::CharReader *reader = builder.newCharReader();
    size_t n = strlen( src_str );
    if (reader->parse( src_str, src_str+n, &jvalue, &parserrs )) {
        Source tstsrc("testsrc");
        tstsrc.load( jvalue );
        tstsrc.describe();
        tstsrc.validate( *Default_rps );  // may throw
        if (tstsrc.localp()) {
            fs::path abspath {"(unknown)"};
            tstsrc.res_path( abspath );
            LOG_INFO(Lgr) << "validated path: " << abspath;
        }
        delete reader;
        return true;
    }
    LOG_ERROR(Lgr) << "parse error: " << parserrs << '\n';
    delete reader;
    return false;
}


/// Parse some valid Source JSON
///
BOOST_DATA_TEST_CASE( Valid_json, bdata::make(good_sources), src_str )
{
    BOOST_TEST( test_create(src_str) );
}
