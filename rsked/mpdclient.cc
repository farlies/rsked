/// Implementation of MPD client for the Rsked Mpd_player

/*   Part of the rsked package.
 *   Copyright 2020 Steven A. Harp   farlies(at)gmail.com
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
 */

#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <functional>

#include "mpdclient.hpp"
#include "logging.hpp"

#include <mpd/connection.h>
#include <mpd/status.h>


/// CTOR with default TCP port on localhost
///
Mpd_client::Mpd_client()
    : m_socket_path( "localhost" ), m_port(0)
{
}

/// CTOR: Construct with named socket path. Does not connect.
///
Mpd_client::Mpd_client( const char* path )
    : m_socket_path( path ), m_port(0)
{
}

/// CTOR: Construct with hostname and TCP port.
///
Mpd_client::Mpd_client( const char* hostname, unsigned port )
    : m_socket_path( hostname ), m_port(port)
{
}

/// On destruction, disconnect if connected.
///
Mpd_client::~Mpd_client()
{
    if (m_conn) { mpd_connection_free( m_conn ); }
}

/// Disconnect from the MPD server.
///
void Mpd_client::disconnect()
{
    if (m_conn) {
        mpd_connection_free( m_conn );
        m_conn = nullptr;
    }
}

/// Configure the way Mpd_client will connect to MPD.  This has no
/// effect on any existing connection: disconnect() then connect() to
/// make new parameters take effect.  With just 1 argument, the host
/// indicates a unix socket pathname.  Port=0 indicates the default
/// port, usually 6600.
///
void Mpd_client::set_connection_params( const std::string &host, unsigned port )
{
    m_port = port;
    m_socket_path = host;
}

/// Throw unless the client has a connection in m_conn. It does not attempt
/// to validate the connection.
/// * May throw
///
void Mpd_client::assert_connected()
{
    if (nullptr == m_conn) {
        m_last_err = Mpd_err::NoConnection;
        throw Mpd_connect_exception();
    }
}


/// Attempt to connect if not connected; it will throw an
/// Mpd_connect_exception unless a connection can be opened after
/// MAX_TRIES attempts. If it returns the connection m_conn is valid.
///
/// * May throw.
///
void Mpd_client::connect()
{
    constexpr const unsigned MAX_TRIES=2;
    for (unsigned ntries=MAX_TRIES; ntries; ntries--) {
        // get a connection object if we have none
        if (not m_conn) {
            m_conn = mpd_connection_new( m_socket_path.c_str(), 
                                         m_port, m_timeout_ms );
            if (!m_conn) { // this means: out of memory
                throw Mpd_connect_exception();
            }
        }
        // check (and clear) errors on the connection, determine server version
        auto err = mpd_connection_get_error( m_conn );
        if (err == MPD_ERROR_SUCCESS) {
            const unsigned *vnums =  mpd_connection_get_server_version(m_conn);
            if (vnums) {
                if (!m_server_vers[3]) {
                    m_server_vers[0] = vnums[0];
                    m_server_vers[1] = vnums[1];
                    m_server_vers[2] = vnums[2];
                    m_server_vers[3] = 1;
                    // I will print this only once
                    LOG_DEBUG(Lgr) << "Mpd_client: server version is " << vnums[0]
                                   << "." << vnums[1] << "." << vnums[2];
                }
            } else {
                LOG_DEBUG(Lgr) << "Mpd_client: server version is unknown";
            }
            return; // Success return
        }
        // diagnose connection problem
        enum mpd_server_error serr;
        diag_error("Mpd_client::connect: ",serr);
        // will clear the error but may also set m_conn=nullptr;
        if (ntries) sleep(1);  // give it a little time before retry
    }
    disconnect(); // dispose of connection object
    throw Mpd_connect_exception(); // Failure--throw
}

    // LOG_DEBUG(Lgr) << "Mpd_client connected to " << m_socket_path.c_str()
    //               << " (port " << m_port << ")";


