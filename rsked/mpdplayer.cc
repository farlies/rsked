/// The Player interface for the Music Player Daemon (MPD).
/// Mpd_player configuration parameters:
/// - run_mpd
/// - bin_path
/// - socket
/// - host
/// - port

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

#include "configutil.hpp"
#include "config.hpp"
#include "mpdplayer.hpp"
#include "mpdclient.hpp"
#include "playermgr.hpp"

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

/// Default MDP network port and host/socket
constexpr const unsigned Default_mpd_port = 6600;
constexpr const char *Default_mpd_hostname = "localhost";
const boost::filesystem::path Default_mpd_socket {"~/.config/mpd/socket"};



/// CTOR - create an Mpd_client right away, but don't connect yet.
///
Mpd_player::Mpd_player()
    : m_name("Mpd_player"),
      m_remote(std::make_unique<Mpd_client>()),
      m_socket(expand_home(Default_mpd_socket)),
      m_hostname(Default_mpd_hostname),
      m_port(Default_mpd_port),
      m_cm(Child_mgr::create(m_name))

{
    LOG_INFO(Lgr) << "Created an Mpd_player named " << m_name;
}


/// CTOR with name.  Note that the player will expect a config file
/// section with this name to retrieve its parameters.
///
Mpd_player::Mpd_player(const char *name)
    : m_name(name),
      m_remote(std::make_unique<Mpd_client>()),
      m_socket(expand_home(Default_mpd_socket)),
      m_hostname(Default_mpd_hostname),
      m_port(Default_mpd_port),
      m_cm(Child_mgr::create(name))
{
    LOG_INFO(Lgr) << "Created an Mpd_player named " << m_name;
}


/// DTOR
///
Mpd_player::~Mpd_player()
{
    shutdown_mpd();  // will not throw
}

////////////////////////////// Private Methods //////////////////////////////

/// This will make sure the child MPD process is started (if rsked is
/// responsible for this) and that our MPD client has connected.
/// Sets m_usable according to the result.
/// * May THROW a Player_exception or CM_exception.
///
void Mpd_player::assure_connected()
{
    if (m_run_mpd and not m_cm->running()) {
        try_start();
    }
    //
    m_usable=m_remote->check_status((m_debug ? Mpd_opt::Print : Mpd_opt::NoPrint));
    if (!m_usable) {
        // there was a serious issue, e.g. cannot communicate with mpd
        throw Player_comm_exception();
    }
}


/// Optionally start the child process, and connect to the MPD command
/// socket.  On success, it will return with m_usable set to true.
/// The player will begin in stopped, not playing.  Having MPD as a
/// child process of rsked is the preferred configuration since it is
/// more easily killed if it becomes nonresponsive (this sometimes
/// happens with network streams). We do not run it as a daemon for
/// similar reasons.
///
/// * May throw a Player_exception or CM_exception.
///
void Mpd_player::try_start()
{
    // If we are running MPD as a child, start it now.
    if (m_run_mpd and not m_cm->running()) {
        LOG_DEBUG(Lgr) << "Mpd_player::try_start launching child MPD";
        m_cm->clear_args();
        m_cm->add_arg( "--no-daemon" ); // daemon is rather uncontrollable!
        //m_cm->add_arg( "--kill");    ! No: MPD aborts if no pidfile is found :-(
        //m_cm->add_arg( "--stderr" ); ! No: use configured log file instead
        m_cm->start_child();   // may throw
    }
    // MPD needs a little time to start accepting connections, so we
    // try a few times. The unix socket (preferred) goes live after the
    // TCP one.  Interruption by a signal is a possibility while we
    // are waiting.
    constexpr const unsigned MAX_RETRIES=3;
    constexpr const long MPD_WAIT_MSEC=350;
    //
    for (unsigned i=1; i<=MAX_RETRIES; i++) {
        struct timespec req {0,MPD_WAIT_MSEC*1000000}; // wait for mpd
        struct timespec rem {0,0};
        while (-1 == nanosleep(&req,&rem)) { req = rem; };
        //
        if (try_connect()) {    // success
            LOG_INFO(Lgr) << "MPD connect--success on attempt " << i;
            stop();
            m_usable = true;
            return;
        }
    }
    mark_unusable();
    throw Player_startup_exception();
}    


