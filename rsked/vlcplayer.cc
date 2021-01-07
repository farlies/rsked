/// The vlc_player plays audio via the VLC Media Player (aka VLC).
/// VLC Media Player is a project of the Non-profit VideoLAN Organization.
///
/// In the json rsked Vlc_player configuration you may adjust any
/// of the following parameters:
/// - enabled  (true|false)
/// - bin_path "/usr/bin/vlc"
/// - debug    (true|false)
///
/// * * *
/// This implementation uses the textual 'cli' interface to VLC.
/// It currently does not attempt to adjust the LANG to 'en_US.UTF-8' but
/// that is what is assumed. Any other locale: ymmv.
////////////////////////////////////////////////////////////////////////////

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
#include "vlcplayer.hpp"
#include "playermgr.hpp"

#include "test/fake_rsked.hpp"

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>

namespace fs = boost::filesystem;

const fs::path Default_vlc_bin {"/usr/bin/vlc"};

//////////////////////////////////////////////////////////////////////////
///
/// In theory, Vlc volume goes up to 1024. But settings much above 300
/// seem to cause gross audio distortation.  TODO: determine why.
/// Constant VlcMaxVol calibrates the effective volume setting, and
/// corresponds to a json configured "100".
/// You may need to adjust the volume of vlc relative to other sources
/// like SDR so that a consistent level is presented by all players.
///
const unsigned VlcMaxVol       { 300 };  // VLC units
const unsigned Default_vlc_vol { 100 };  // Percentage
///
//////////////////////////////////////////////////////////////////////////




/// this is used to parse vlc status messages from cli interface
///
static boost::regex Status_regex(R"*(
^ \( \s* (state|(?:audio \s volume)|(?:new \s input)) [:]* \s+
  ( [^)]+ ) \s+ \) $)*",  boost::regex::mod_x);

/// look for an unsigned decimal number on a line by itself:
///
static boost::regex
Unsigned_regex(R"*( ^ \s* (\d+) \s* $ )*", boost::regex::mod_x);


/// Extract an unsigned integer appearing on a line by itself.
/// Sets parameter u to the parsed integer and return true on success.
/// If more than one integer is present, only the first will be parsed.
/// Returns false if no integer was found.
///
/// * May throw  std::runtime_error in very rare circumstances
///
static bool parse_unsigned( const std::string &resp, unsigned long &u )
{
   boost::match_results<std::string::const_iterator> what;
   boost::match_flag_type flags = boost::match_default;

   if (regex_search(resp.begin(),resp.end(),what,Unsigned_regex,flags))
   {
       std::string snumber(what[1].first, what[1].second);
       try {
           u = std::stoul( snumber ); // can throw std::invalid_argument
       }
       catch( std::invalid_argument & ) { // unlikely
           LOG_DEBUG(Lgr) << "parse_unsigned: invalid int '"
                          << snumber << "'";
           return false;
       }
       return true;
   }
   // LOG_DEBUG(Lgr) << "parse_unsigned: no match for:" << resp;
   return false;
}


/// Takes apart the status response into state, volume and resource
///  'stopped' : PlayerState::Stopped
///  'playing' : PlayerState::Playing
///  'paused'  : PlayerState::Paused
///
/// * May throw std::runtime_error in very rare circumstances
///
static void parse_status( const std::string &sresp1, PlayerState &pstate,
                   unsigned &obsvol, std::string &resource)
{
   std::string::const_iterator start, end;
   start = sresp1.begin();
   end = sresp1.end();
   boost::match_results<std::string::const_iterator> what;
   boost::match_flag_type flags = boost::match_default;

   while(regex_search(start, end, what, Status_regex, flags))
   {
       std::string skey(what[1].first, what[1].second);
       std::string sval(what[2].first, what[2].second);
       if (skey == "audio volume") {
           try {
               obsvol = static_cast<unsigned>( std::stoul(sval) );
               // can throw std::invalid_argument
           } catch(...) {
               LOG_WARNING(Lgr) << "Vlc reports odd volume: " << sval;
           }
       }
       else if (skey == "status") {
           if ("stopped" == sval) { pstate = PlayerState::Stopped; }
           else if ("playing" == sval) { pstate = PlayerState::Playing; }
           else if ("paused" == sval) { pstate = PlayerState::Paused; }
           else {
               LOG_ERROR(Lgr) << "VLC unexpected status: '" << sval << "'";
               pstate=PlayerState::Broken;
           }
       }
       else if (skey == "new input") {
           resource = sval;   // TODO: maybe trim any file:// prefix?
       }
       start = what[0].second;  // move to the next sexpr
       flags |= boost::match_prev_avail;
       flags |= boost::match_not_bob;
   }

}