/// Check current status by running the status command.
/// @arg printopt If  Mpd_opt::Print, then status will be logged if possible.
/// @returns true if server responds SUCCESS, or false if any error.
///
/// * Will not throw.
///
bool Mpd_client::check_status(Mpd_opt printopt)
{
    mpd_status *status {nullptr};
    m_last_err = Mpd_err::NoError;
    try {
        enum mpd_server_error serr;
        connect();
        if (nullptr == m_conn) {
            m_last_err = Mpd_err::NoConnection;
            return false;
        }
        // Get status:
        status = mpd_run_status( m_conn );
        if (!status) {
            LOG_ERROR(Lgr) << "Mpd_client: Did not get a status object from MPD";
            diag_error("Mpd_client::check_status/1: ",serr);
            m_last_err = Mpd_err::NoStatus;
            return false;
        }
        m_elapsed_secs = mpd_status_get_elapsed_time(status);
        switch (mpd_status_get_state( status )) {
        case MPD_STATE_PLAY:
            m_obs_state = PlayerState::Playing;
            break;
        case MPD_STATE_STOP:
            m_obs_state = PlayerState::Stopped;
            break;
        case MPD_STATE_PAUSE:
            m_obs_state = PlayerState::Paused;
            break;
        default:
            m_obs_state = PlayerState::Broken;            
        }
        if (printopt==Mpd_opt::Print) {
            log_status( status );
        }
        // The status might contain a runtime error message...
        // This is often interesting, for a list see
        // https://curl.haxx.se/libcurl/c/libcurl-errors.html
        //  "CURL failed: Couldn't connect to server"
        //  "CURL failed: Couldn't resolve host"
        //  "CURL failed: Recv failure"
        const char* perr = mpd_status_get_error(status);
        bool runtime_error = false;
        if (perr) {
            LOG_ERROR(Lgr) << "Mpd_client::check_status/2: " << perr;
            runtime_error = true;
            // attempt to clear it
            if (m_conn and not mpd_run_clearerror(m_conn)) {
                LOG_ERROR(Lgr) << "Mpd_client::check_status/3 "
                    "failed to clear the last player error";
            }
        }
        auto err=diag_error("Mpd_client::check_status/4: ",serr);
        mpd_status_free( status );
        return ((MPD_ERROR_SUCCESS == err) and not runtime_error);
    }
    catch(...) {                // catch everything
    }
    if (status) mpd_status_free( status );
    return false;
}


///////////////////////////////////////////////////////////////////////////////

/// Diagnose and log problems with the API.  If it returns MPD_ERROR_SERVER,
/// the server error is set in serr.  This ALWAYS clears the error
/// using mpd_connection_clear_error() since otherwise the connection
/// will be unusable.  Better to call 
///
/// @arg where  string that names the last operation--help localize the problem
/// @arg serr   reference to a variable hold a possible server error code
/// @returns The mpd_error, possibly MPD_ERROR_SUCCESS
///
enum mpd_error Mpd_client::diag_error(const char* where, 
                                      enum mpd_server_error& serr)
{
    serr=MPD_SERVER_ERROR_UNK;  // indicates irrelevant.
    if (!m_conn) {
        LOG_ERROR(Lgr) << "diag_error(" << where << ")--null m_conn";
        return MPD_ERROR_CLOSED;
    }
    enum mpd_error err = mpd_connection_get_error(m_conn);
    switch (err) {
    case MPD_ERROR_SUCCESS:     // 0. no error
        // LOG_DEBUG(Lgr) << where << "no error";
        break;
    case MPD_ERROR_OOM:         // 1. out of memory
        LOG_ERROR(Lgr) << where << "out of memory";
        break;
    case MPD_ERROR_ARGUMENT:    // 2. invalid argument
        LOG_ERROR(Lgr) << where  << "invalid argument";
        break;
    case MPD_ERROR_STATE:       // 3. in wrong state for call
        LOG_ERROR(Lgr) << where  << "wrong state for call";
        break;
    case MPD_ERROR_TIMEOUT:     // 4. timeout talking to MPD
        LOG_ERROR(Lgr) << where  << "timeout talking to MPD--try reconnecting";
        disconnect();
        break;
    case MPD_ERROR_SYSTEM:      // 5. "system error"
        LOG_ERROR(Lgr) << where  << "MPD system error";
        break;
    case MPD_ERROR_RESOLVER:    // 6. stream host unknown to dns
        LOG_ERROR(Lgr) << where  << "cannot resolve hostname-network down?";
        break;
    case MPD_ERROR_MALFORMED:   // 7. response from mpd is fubar
        LOG_ERROR(Lgr) << where  << "invalid response from MPD";
        break;
    case MPD_ERROR_CLOSED:      // 8. connection closed by mpd
        LOG_ERROR(Lgr) << where  << "connection closed by MPD--try reconnecting";
        disconnect();
        break;
    case MPD_ERROR_SERVER:      // 9. special server error
        serr=diag_server_error(where);
        break;
    default:
        LOG_ERROR(Lgr) << where << "uncategorized error " << err;
        break;
    }
    if (err != MPD_ERROR_SUCCESS) {
        // Some of these (like TIMEOUT) may *assert* and abort the program: do not
        // attempt to call mpd_connection_get_error_message, but do write code to cerr
        LOG_DEBUG(Lgr) << "libmpd error number " << err << " != " << MPD_ERROR_SUCCESS;
        // const char* perr = mpd_connection_get_error_message(m_conn);
        // if (perr) { LOG_ERROR(Lgr) << perr; }
    }
    // always execute this before exit if there is still a connection
    if (m_conn) mpd_connection_clear_error(m_conn);
    return err;
}