/// Attempt connection. Use the Unix socket if it exists, otherwise
/// try the TCP port. This will not start an MPD child process.
/// If probe_only is false, then failure will mark the player unusable.
/// Returns true on success (connected), or false on failure.
///
/// * Will NOT throw.
///
bool Mpd_player::try_connect(bool probe_only)
{
    // catch Mpd_connect_exception
    // maybe try unix socket
    if (boost::filesystem::exists(m_socket)) {
        try {
            m_remote->set_connection_params( m_socket.native() );
            m_remote->connect();
            LOG_INFO(Lgr) << m_name << " unix socket is connected.";
            return true;
        }
        catch(...) {
            LOG_WARNING(Lgr) << m_name << " unix socket connection failed.";
        }
    } else {
        LOG_WARNING(Lgr) << m_name << " unix socket is not ready.";
    }
    // else try TCP socket
    try {
        m_remote->set_connection_params( m_hostname, m_port );
        m_remote->connect();
        LOG_INFO(Lgr) << m_name << " " << m_port 
                      << " /TCP  socket is connected.";
        return true;
    }
    catch(...) {
        if (not probe_only) {
            LOG_WARNING(Lgr) << m_name << " TCP socket connection failed.";
            m_usable = false;
        }
    }
    return false;
}

/// Kill any running mpd and remove its socket file.
///
/// * Will NOT throw
///
void Mpd_player::shutdown_mpd()
{
    if (m_run_mpd) {
        m_cm->kill_child(); // Kill the child process; throws no exceptions
        if (not m_socket.empty()) {
            boost::system::error_code ec;
            boost::filesystem::remove( m_socket, ec );
        }
    }
}


/// Return true if there is any evidence that mpd has opened the configured
/// socket or is listening on the configured port. This doesn't actually
/// validate that the external process is mpd, just that something is
/// sitting on the resource. This condition is undesirable if we have not
/// started mpd intend to, since mpd will loop forever attempting to
/// to grab the port/socket, thereby breaking rsked. Note that mpd fails
/// to clean up its unix socket which may yield false positives.
///
/// * Will NOT throw, Except: system runs out of memory while checking
///
bool Mpd_player::any_mpd_running()
{
    using boost::asio::ip::tcp;
    bool result=false;
    boost::system::error_code ec;
    if (boost::filesystem::exists(m_socket, ec)) {
        return true;
    }
    // Try  m_hostname/m_port
    try {
        boost::asio::io_service l_io_service {};
        boost::asio::ip::tcp::socket l_socket { l_io_service };
        tcp::resolver resolver(l_io_service);
        std::string pstr { std::to_string(m_port) };
        boost::asio::connect(l_socket,resolver.resolve(m_hostname,pstr));
        result = true;
        l_socket.close(); // can throw
    }
    catch (std::exception &) {
        // failure expected.
    }
    
    return result;
}


/// Flag the player as UNusable.
/// Kills any running child process.
///
/// * Will NOT throw
///
void Mpd_player::mark_unusable()
{
    m_usable = false;
    m_remote->disconnect();          // no exceptions
    m_last_unusable = time(0);
    LOG_WARNING(Lgr) << m_name << " is being marked as Unusable until future notice";
    if (m_run_mpd) {
        shutdown_mpd();           // Kill the child process; no exceptions
    }
    m_state = PlayerState::Broken;
}


/// If the player has made no progress in elapsed time (no change)
/// for m_stalls_max number of checks then throw a Player_media_exception.
/// (This might be a bit harsh, since it could be a brief connection issue.)
/// Will not alert if the source is marked as "may-be-quiet".
///
/// * May THROW a Player_exception.
///
void Mpd_player::check_not_stalled()
{
    if (m_src->may_be_quiet()) return;
    //
    unsigned u= m_remote->elapsed_secs();
    LOG_DEBUG(Lgr) << "Mpd_player elapsed_secs=" << u;
    //
    if (u == m_last_elapsed_secs) {
        ++m_stall_counter;
        LOG_WARNING(Lgr) << "Mpd_player stall_counter=" << m_stall_counter;
        if (m_stall_counter > m_stalls_max) {
            LOG_WARNING(Lgr) << m_name << " appears stalled on {"
                             << m_src->name() << "}";
            throw Player_media_exception();
        }
    } else {
        if (m_stall_counter) {
            LOG_DEBUG(Lgr) << "Mpd_player stall_counter reset to 0";
            m_stall_counter = 0;
        }
        m_last_elapsed_secs = u;
    }
}


///////////////////////////////// Player API /////////////////////////////////

/// Returns true if MPD is not actually playing, or the connection to the
/// process is broken.
///
/// * Will NOT throw.
///
bool Mpd_player::completed()
{
    if (!m_remote->check_status((m_debug ? Mpd_opt::Print : Mpd_opt::NoPrint))) {
        // there was a serious issue, e.g. cannot communicate with mpd
        return false;
    }
    return (m_remote->obs_state() != PlayerState::Playing);
}