///////////////////////////////////////////////////////////////////////////

/// Establish baseline capabilities. Shared by all ctors.
/// * Will not throw.
///
void Vlc_player::cap_init()
{
    clear_caps();
    add_cap(Medium::file,       Encoding::mp3);
    add_cap(Medium::directory,  Encoding::mp3);
    add_cap(Medium::playlist,   Encoding::mp3);
    add_cap(Medium::stream,     Encoding::mp3);
    //
    add_cap(Medium::file,       Encoding::mp4);
    add_cap(Medium::directory,  Encoding::mp4);
    add_cap(Medium::playlist,   Encoding::mp4);
    add_cap(Medium::stream,     Encoding::mp4);
    //
    add_cap(Medium::file,       Encoding::ogg);
    add_cap(Medium::directory,  Encoding::ogg);
    add_cap(Medium::playlist,   Encoding::ogg);
    //
    add_cap(Medium::file,       Encoding::flac);
    add_cap(Medium::directory,  Encoding::flac);
    add_cap(Medium::playlist,   Encoding::flac);
    //
    std::string cstr;
    cap_string( cstr );
    LOG_DEBUG(Lgr) << m_name << " " << cstr;
}


/// CTOR - create an Vlc_client right away, but don't connect yet.
/// On creation, it establish the music library path, m_library_path;
/// This player needs to access the configured ResPathSpec in
/// order to (1) set working directory to music library, and
/// (2) validate relative pathnames. So, Main::rsked must be created
/// and have a configured schedule prior to creating this player!
///
Vlc_player::Vlc_player()
    : m_name("Vlc_player"),
      m_volume(Default_vlc_vol),
      m_library_path( Main::rsked->get_respathspec()->get_libpath() ),
      m_cm(Child_mgr::create(m_name))

{
    LOG_INFO(Lgr) << "Created a Vlc_player named " << m_name;
    m_library_uri += m_library_path.native();
    cap_init();
}


/// CTOR with name.  Note that the player will expect a config file
/// section with this name to retrieve its parameters.
///
Vlc_player::Vlc_player(const char *name)
    : m_name(name),
      m_volume(Default_vlc_vol),
      m_library_path( Main::rsked->get_respathspec()->get_libpath()),
      m_cm(Child_mgr::create(name))
{
    LOG_INFO(Lgr) << "Created an Vlc_player named " << m_name;
    m_library_uri += m_library_path.native();
    cap_init();
}


/// DTOR -- any running vlc should shut down automatically when
///  its stdin goes away...
///
Vlc_player::~Vlc_player()
{
}

////////////////////////////// Private Methods //////////////////////////////


/// Look for any of the known error strings from the (English language)
/// vlc cli response in string resp. Return true iff an error id detected.
///
static bool detect_vlc_error( const std::string &resp )
{
    static boost::regex
        err_regex(R"*( ^ [E|e]rror | (filesystem|access) \s stream \s error )*",
                  boost::regex::mod_x);
    boost::match_results<std::string::const_iterator> what;
    boost::match_flag_type flags = boost::match_default;

    return regex_search( resp.begin(), resp.end(), what, err_regex, flags );
}


/// Runs the command in cmd. Note: cmd must end in exactly one '\n'!
/// Retrieve the resuts in m_last_resp.
/// Check if there is an error message, and throw if so.
/// If the 'log_errors" argument is true, then log an error.
/// * Can throw Player_ops_exception
///
void Vlc_player::do_command( const std::string& cmd, bool log_errors )
{
    if (not m_cm or not m_cm->running() or not m_cm->has_pty())  {
        if (log_errors) {
            LOG_ERROR(Lgr) << m_name << " not ready for commands : '"
                           << cmd << "'\n";
        } // attempt to issue commands before player is ready
        throw Player_ops_exception();
    }

    if ((2 > cmd.size()) or (cmd.back() != '\n')) {
        if (log_errors) {
            LOG_ERROR(Lgr) << m_name << " malformed command: '"
                           << cmd << "'\n";
        } // this is a coding error in the caller
        throw Player_ops_exception();
    }
    if (m_debug) {
        std::string cmdx = cmd;
        cmdx.pop_back();
        LOG_DEBUG(Lgr) << "Tell vlc: " << cmdx;
    }
    m_cm->pty_write_nb( cmd );
    m_last_resp.clear();
    m_cm->pty_read_nb( m_last_resp, MaxResponse );
    if (m_debug) {
        LOG_DEBUG(Lgr) << "Vlc responds: " << m_last_resp;
    }
    if ( detect_vlc_error(m_last_resp) ) {
        if (log_errors) {
            std::string cmdx = cmd;
            cmdx.pop_back();
            LOG_ERROR(Lgr) << m_name << " '" << cmdx
                           << "' resulted in: " << m_last_resp;
        }
        throw Player_ops_exception();
    }
}