/// Diagnose problems in the MPD server itself. Most of these we
/// should never see. Currently just logs.  These are checked if
/// mpd_connection_get_error returns MPD_ERROR_SERVER.
///
/// @arg where   string indicates last client operation 
/// @returns  server error code.
///
enum mpd_server_error Mpd_client::diag_server_error(const char* where)
{
    if (!m_conn) {
        LOG_ERROR(Lgr) << where << " null m_conn";
        return MPD_SERVER_ERROR_UNK;
    }
    enum mpd_server_error err = mpd_connection_get_server_error(m_conn);
    switch (err) {
    case MPD_SERVER_ERROR_NOT_LIST:
        LOG_ERROR(Lgr) << where  << "Server error: Not list";
        break;
    case MPD_SERVER_ERROR_ARG:
        LOG_ERROR(Lgr) << where  << "Server error: argument";
        break;
    case MPD_SERVER_ERROR_PASSWORD:
        LOG_ERROR(Lgr) << where  << "Server error: password";
        break;
    case MPD_SERVER_ERROR_PERMISSION:
        LOG_ERROR(Lgr) << where  << "Server error: permission";
        break;
    case MPD_SERVER_ERROR_UNKNOWN_CMD:
        LOG_ERROR(Lgr) << where  << "Server error: unknown command";
        break;
    case MPD_SERVER_ERROR_NO_EXIST:
        LOG_ERROR(Lgr) << where  << "Server error: no exist";
        break;
    case MPD_SERVER_ERROR_PLAYLIST_MAX:
        LOG_ERROR(Lgr) << "Server error: playlist maximum";
        break;
    case MPD_SERVER_ERROR_SYSTEM:
        LOG_ERROR(Lgr) << where  << "Server error: system";
        break;
    case MPD_SERVER_ERROR_PLAYLIST_LOAD:
        LOG_ERROR(Lgr) << where  << "Server error: playlist load";
        break;
    case MPD_SERVER_ERROR_UPDATE_ALREADY:
        LOG_ERROR(Lgr) << where  << "Server error: update already";
        break;
    case MPD_SERVER_ERROR_PLAYER_SYNC:
        LOG_ERROR(Lgr) << where  << "Server error: player sync";
        break;
    case MPD_SERVER_ERROR_EXIST:
        LOG_ERROR(Lgr) << where  << "Server error: exist";
        break;
    case MPD_SERVER_ERROR_UNK:
    default:
        LOG_ERROR(Lgr) << where  << "Server error: Unknown";
        break;
    }
    return err;
}