/// Does a check to verify MPD  is playing (or completed
/// playing) the resource named in src.
/// Returns false if:
/// -- player is playing something else
/// -- player is in the wrong state (e.g. stopped, paused)
/// -- cannot communicate successfully with MPD
///
/// It is up to the caller to rectify the problem if false is
/// returned.  Unfortunately it seems impossible to tell whether mpd is
/// playing a named playlist--if this type of source is encountered
/// we just verify the name of the source is the last one we enqueued.
///
/// * May THROW a Player_exception if there is something basically
///   wrong with the player.  Player should have marked itself unusable.
///
bool Mpd_player::currently_playing( spSource src )
{
    if (!src or !m_src) { return false; }
    assure_connected();
    if (m_src->name() != src->name()) { // wrong source?
        return false;
    }
    // remote elapsed_secs() and obs_state() have been updated.
    // Verify that state is play, or pause, or stopped with no-repeat
    // If playing, check whether elapsed time is different than last check;
    // and if the same, increment the stall counter, up to STALLS_MAX.
    switch (m_remote->obs_state()) {
    case PlayerState::Stopped:
        if (src->repeatp()) {
            LOG_WARNING(Lgr) << m_name 
                             << " observed stopped but should be repeating";
            return false;
        }
        break;
    case PlayerState::Playing:
        check_not_stalled();
        return true;
        break;
    case PlayerState::Paused:
        // If it should be playing but is paused, resume it (generically)
        if (PlayerState::Playing == m_state) {
            resume();           // there will be a second or so of latency
            return true;        // so assume its okay for this check
        }
        break;
    default:
        // Includes Broken state
        LOG_WARNING(Lgr) << "Mpd player in an unknown state";
        return false;
    }

    if (src->medium() == Medium::playlist) {
        return true;
    }
    // this checks that the song or the URL is the same as resource()
    return m_remote->verify_playing_uri( src->resource() );
}


/// Exit: Terminate mpd process.
/// * Will not throw.
///
void Mpd_player::exit()
{
    if (m_cm->running()) {
        try {
            m_remote->disconnect();
            shutdown_mpd();
            LOG_INFO(Lgr) << m_name << " exit";
            m_state = PlayerState::Stopped;
        }
        catch (...) {
            // Child_mgr logs error message
        }
    } else {
        LOG_INFO(Lgr) << m_name << " already exited";
    }
}

/// Extract configuration values. Should specify either socket or host
/// and port (for TCP connection); may specify both.  This will not
/// start MPD if testp is true, so it is safe to use in test
/// mode. The first call to play will actually start the
/// child/connection.  It will check whether a rogue mpd is running
/// and mark itself as unusable if one is found.
///
/// * May throw.
///
void Mpd_player::initialize( Config &cfg, bool testp )
{
    cfg.get_bool(m_name.c_str(),"enabled",m_enabled);
    if (not m_enabled) {
        LOG_INFO(Lgr) <<"Mpd_player '" << m_name << "' (disabled)";
    }
    // additional configuration only if enabled:
    cfg.get_bool(m_name.c_str(),"run_mpd",m_run_mpd);
    cfg.get_unsigned(m_name.c_str(),"port",m_port);
    cfg.get_string(m_name.c_str(),"host", m_hostname);
    cfg.get_bool(m_name.c_str(),"debug", m_debug);
    // depending on timing, the socket might not exist (yet)
    cfg.get_pathname(m_name.c_str(),"socket", FileCond::NA, m_socket);
    cfg.get_pathname(m_name.c_str(),"bin_path", 
                     FileCond::MustExist, m_bin_path);
    m_cm->set_binary( m_bin_path );
    m_cm->set_name( m_name );
    // If not enabled, stop here, do not check whether it is running
    if (not m_enabled) {
        return;
    }
    // Check whether a rogue mpd process is running when rsked is
    // supposed to be the mpd parent; this is a common misconfiguration.
    // Also, mpd fails to clean up its unix socket on exit.
    if (m_run_mpd and not m_cm->running()) {
        if (any_mpd_running()) {   // No throw.
            LOG_ERROR(Lgr) << "Mpd_player found a non-child mpd running!";
            if (testp) {                // if testing, throw
                throw Player_startup_exception();
            }
            // Not testing: mark mpd unusable but attempt to run anyway
            // An alternative would be to toggle run_mpd and use the rogue one.
            mark_unusable();
        }
    }
    //
    LOG_INFO(Lgr) << "Mpd_player '" << m_name << "' initialized";
}


/// Evaluates whether we can use MPD (only if enabled).
/// If this daemon has not been tried for m_recheck_secs since
/// last being marked unusable, attempt to start it again.
///
/// *  Will NOT throw.
///
bool Mpd_player::is_usable()
{
    if (not m_enabled) return false;
    //
    try {
        assure_connected(); // sets m_usable
    } catch(...) {
        m_usable=false;
    }
    if (!m_usable) {
        if ( (time(0) - m_last_unusable) > m_recheck_secs ) {
            LOG_DEBUG(Lgr) << m_name << " tentatively marked as usable again";
            try_start();
        }
    }
    return m_usable;
}

