/**
 * Test the MPD client
 *
 * This is a manual test, and requires user interaction to listen for
 * the correct audio output. It does not use the boost test framework.
 */

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

#include "logging.hpp"
#include "mpdclient.hpp"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;


/// an mp4 file
std::string g_reptiles { "Brian Eno/Another Green World/08 - Sombre Reptiles.m4a" };

/// a flac file
std::string g_track4 { "Dub Apocalypse/Live at 3S Artspace/03 Track04.flac" };

/// a directory with mp4 tracks
std::string g_agworld { "Brian Eno/Another Green World" };

/// a playlist (.m3u)  Any extension like .m3u will be stripped for mpd.
std::string g_master { "master.m3u" };

///////////////////////////////////////////////////////////////////////////////////

void log_last_err( Mpd_client &client )
{
    switch( client.last_err() ) {
    case Mpd_err::NoError:
        LOG_INFO(Lgr) << "MPD no error";
        break;
    case Mpd_err::NoConnection:
        LOG_ERROR(Lgr) << "MPD could not connect";
        break;
    case Mpd_err::NoStatus:
        LOG_ERROR(Lgr) << "MPD did not receive a status response";
        break;
    case Mpd_err::NoExist:
        LOG_ERROR(Lgr) << "MPD could not access the resource";
        break;
    default:
        LOG_ERROR(Lgr) << "MPD unknown Mpd_err";
    }
}


void test1( Mpd_client &client,  const std::string &resource )
{
    client.connect();
    //
    client.stop();
    client.clear_queue();
    client.set_repeat_mode(false);
    client.enqueue( resource );
    client.check_status(Mpd_opt::Print);
    if (client.last_err() == Mpd_err::NoError) {
        client.play();
        sleep(10);
        client.check_status(Mpd_opt::Print);
        client.stop();
    }
    //
    client.disconnect();
}


void testp( Mpd_client &client,  const std::string &plist )
{
    // strip any trailing extension, like .m3u, if present
    fs::path plfile { plist };
    std::string plstem { plfile.stem().native() };
    LOG_DEBUG(Lgr) << "playlist stem: '" << plstem << "'";
    client.connect();
    //
    client.stop();
    client.clear_queue();
    client.set_repeat_mode(false);
    client.enqueue_playlist( plstem );
    client.check_status(Mpd_opt::Print);
    if (client.last_err() == Mpd_err::NoError) {
        client.play();
        sleep(10);
        client.check_status(Mpd_opt::Print);
        client.stop();
    }
    //
    client.disconnect();
}


int main(int, char **)
{
    init_logging("mpdtest","mpdtest_%5N.log",LF_FILE|LF_DEBUG|LF_CONSOLE);
    //
    Mpd_client  client {};
    try {
        // test1( client, g_reptiles );   // .mp4
        // test1( client, g_track4 );     // .flac
        // test1( client, g_agworld );        // mp4 directory
        testp( client, g_master );        // playlist
    }
    catch (std::exception &xxx) {
        LOG_ERROR(Lgr) << "Exit on exception: " << xxx.what();
    }
    //
    log_last_err( client );
    finish_logging();
    return 0;
}