/// This will make sure the child VLC process is started and
/// responsive.  Sets m_usable according to the result.
//
/// * May THROW a Player_exception or CM_exception.
///
void Vlc_player::assure_running()
{
    if (not m_cm->running()) {
        try_start();
    }
    m_usable= check_status();
    if (!m_usable) {
        throw Player_comm_exception();
    }
}

/// Sets the vlc internal volume level to m_volume (%).
/// This should be done once after starting the child process.
/// Does nothing if the vlc child is not running.
///
/// * May throw a Player_exception or CM_exception.
///
void Vlc_player::set_volume()
{
    if (not m_cm->running()) {
        LOG_WARNING(Lgr) << m_name << " set volume cmd skipped: not running.";
        return;
    }
    unsigned vol = (m_volume * VlcMaxVol / 100);
    std::string svol { std::to_string( vol ) };
    std::string set_vol_cmd { "volume " };
    set_vol_cmd += svol;
    set_vol_cmd += '\n';
    LOG_DEBUG(Lgr) << m_name << " set vlc volume to " << svol;
    do_command( set_vol_cmd, true );
}

/// Optionally start the child process and verify it is responsive.
/// On success, it will return with m_usable set to true.
/// The player will begin in stopped, with an empty play list.
///
/// * May throw a Player_exception or CM_exception.
///
void Vlc_player::try_start()
{
    constexpr const unsigned MAX_RETRIES=3;
    constexpr const long VLC_WAIT_USEC = 123'000;

    // We run VLC as a child: start it now.
    if (not m_cm->running()) {
        LOG_INFO(Lgr) << "Launching VLC child process";
        m_cm->set_name("vlc");
        m_cm->enable_pty();
        m_cm->set_pty_read_timeout( 0, 100'000 );  // sec,usec: may be sluggish
        m_cm->set_wdir( m_library_path ); // relative paths of music files
        //
        m_cm->clear_args();
        m_cm->add_arg( "-Icli" ); // command line interface
        m_cm->add_arg( "--no-playlist-autostart" ); // don't autostart
        m_cm->start_child();      // may throw
    }
    for (unsigned i=1; i<=MAX_RETRIES; i++) {
        usleep( VLC_WAIT_USEC );  // give it time to fire up
        if (check_status()) {     // success
            m_usable = true;
            LOG_INFO(Lgr) << "VLC running ";
            set_volume();
            stop();
            return;
        }
    }
    mark_unusable();
    throw Player_startup_exception();
}

/// Issue a 'status' command to vlc and capture the result. Updates
/// m_state, m_obsvol and m_obsuri. If a result is obtained then
/// return true.  If there is no child process, or if we do not get a
/// response within the timeout period then return false.  Failure
/// will mark the player unusable (m_usable := false).
///
/// * Will NOT throw.
///
bool Vlc_player::check_status()
{
    try {
        if (not m_cm->running() or not m_cm->has_pty()) {
            return false;
        }
        do_command("status\n",true);
        m_obsuri = "";
        parse_status( m_last_resp, m_state, m_obsvol, m_obsuri );
        return true;
    }
    catch( const Chpty_exception &err ) {
        LOG_ERROR(Lgr) << m_name << " is unresponsive: " << err.what();
        mark_unusable();
    }
    catch( const std::exception &err ) {
        LOG_ERROR(Lgr) << m_name << " unexpected error in check_status: "
                       << err.what();
    }
    return false;
}


/// Kill any running child vlc.
///
/// * Will NOT throw
///
void Vlc_player::shutdown_vlc()
{
    // do not do anything if in test mode or not running
    if (m_testmode) return;
    if (not m_cm->running())  return;

    if (m_cm->has_pty()) {
        try {
            m_cm->pty_write_nb("quit\n");
        } catch(...) {}
    }
    m_cm->kill_child(); // Kill the child process; throws no exceptions
}


/// Flag the player as UNusable.
/// Kills any running child process.
///
/// * Will NOT throw
///
void Vlc_player::mark_unusable()
{
    m_usable = false;
    m_cm->kill_child();       // won't throw
    m_last_unusable = time(0);
    LOG_WARNING(Lgr) << m_name << " is being marked as Unusable until future notice";
    m_state = PlayerState::Broken;
}


/// If the player has made no progress in elapsed time (no change)
/// for m_stalls_max number of checks then throw a Player_media_exception.
/// (Tune parameter to ride out brief connection issues.)
/// Will not alert if the source is marked as "may-be-quiet".
///
/// * May THROW: Player_exception
///
void Vlc_player::check_not_stalled()
{
    if (m_src->may_be_quiet()) return;
    //
    try {
        do_command("get_time\n",true);
        unsigned long u {0};
        if (not parse_unsigned(m_last_resp,u)) { // no number returned
            // LOG_DEBUG(Lgr) << m_name << " no progress reported...maybe stalled";
            u = m_last_elapsed_secs;
        } else {
            LOG_DEBUG(Lgr) << m_name << " elapsed_secs=" << u;
        }
        // Note that u might jump down if src is a directory or playlist
        // since it reads the time in the individual /track/.
        if (u == m_last_elapsed_secs) {
            ++m_stall_counter;
            LOG_WARNING(Lgr) << "Vlc_player stall_counter=" << m_stall_counter;
        } else {
            if (m_stall_counter) {
                LOG_DEBUG(Lgr) << "Vlc_player stall_counter reset to 0";
                m_stall_counter = 0;
            }
            m_last_elapsed_secs = u;
        }
    } catch (const std::exception &err) {
        LOG_DEBUG(Lgr) << m_name << " error checking not stalled:" << err.what();
        throw Player_exception();
    }
    if (m_stall_counter > m_stalls_max) {
        LOG_WARNING(Lgr) << m_name << " stalled on source {"
                         << m_src->name() << "}";
        throw Player_media_exception();
    }
}


///////////////////////////////// Player API /////////////////////////////////

/// Returns true if VLC is done playing, or the connection to the
/// process is broken. Will also return false if the player is paused.
///
/// * Will NOT throw.
///
bool Vlc_player::completed()
{
    try {
        do_command("is_playing",false);
        // command returns '0\n' if not playing, '1\n' if playing.
        // also: is_playing will return 1 if the player is paused
        return (m_last_resp.npos == m_last_resp.find("0\n"));
    }
    catch (const std::exception &err) {
        LOG_ERROR(Lgr) << m_name << " completed check failed: " << err.what();
    }
    return true;
}

/// Does a check to verify VLC  is playing (or completed
/// playing) the resource named in src.
/// Returns false if:
/// -- player is playing something else
/// -- player is in the wrong state (e.g. stopped, paused)
/// -- cannot communicate successfully with VLC
///
/// It is up to the caller to rectify the problem if false is
/// returned.
///
/// * May THROW a Player_exception if there is something basically
///   wrong with the player.  Player should have marked itself unusable.
///
bool Vlc_player::currently_playing( spSource src )
{
    if (!src or !m_src) { return false; }
    assure_running();     // This sets m_state and m_obsuri
    if (m_src->name() != src->name()) { // had been on a different source
        return false;
    }

    if (not m_usable) return false;

    // remote elapsed_secs() and obs_state() have been updated.
    // Verify that state is play, or pause, or stopped with no-repeat
    // If playing, check whether elapsed time is different than last check;
    // and if the same, increment the stall counter, up to STALLS_MAX.

    switch (m_state) {
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
        LOG_WARNING(Lgr) << "Vlc player in an unknown state";
        return false;
    }
    // If we get here, it may be playing, rightly paused, or stopped
    // (perhaps because it completed playing in a non-repeat mode).

    // Verify the resource, if possible.  Note: it's not possible to
    // easily check that a particular playlist or directory is playing, and
    // radio should never appear!
    if ((src->medium() == Medium::playlist) or
        (src->medium() == Medium::directory) or
        (src->medium() == Medium::radio)) {
        return true;
    }
    return verify_playing_uri( src->resource() );
}

/// Check that the song or the URL VLC is playing is argument 'resource'.
/// This argument may take the following forms:
///  1. file relative path:  "Brian Eno/Another Green World/01 - Sky Saw.m4a'"
///  2. directory rel. path: "Brian Eno/Another Green World/"
///  3. playlist name:       "master.m3u"
///  4. network url:         "http://cms.stream.publicradio.org/cms.mp3"
///
/// Vlc always reports a full absolute URI, starting with "file://" e.g.
/// "file:///home/sharp/Music/Brian Eno/Another Green World/01 - Sky Saw.m4a"
/// >.........................<--- this part is m_library_uri
///
/// * Will not throw
///
bool Vlc_player::verify_playing_uri( const std::string& res ) const
{
    // TODO:
    // If URI begins with library uri,
    if (0 == m_obsuri.find( m_library_uri )) {
        std::string res_uri = m_library_uri + res;
        return (m_library_uri == res_uri);
    }
    return (m_obsuri == res);
}


/// Exit: Terminate vlc process.
/// * Will not throw.
///
void Vlc_player::exit()
{
    if (m_cm->running()) {
        try {
            shutdown_vlc();
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

/// Extract configuration values.  The first call to play will
/// actually start the child.  If not enabled, the child manager
/// is not touched and the binary path is not checked.
///
/// * May throw.
///
void Vlc_player::initialize( Config &cfg, bool testp )
{
    m_testmode = testp;
    cfg.get_bool(m_name.c_str(),"enabled",m_enabled);
    if (not m_enabled) {
        LOG_INFO(Lgr) <<"Vlc_player '" << m_name << "' (disabled)";
    }
    cfg.get_unsigned(m_name.c_str(),"volume",m_volume);
    if (m_volume > 100) {
        LOG_WARNING(Lgr) << "Vlc volume > 100: possible distortion";
        // m_volume = 100;  // allow > 100% at user's risk
    }
    cfg.get_bool(m_name.c_str(),"debug", m_debug);
    //
    m_bin_path = Default_vlc_bin;
    if (m_enabled) {
        cfg.get_pathname(m_name.c_str(),"bin_path",
                         FileCond::MustExist, m_bin_path);
        m_cm->set_binary( m_bin_path );
        m_cm->set_name( m_name );
    }
    //
    if (m_enabled) {
        LOG_INFO(Lgr) << "Vlc_player '" << m_name << "' initialized";
    }
}


/// Evaluates whether we can use VLC (only if enabled).
/// If this daemon has not been tried for m_recheck_secs since
/// last being marked unusable, attempt to start it again.
///
/// *  Will NOT throw.
///
bool Vlc_player::is_usable()
{
    if (not m_enabled) return false;
    if (m_testmode) {
        return (m_state != PlayerState::Broken);
    }
    try {
        assure_running();       // sets m_usable
    } catch(...) {
        m_usable=false;         // nix it again just in case
    }
    if (!m_usable) {
        if ( (time(0) - m_last_unusable) > m_recheck_secs ) {
            LOG_DEBUG(Lgr) << m_name << " tentatively marked as usable again";
            try_start();
        }
    }
    return m_usable;
}

/// Return enabledness. (API)
/// * Will NOT throw.
///
bool Vlc_player::is_enabled() const
{
    return m_enabled;
}

/// Enable or disable this player.
///   enabled==true:  Enable
///   enabled==false: Disable
///
/// When disabled, the player exits and goes to state Disabled.
/// When enabled, the player goes to state Stopped.
/// Return the new value of the enabled flag.
///
/// * Will not throw.
///
bool Vlc_player::set_enabled( bool enabled )
{
    bool was_enabled = m_enabled;
    if (was_enabled and not enabled) {
        exit();
        m_enabled = false;
        m_state = PlayerState::Disabled;
        LOG_WARNING(Lgr) << m_name << " is being Disabled";
    }
    else if (enabled and not was_enabled) {
        m_state = PlayerState::Stopped;
        m_enabled = true;
        LOG_WARNING(Lgr) << m_name << " is being Enabled";
    }
    return m_enabled;
}



/// Pause play. Vlc seems to know how to pause and resume a network stream.
///
/// * May throw Player_exception()
///
void Vlc_player::pause()
{
    if (m_testmode) {
        return;
    }
    assure_running();
    do_command("pause\n",true);
    m_state = PlayerState::Paused;
}


/// Start playing the source.  If src is NULL, then no further actions
/// are taken.  Anything currently playing is stopped.  The playlist is
/// cleared and only the source will be scheduled to play.  It will
/// verify that VLC is usable; if the process or connection had not
/// been initiated, it will be launched now: this is a suitable way to
/// start the VLC process/connection.
///
/// * May throw various flavors of Player_exception()
///
void Vlc_player::play( spSource src )
{
    if (not m_enabled) {
        LOG_ERROR(Lgr) << m_name << " is disabled--cannot play";
        throw Player_media_exception();
    }
    if (m_testmode) {
        LOG_DEBUG(Lgr) << m_name << ": play command ignored in test mode";
        return;
    }
    if (!src) {     // null src is not an error: means stop
        LOG_DEBUG(Lgr) << m_name << ": play null source, i.e. STOP";
        stop();
        return;
    }

    // Verify src is a valid type for Vlc
    if (not has_cap(src->medium(),src->encoding())) {
        LOG_ERROR(Lgr) << m_name << "cannot play type of source in" << src->name();
        throw Player_media_exception();
    }
    assure_running();
    if (not m_usable) {
        LOG_ERROR(Lgr) << m_name << " is not usable--cannot play";
        throw Player_startup_exception();
    }
    do_command("stop\n",true);
    do_command("clear\n",true);
    // Note that the 'repeat' command will only repeat one track, not
    // a whole playlist or directory--use 'loop' to achieve that effect.
    if ( src->repeatp() ) {
        do_command("loop on\n",true);
    } else {
        do_command("loop off\n",true);
    }
    set_volume();
    // ------------------------------------------------------------------------
    // Enqueue the resource:
    std::string qcommand {"enqueue "};  // note required trailing space
    std::string effpath;
    if (src->localp()) {
        boost::filesystem::path abspath {};
        if (not src->res_path( abspath )) { // get the expanded absolute path
            throw Player_media_exception(); // ??? best error to throw ???
        }
        effpath = abspath.native();
    } else {
        effpath = src->resource();
    }
    qcommand += effpath;
    qcommand += "\n";
    do_command( qcommand, true );
    LOG_DEBUG(Lgr) << m_name << " enqueued: `" << effpath << "`";
    // ------------------------------------------------------------------------
    do_command( "goto 1\n", true ); // essential for playlists
    do_command( "play\n", true );
    if (m_debug) {
        do_command( "playlist\n", true );
    }
    // ...success
    m_src = src;  // intended source
    m_stall_counter = 0;
    m_last_elapsed_secs = 0;
    m_state = PlayerState::Playing; // assume for now
}


/// Resume play after pause.  Should *only* be invoked after pause!
/// * May throw Player_exception
///
void Vlc_player::resume()
{
    if (m_testmode) {
        return;
    }
    assure_running();
    do_command("play\n",true);
    m_state = PlayerState::Playing;
}


/// Return current state.
///
/// * Will NOT throw
///
PlayerState Vlc_player::state()
{
    return m_state;
}


/// Stop play. If vlc is not running or plays is in test mode, this
/// does nothing.
///
/// * May throw Player_media_exception
///
void Vlc_player::stop()
{
    if (m_testmode) {
        return;
    }
    m_src.reset();
    m_state = PlayerState::Stopped;
    if (not m_cm->running()) {
        return;
    }
    do_command("stop\n",true);
}


/// Return true if this player is okay, false if not so.
/// This is the same as is_usable(), however it may return false
/// if we lose internet while playing an internet stream.
/// Does not start the player if in test mode.
///
/// * Throws NO exceptions.
///
bool Vlc_player::check()
{
    bool rc = is_usable();
    if (m_testmode) {
        return (m_state != PlayerState::Broken);
    }

    // If we are playing an MP3 stream but the internet becomes
    // unavailable, then return false.
    if (m_src and m_src->medium()==Medium::stream) {
        if (not Player_manager::inet_available()) {
            // oops -- abort playing this stream if we are trying to play
            if (m_state == PlayerState::Playing) try { stop(); } catch(...) {}
            LOG_WARNING(Lgr)
                << "Vlc_player playing stream, but there are internet problems";
            rc = false;
        }
    }
    return rc;
}