/// Print some status information.
///
void Mpd_client::log_status(mpd_status* s)
{
    switch (mpd_status_get_state( s )) {
    case MPD_STATE_PLAY:
        LOG_DEBUG(Lgr) << "MPD State: Playing";
        break;
    case MPD_STATE_STOP:
        LOG_DEBUG(Lgr) << "MPD State: Stopped";
        break;
    case MPD_STATE_PAUSE:
        LOG_DEBUG(Lgr) << "MPD State: Paused";
        break;
    default:
        LOG_DEBUG(Lgr) << "Unknown";
    }
    // Note: volume -1 means no volume support
    LOG_DEBUG(Lgr) << " Volume: " << mpd_status_get_volume( s );
    LOG_DEBUG(Lgr) << " Repeat: " << (mpd_status_get_repeat(s) ? "Yes" : "No");
    LOG_DEBUG(Lgr) << " Random: " << (mpd_status_get_random(s) ? "Yes" : "No");
    LOG_DEBUG(Lgr) << " Single: " << (mpd_status_get_single(s) ? "Yes" : "No");
    LOG_DEBUG(Lgr) << " Consume: " << (mpd_status_get_consume(s) ? "Yes" : "No");
    LOG_DEBUG(Lgr) << " Queue Length: " << mpd_status_get_queue_length(s);
    LOG_DEBUG(Lgr) << " Elapsed time: " << mpd_status_get_elapsed_time(s)
                   << " sec";
    LOG_DEBUG(Lgr) << " Total time: " << mpd_status_get_total_time(s) << " sec";
    LOG_DEBUG(Lgr) << " Bit rate: " << mpd_status_get_kbit_rate(s) << " kbps";
    const char* perr = mpd_status_get_error(s);
    if (perr) {
        LOG_ERROR(Lgr) << " Error: " << perr;
    }
}

///////////////////////////////////////////////////////////////////////////////


/// Stop playing; this will try twice.
/// May throw.
/// 
void Mpd_client::stop()
{
    unsigned tries=1;
    connect();
    assert_connected();
    while (not mpd_run_stop(m_conn)) {
        enum mpd_server_error serr;
        auto perr = diag_error("Mpd_client::stop: ",serr);
        if (tries-- and (perr==MPD_ERROR_TIMEOUT)) continue;
        throw Mpd_run_exception();
    }
}

/// Add a local file, directory, or URL to the end of the queue.
/// The string should be relative to the music directory, and
/// should not have a trailing slash.
///
/// * May throw.
///
void Mpd_client::enqueue( const std::string &uri )
{
    connect();
    assert_connected();
    int id = mpd_run_add( m_conn, uri.c_str() );
    if (-1 == id) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::enqueue: ",serr);
        LOG_ERROR(Lgr) << "Failed to enqueue resource '" << uri << "'";
        m_last_err = Mpd_err::NoExist;
        throw Mpd_queue_exception();
    }
}


/// Enqueue a song given a pointer to its mpd_song song struct.
/// @return id of the added song
/// *  May throw.
///
int Mpd_client::enqueue( const mpd_song* song )
{
    int id=(-1);
    const char *uri = mpd_song_get_uri( song );
    if (uri) {
        id = enqueue_id( uri );
    }
    return id;
}


/// Add a URI or a single file to the end of the queue.
/// Note well: this will not handle directories!
/// Not currently used in rsked.
/// @returns id of the added URI
///
/// *  May throw.
///
int Mpd_client::enqueue_id( const std::string &uri )
{
    connect();
    assert_connected();
    int id = mpd_run_add_id( m_conn, uri.c_str() );
    if (-1 == id) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::enqueue: ",serr);
        LOG_ERROR(Lgr) << "Failed to enqueue resource '" << uri << "'";
        m_last_err = Mpd_err::NoExist;
        throw Mpd_queue_exception();
    }
    return id;
}


/// Adds a named playlist to the end of the queue. May throw.
/// @returns id of the added URI
///
void Mpd_client::enqueue_playlist( const std::string &plname )
{
    connect();
    assert_connected();
    bool ok = mpd_run_load( m_conn, plname.c_str() );
    if (not ok) {
        enum mpd_server_error serr;
        auto err=diag_error("Mpd_client::enqueue_playlist: ",serr);
        if ((MPD_ERROR_SERVER==err) and (MPD_SERVER_ERROR_NO_EXIST==serr)) {
            LOG_ERROR(Lgr) << "Playlist '" << plname << "' was not found";
        }
        m_last_err = Mpd_err::NoExist;
        throw Mpd_queue_exception();
    }
}


