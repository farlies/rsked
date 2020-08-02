/// Test the Schedule class, run as:
///
///    tsked  --log_level=all


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
#define BOOST_TEST_MODULE sked_test
#ifndef BOOST_TEST_DYN_LINK
#define BOOST_TEST_DYN_LINK 1
#endif
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include "schedule.hpp"


namespace bdata = boost::unit_test::data;


/// Simple test fixture that just handles logging setup/teardown.
///
struct LogFixture {
    LogFixture() {
        init_logging("tsked","tsked_%5N.log",LF_FILE|LF_DEBUG);
    }
    ~LogFixture() {
        finish_logging();
    }
};

BOOST_TEST_GLOBAL_FIXTURE(LogFixture);

/// hack to fill a time structure for testing play_daytime
///
struct tm *mktime(int yday, int wday, int hour, int min, int sec=0 )
{
    static struct tm stm;
    stm.tm_sec =  sec;
    stm.tm_min = min;
    stm.tm_hour = hour;
    stm.tm_wday = wday;
    stm.tm_yday = yday;
    return &stm;
}

/// Helper for test0. Check program played at given time.
/// Expect a slot named esource.  Any announcement returned
/// is marked as complete.
/// Return false if error, otherwise true.
///
static int test_time( Schedule &sched, int yday, int wday, int hour,
                      int min, int sec, const char* esource )
{
    struct tm  *pstm = mktime( yday, wday, hour, min, sec );
    try {
        spPlay_slot s { sched.play_daytime( pstm ) };

        LOG_INFO(Lgr) << DayNames[wday] << "  "
                      << std::setfill('0') << std::setw(2) << hour << ":"
                      << std::setfill('0') << std::setw(2) << min << ":"
                      << std::setfill('0') << std::setw(2) << sec
                      << "   doy=" << yday << "  Play: " << s->name()
                      << "  src=" << s->source()->name();
        if (s->is_announcement()) {
            if (s->is_complete(yday)) {
                LOG_ERROR(Lgr) << "Schedule returned completed announcement";
                return false;
            } else {
                LOG_INFO(Lgr) << "Marking as complete for day " << yday;
                s->set_complete(yday);
            }
        }
        if (s->name() != esource) {
            LOG_ERROR(Lgr) << "Expected source: " << esource;
            return false;
        }
    }
    catch( const std::exception &err ) {
        LOG_ERROR(Lgr) << DayNames[wday] << "  "
                      << std::setfill('0') << std::setw(2) << hour << ":"
                      << std::setfill('0') << std::setw(2) << min << ":"
                      << std::setfill('0') << std::setw(2) << sec
                      << "   doy=" << yday << ": " << err.what();
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////

/// Valid baseline schedule for testing, includes a rich set of features.
constexpr const char* TestSchedule1 = "../test/sked-test1.json";

/// Bad schedules array--each should fail on load.
///
const char* badskeds[] = {
    "../test/sked-test2.json",
    "../test/sked-test3.json",
    "../test/sked-test4.json",
    "../test/sked-test5.json",
    "../test/sked-test6.json"
};

/// Load should throw. Schedule should be marked as not valid.
///
BOOST_DATA_TEST_CASE( Sked_defect, bdata::make(badskeds), skedname )
{
    Schedule sched;
    LOG_INFO(Lgr) << "Unit test: Sked_defect " << skedname;
    BOOST_CHECK_THROW( sched.load(skedname), Schedule_error );
    BOOST_TEST(false == sched.valid());
}


/// For time Mon 15:15:00, get the source (cms).
/// Mark it as failed.
/// Get again and expect the alternate source: ksjn
/// Mark it as failed.
/// Get again and expect the alternate source: master
///
BOOST_AUTO_TEST_CASE( Source_fail_test )
{
    Schedule sched;
    LOG_INFO(Lgr) << "Unit test: Source_fail_test";

    sched.load(TestSchedule1);

    struct tm  *pstm = mktime( 120, Mon, 15, 15, 0 );
    spPlay_slot slot { sched.play_daytime( pstm ) };
    spSource src = slot->source();
    BOOST_TEST( src->name() == "cms" );

    src->mark_failed();
    src->describe();
    BOOST_TEST(src->failedp());
    //
    slot = sched.play_daytime( pstm );
    spSource src2 = slot->source();
    BOOST_TEST( src2->name() == "ksjn" );
    //
    src2->mark_failed();
    src2->describe();
    BOOST_TEST(src2->failedp());
    //
    slot = sched.play_daytime( pstm );
    spSource src3 = slot->source();
    BOOST_TEST( src3->name() == "master" );
}


/// Probe particular times of day. Return error count.
///
BOOST_AUTO_TEST_CASE( Time_probe )
{
    Schedule sched;
    LOG_INFO(Lgr) << "Unit test: Time_probe";
    sched.load(TestSchedule1);

    BOOST_TEST(test_time( sched,  120,  Mon,  0, 0, 0, "OFF" ));
    BOOST_TEST(test_time( sched,  120,  Mon,  7,29,59, "OFF" ));
    BOOST_TEST(test_time( sched,  120,  Mon,  7,30,00, "cms" ));
    BOOST_TEST(test_time( sched,  120,  Mon,  9,00,01, "motd-ymd" ));
    BOOST_TEST(test_time( sched,  120,  Mon,  9,30,00, "cms" ));
    BOOST_TEST(test_time( sched,  120,  Mon, 12,00,00, "dnow" ));
    BOOST_TEST(test_time( sched,  120,  Mon, 14,00,00, "nis" ));
    BOOST_TEST(test_time( sched,  120,  Mon, 15,00,00, "cms" ));
    BOOST_TEST(test_time( sched,  120,  Mon, 15,30,02, "motd-ymd" ));
    BOOST_TEST(test_time( sched,  120,  Mon, 15,30,03, "cms" ));
    BOOST_TEST(test_time( sched,  120,  Mon, 21,00,00, "OFF" ));
    BOOST_TEST(test_time( sched,  120,  Mon, 23,59,59, "OFF" ));
    // Weds
    BOOST_TEST(test_time( sched,  122,  Wed,  9,00,01, "motd-ymd" ));
}


/// Probe particular times of day. Return error count.
///
BOOST_AUTO_TEST_CASE( Multi_announce )
{
    Schedule sched;
    LOG_INFO(Lgr) << "Unit test: Multi_announce";
    sched.load(TestSchedule1);

    // simulate starting up after 2 consecutive announcements missed
    BOOST_TEST(test_time( sched,  119,  Sun, 15,50,00, "motd-ymd" ));
    BOOST_TEST(test_time( sched,  119,  Sun, 15,51,00, "cms" ));
    BOOST_TEST(test_time( sched,  119,  Sun, 20,20,05, "cms" ));
    BOOST_TEST(test_time( sched,  119,  Sun, 21,00,00, "OFF" ));
    BOOST_TEST(test_time( sched,  119,  Sun, 23,59,59, "OFF" ));
}