/// Pause play. If the current source is a network stream, then just
/// stop the player--it will not recover from pause; otherwise execute
/// a real pause.
///
/// * May throw Mpd_exception()
///
void Mpd_player::pause()
{
    assure_connected();
    if (m_src->medium()==Medium::stream) {
        LOG_DEBUG(Lgr) << m_name << " stopping network stream";
        m_remote->stop();
    } else {
        m_remote->pause();
    }
    m_state = PlayerState::Paused;
}


/// Start playing the source.  If src is NULL, then no further actions
/// are taken.  Anything currently playing is stopped.  The queue is
/// cleared and only the source will be scheduled to play.  It will
/// verify that MPD is usable; if the process or connection had not
/// been initiated, it will be launched now: this is the normal way to
/// start the MPD process/connection. The volume is set to 100%.
///
/// * May throw Player_media_exception(), Player_comm_exception
///
void Mpd_player::play( spSource src )
{
    if (not m_enabled) {
        LOG_ERROR(Lgr) << m_name << " is disabled--cannot play";
        throw Player_media_exception();
    }
    if (!src) { return; }       // null src
    assure_connected();
    try {
        if (not m_usable) {
            LOG_ERROR(Lgr) << m_name << " is not usable--cannot play";
            throw Player_comm_exception();
        }
        m_remote->stop();
        m_remote->clear_queue();
        m_remote->set_volume(100);
    } catch (const Mpd_run_exception &) {
        throw Player_media_exception();
    }

    Medium medium = src->medium();
    // validate medium
    switch (medium) {
    case Medium::off:
    case Medium::radio:
        LOG_ERROR(Lgr) << m_name << " does not handle this medium: "
                       << media_name(src->medium());
        throw Player_media_exception();
        break;

    case Medium::stream:
    case Medium::directory:
    case Medium::file:
        LOG_INFO(Lgr) << m_name << " play: {" << src->name() << "}";
        try {
            if (src->dynamic()) {
                std::string dynstr;
                uri_expand_time( src->resource(), dynstr );
                m_remote->enqueue(dynstr);
            } else {
                m_remote->enqueue(src->resource());
            }
        } catch (Mpd_queue_exception&) {
            throw Player_media_exception();
        }
        m_src = src;
        break;
    case Medium::playlist:
        LOG_INFO(Lgr) << m_name << " play: {" << src->name() << "}";
        try {
            m_remote->enqueue_playlist(src->resource());
        } catch (Mpd_queue_exception&) {
            throw Player_media_exception();
        }
        m_src = src;
        break;
    };
    //
    m_stall_counter = 0;
    m_last_elapsed_secs = 0;
    m_remote->set_repeat_mode( src->repeatp() );
    m_remote->play();
    m_state = PlayerState::Playing;
}

/// Resume play after pause. If the current source is a network stream,
/// then reissue play on m_src; otherwise use unpause.
/// * May throw Player_exception
///
void Mpd_player::resume()
{
    assure_connected();
    if (m_src->medium()==Medium::stream) {
        LOG_DEBUG(Lgr) << m_name << " restarting network stream";
        play(m_src);
    } else {
        m_remote->unpause();
    }
    m_state = PlayerState::Playing;
}

/// Return current state.
///
/// * Will NOT throw :-)
///
PlayerState Mpd_player::state()
{
    return m_state;
}

/// Stop play. If there is no connection and mpd is supposed to be our child
/// process, then this does nothing.
/// 
/// * May throw Player_media_exception
///
void Mpd_player::stop()
{
    m_src.reset();
    m_state = PlayerState::Stopped;
    //
    try {
        assure_connected();
        m_remote->stop();
    } catch (const Mpd_run_exception &) {
        throw Player_media_exception();
    }
}

/// Return true if this player is okay, false if not so.
/// This is the same as is_usable(), however it may return false
/// if we lose internet while playing an internet stream.
/// * Throws NO exceptions.
///
bool Mpd_player::check()
{
    bool rc = is_usable();

    // If we are playing an MP3 stream but the internet becomes
    // unavailable, then return false.
    if (m_src and m_src->medium()==Medium::stream) {
        if (not Player_manager::inet_available()) {
            // oops -- abort playing this stream if we are trying to play
            if (m_state == PlayerState::Playing) try { stop(); } catch(...) {}
            LOG_WARNING(Lgr) << "Mpd_player playing stream--internet problems";
            rc = false;
        }
    }
    return rc;
}
    