/// Pause playing the current track. No effect if already paused or
/// stopped. May throw.
///
void Mpd_client::pause()
{
    connect();
    assert_connected();
    if (not mpd_run_pause( m_conn, true )) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::pause: ",serr);
        throw Mpd_run_exception();
    }
}

/// Resume playing the current track. No effect if already playing or
/// stopped. May throw.
///
void Mpd_client::unpause()
{
    connect();
    assert_connected();
    if (not mpd_run_pause( m_conn, false )) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::unpause: ",serr);
        throw Mpd_run_exception();
    }
}

/// Play the current track. May throw.
///
void Mpd_client::play()
{
    connect();
    assert_connected();
    if (not mpd_run_play( m_conn )) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::play: ",serr);
        throw Mpd_run_exception();
    }
}

/// Play starting with the track at position p. Note that 0 is the
/// first queue position. May throw.
///
void Mpd_client::play_pos(unsigned p)
{
    connect();
    assert_connected();
    if (not mpd_run_play_pos( m_conn, p )) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::play_pos: ",serr);
        throw Mpd_run_exception();
    }
}


/// Play the given track ID. May throw.
///
void Mpd_client::play( unsigned id )
{
    connect();
    assert_connected();
    if (not mpd_run_play_id( m_conn, id )) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::play(id): ",serr);
        throw Mpd_run_exception();
    }
}

/// Whether to repeat the queue indefinitely many times.
///
void Mpd_client::set_repeat_mode(bool repeatp)
{
    connect();
    assert_connected();
    if (not mpd_run_repeat(m_conn, repeatp)) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::set_repeat_mode: ",serr);
        throw Mpd_run_exception();
    }
}

/// Set the volume to a given percentage of full (0-100).
///
void Mpd_client::set_volume(unsigned pct)
{
    if (pct > 100) {
        pct = 100;
    }
    connect();
    assert_connected();
    if (not mpd_run_set_volume(m_conn, pct)) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::set_volume: ",serr);
        throw Mpd_run_exception();
    }
}

/// Clear the queue and finish response.
/// May throw.
///
void Mpd_client::clear_queue()
{
    connect();
    assert_connected();
    if (not mpd_run_clear(m_conn)) {
        enum mpd_server_error serr;
        diag_error("Mpd_client::clear_queue: ",serr);
        throw Mpd_run_exception();
    }
}

/// Determine if the given URI is playing. Will return true if
/// mpd is on the right URI, but play is paused or stopped.
/// @returns true if the current song matches the given URI
///
bool Mpd_client::verify_playing_uri( const std::string &uri)
{
    connect();
    assert_connected();
    mpd_song *song = mpd_run_current_song(m_conn);
    if (!song) { return false; }
    if (const char* puri=mpd_song_get_uri(song)) {
        return (uri == puri);
    }
    return false;
}


/// Collect URIs for the songs of album with given (exact) name.
/// @returns number of URIs collected.
///
unsigned Mpd_client::search_album(const std::string &name,
                                  std::vector<std::string>& urivec)
{
    unsigned ns=0;
    connect();
    assert_connected();
    mpd_search_cancel(m_conn);
    bool rc =
        mpd_search_db_songs(m_conn,true)
        and mpd_search_add_tag_constraint(m_conn, MPD_OPERATOR_DEFAULT,
                                  MPD_TAG_ALBUM, name.c_str())
        and mpd_search_commit(m_conn);
    if (rc) {
        while(mpd_song *song = mpd_recv_song(m_conn)) {
            if (const char* uri=mpd_song_get_uri(song)) {
                urivec.push_back( uri );
                ++ns;
            }
            mpd_song_free( song );
        }
    } else {
        enum mpd_server_error serr;
        diag_error("search_album: ",serr);
        throw Mpd_search_exception();
    }
    return ns;
}

/// example function that may be passed to search_album
///
void desc_song( const mpd_song *song )
{
    const char* name = mpd_song_get_tag( song, MPD_TAG_TITLE, 0 );
    const char* track = mpd_song_get_tag( song, MPD_TAG_TRACK, 0 );
    if (name) {
        LOG_INFO(Lgr) << (track ? track : "??") <<  ": " << name;
        if (const char* uri=mpd_song_get_uri(song)) {
            LOG_INFO(Lgr) << "MPD URI=" << uri;
        }
    }
}

