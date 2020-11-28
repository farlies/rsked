/**
 *  Implements a radio player that uses our modified gqrx SDR application.
 */

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
#include "schedule.hpp"
#include "usbprobe.hpp"
#include "gqrxclient.hpp"
#include "sdrplayer.hpp"


/// Default binary path 
constexpr const char* Gqrx_bin_path = "/opt/gqrx/build/gqrx";

/// S-meter thresholds
const double Low_s { -20.0 };
const double Low_low_s { -40.0 };

/// GQRX receiver configuration file pathnames (Sdr_player)
constexpr const char* Gqrx_config_gold = "~/.config/gqrx/gold.conf";
constexpr const char* Gqrx_config_work = "~/.config/gqrx/gqrx.conf";

/// GQRX network port and host
constexpr const unsigned Gqrx_port = 7356;
constexpr const char *Gqrx_host = "127.0.0.1";


////////////////////////////// SDR_PLAYER /////////////////////////////////////

/// CTOR
Sdr_player::Sdr_player() 
    : m_remote( std::make_unique<gqrx_client>(this) ),
      m_cm( Child_mgr::create( m_name ) )
{
    LOG_INFO(Lgr) << "Created an Sdr_player";
}

/// DTOR.  Disconnects then deletes.
Sdr_player::~Sdr_player()
{
    m_remote->disconnect();
    m_cm->kill_child();
}

/// Return Usability flag.  Might be not usable if:
///   - player is disabled in the configuration
///   - there is no SDR hardware available
///   - cannot run the gqrx binary, e.g. missing
///   - cannot connect to the gqrx command socket in a timely fashion
///   - gqrx has runtime error, e.g. not run in X-session--oops
/// If m_usable is false, it will be rechecked every so-many hours
/// since it is possible that e.g. a station might be return to the
/// air, an SDR might be hotplugged, etc.  During normal operations
/// this is unlikely to work, but is useful for testing purposes.
///
bool Sdr_player::is_usable()
{
    if (!m_enabled) return false;
    if (!m_usable) {
        if ( (time(0) - m_last_unusable) > m_recheck_secs ) {
            mark_unusable(false);
        }
    }
    return m_usable;
}

/// Flag the player as UNusable (p==TRUE), or usable again (p==false).
/// Kills any running child process.
///
void Sdr_player::mark_unusable( bool unusablep )
{
    m_usable = ( ! unusablep );

    if (!m_usable) {
        m_last_unusable = time(0);
        LOG_WARNING(Lgr) << "Sdr_player being marked as Unusable until future notice";
        m_cm->kill_child();           //  < ! >
        m_state = PlayerState::Broken;
    } else {
        LOG_WARNING(Lgr) << "Sdr_player is being tentatively marked as usable again";
        m_state = PlayerState::Stopped;
    }        
}

/// Determine if there are any matching radios on the USB bus.
/// If the config file specifies a device_vendor and device_product
/// use just that one, otherwise use the default set of SDRs.
///
bool Sdr_player::probe_sdr(Config& cfg)
{
    Usb_probe probe;
    std::string vendor_str;
    const char* myname = m_name.c_str();
    
    if (cfg.get_string(myname,"device_vendor",vendor_str)) {
        std::string product_str;
        if (cfg.get_string(myname,"device_product",product_str)) {
            unsigned long vendor=0,product=0;
            try {  // convert from hex - may fail
                vendor = std::stoul(vendor_str, nullptr, 16 );
                product = std::stoul(product_str, nullptr, 16 );
            } catch (  std::invalid_argument& ) {
                LOG_ERROR(Lgr) << "Invalid SDR device: " << vendor_str
                               << ":" << product_str << " for " << m_name;
                throw Config_error();
            }
            probe.clear_devices();
            probe.add_device(static_cast<uint16_t>(vendor & 0xFFFF),
                             static_cast<uint16_t>(product & 0xFFFF));
        } else {
            LOG_ERROR(Lgr) << "Missing device_product for SDR " << m_name;
        }
    }
    if (0 == probe.count_devices(true)) {
        if (m_usable) {
            LOG_WARNING(Lgr) << "Sdr_player found no recognized SDR devices";
            mark_unusable(true);
        }
        return false;
    }
    return true;
}


/// Extracts paths and parameters for GQRX, (re)initializes members,
/// probes for SDR devices.
///
void Sdr_player::initialize( Config& cfg, bool /* testp */ )
{
    const char* myname = name().c_str();
    m_src = nullptr;
    m_enabled = true;
    m_usable = true;
    m_last_s = (-1000.0);
    m_freq = 0;
    m_state = PlayerState::Stopped;

    cfg.get_bool(myname, "enabled" ,m_enabled);
    if (not m_enabled) {
        LOG_INFO(Lgr) << "Sdr_player '" << m_name << "' (disabled)";
        // if not enabled, we do not check the rest of the configuration
        return;
    }

    // gqrx configuration info
    boost::filesystem::path binpath { Gqrx_bin_path };
    cfg.get_pathname(myname,"gqrx_bin_path", FileCond::MustExist, binpath);
    m_cm->set_binary( binpath );
    //
    // Note: the working config file will be created by copying the gold file
    //  each time gqrx is started. This gets around the policy of gqrx to
    //  add a disabler to configuration files it thinks may have caused a crash.
    m_config_work = Gqrx_config_work;
    cfg.get_pathname(myname, "gqrx_work", FileCond::NA, m_config_work);
    //
    m_config_gold = expand_home(Gqrx_config_gold);
    cfg.get_pathname(myname, "gqrx_gold", FileCond::MustExist, m_config_gold);

    // S-level thresholds
    m_low_s = Low_s;
    cfg.get_double(myname, "low_s", m_low_s);
    m_low_low_s = Low_low_s;
    cfg.get_double(myname, "low_low_s", m_low_low_s);
    if (m_low_low_s > m_low_s) {
        LOG_WARNING(Lgr) << "lowlow_s > low_s ; adjusting.";
        m_low_low_s = (m_low_s - 10.0);
    }

    // Gqrx communications info
    std::string host=Gqrx_host;
    unsigned port = Gqrx_port;
    cfg.get_string(myname,"gqrx_host",host);
    cfg.get_unsigned(myname,"gqrx_port",port);
    m_remote->set_hostport( host, port );
    //
    probe_sdr(cfg);
    //
    LOG_INFO(Lgr) << myname << " initialized";
}




/// The gqrx application takes a configuration file that
/// must be correct (or it forces a deadly modal dialog to fix it).
/// Copy a 'gold' copy of the config to the operational
/// position.  May throw.
///
void Sdr_player::setup_gqrx_config()
{
    using namespace boost::filesystem;
    if (exists(m_config_gold)) {
        copy_file( m_config_gold, m_config_work,
                   copy_option::overwrite_if_exists );
    } else {
        LOG_ERROR(Lgr)
            << "Sdr_player Gold config file is *missing*:" << m_config_gold;
        mark_unusable(true);
        throw Player_config_exception();
    }
}


/// Exit: Terminate external player, if any
///
void Sdr_player::exit()
{
    if (m_cm->running()) {
        try {
            m_remote->disconnect();
            m_cm->kill_child();
            LOG_INFO(Lgr) << "Sdr_player exits";
            m_state = PlayerState::Stopped;
        }
        catch (...) {
            // Child_mgr logs error message
        }
    } else {
        LOG_INFO(Lgr) << "Sdr_player already exited";
    }
}

/// This is the same as Stop
///
void Sdr_player::pause()
{
    stop();
    m_state = PlayerState::Paused;
}

/// If paused, then resume demodulation
///
void Sdr_player::resume()
{
    if (m_state == PlayerState::Paused) {
        play( m_src );
    } else {
        LOG_WARNING(Lgr) << "Sdr_player resume() called but not paused.";
    }        
}

/// Return the current PlayerState
///
PlayerState Sdr_player::state()
{
    return m_state;
}


/// Stop playing. This will not terminate the child process,
/// just halt demodulation and audio output--this is a low
/// power state. May throw.
///
void Sdr_player::stop()
{
    try_connect();
    LOG_INFO(Lgr) << "Sdr_player stop demodulation";
    m_remote->stop_dsp();
    m_state = PlayerState::Stopped;
}

/// Play the given slot (if you can).  If the gqrx process is already
/// running, then just adjust the frequency as needed; otherwise start
/// the process first and connect to it.  A null src will cause the
/// player to simply stop.  This operation may fail for various
/// reasons and will throw.
///
void Sdr_player::play( spSource src )
{
    if (!src) {
        m_src = src;
        stop();
        return;
    }
    if (src->medium() != Medium::radio) {
        LOG_ERROR(Lgr)
            << "Sdr_player cannot play this type of source {"
            << src->name() << "}";
        return;
    }
    try {
        m_src = src;
        LOG_INFO(Lgr) << "Sdr_player play {" << m_src->name() << "}";
        if ( !m_cm->running() ) { try_start(); }
        try_connect();
        set_program();
    }
    catch (Player_exception &ex) {
        LOG_ERROR(Lgr) << "Sdr_player play(): " << ex.what();
        m_src = nullptr;
        mark_unusable(true);
        m_cm->kill_child();
        throw;
    }
    m_state = PlayerState::Playing;
}

/// Attempt to start the child process and connect to its command
/// socket.  On success, it will return and m_usable will be set
/// to true.  It may also throw any ChildMgr or try_connect exception.
///
void Sdr_player::try_start()
{
    setup_gqrx_config();
    m_cm->clear_args();
    m_cm->add_arg( "-c" );          // specify working config file
    m_cm->add_arg( m_config_work.c_str() );
    m_cm->start_child();
    try_connect();
    m_usable = true;
}    


/// This is to debug by looking at the screen when gqrx hangs....
///
static int take_screenshot(const char* fname)
{
    std::string command("/usr/bin/scrot ");
    command += fname;
    int res = system( command.c_str() );
    LOG_DEBUG(Lgr) << "Sdr_player Took a screen shot of the devil";
    return res;
}


/// Try to connect to remote, up to N times, with brief delays.  Note
/// that the program is quite slow to start on platforms like RPi.  If
/// connection fails, mark as unusable, kill the child process, and
/// throw a Player_comm_exception.
///
void Sdr_player::try_connect()
{
    const unsigned max_attempts=15U;
    const int attempt_delay_secs=5;
    //
    if (m_remote->connected()) return; // already connection

    for (unsigned itry=1; itry<=max_attempts; itry++) {
        sleep(attempt_delay_secs);
        if (m_remote->connect()) {
            LOG_INFO(Lgr)
                << "Sdr_player Connected to Rx on attempt " << itry ;
            return;
        }
    }
    LOG_ERROR(Lgr)
        << "Sdr_player failed to connect to gqrx after "
        << max_attempts << " attempts";
    mark_unusable(true);
    take_screenshot("logs/D%Y-%m-%d_%H:%M:%S.png");
    throw( Player_comm_exception() );
}


/// Did the player exit?
///
bool Sdr_player::completed()
{
    return m_cm->completed();
}


/// Does a check to verify the child process is playing the
/// resource named in ps. Returns false if:
///  -- player is playing something else
///  -- player is in the wrong state, e.g. gone, paused
///  -- radio signal strength is too low to be usable
/// The current source will be marked as "failed" if a problem is detected.
/// It is up to the caller to rectify the problem if false is returned.
///
bool Sdr_player::currently_playing( spSource src )
{
    if ( m_src != src ) {
        // LOG_WARNING(Lgr) << "Sdr_player playing something else.";
        return false;
    }
    ChildPhase ophase = m_cm->last_obs_phase();
    if ( m_cm->cmd_phase() == ophase ) {
        if ((m_cm->cmd_phase() == ChildPhase::running)
            && (Smeter::lowlow == check_signal())) {
            if (m_src) { m_src->mark_failed(); } // bad station?
            return false;       // signal too weak
        }
        return true;
    }
    // in the wrong phase
    return false;
}

/// Somehow gqrx process got stopped(!?) Resume it and set demod
/// accordingly. Unlike other players, we do not use SIGSTOP to pause
/// gqrx, but simply disable demodulation
///
/// \returns true if the continue signal was sent
///
bool Sdr_player::cont_gqrx()
{
    try {
        m_cm->cont_child();
        return true;
    } catch(CM_exception &) {
        return false;           // Child_mgr already logged the issue
    }
}

/// Verify demodulation is on or off depending on play state.
/// It should only be on in state Playing.
///
/// \returns true iff demodulation is/becomes consistent with player state
///
bool Sdr_player::check_demod()
{
    try {
        bool pdsp = m_remote->get_dsp();
        if (m_state == PlayerState::Playing){
            if (pdsp) return true;
            m_remote->start_dsp();
            return true;
        } else {
            if (!pdsp) return true;
            m_remote->stop_dsp();
            return true;
        }
    } catch (Player_ops_exception&) {
        return false;
    }
}

/// If Playing, verify that the frequency is correct and signal strength
/// is adequate.
///
/// \returns true if either not playing, or, playing right freq at good level
///
bool Sdr_player::check_play()
{
    if (m_state != PlayerState::Playing) {
        return true;
    }
    if (check_signal() == Smeter::lowlow) {
        return false;
    }
    return true;
}


/// Return true if the player is okay, or can be made okay.
/// Return false otherwise.  Conditions it will check:
/// 1. Child process health: running if player state is Paused or Playing
/// If Playing:
/// 2. Frequency is correct
/// 3. Signal strength is acceptable
///
bool Sdr_player::check()
{
    ChildPhase obs_ph=m_cm->last_obs_phase();

    // if player is marked as broken, kill the child process
    if (m_state == PlayerState::Broken) {
        // TODO: kill_gqrx();
        return true;
    }

    if (obs_ph == ChildPhase::paused) { // Child STOPPED?
        return cont_gqrx();
    }
 
    /* TODO!!!
    if (m_cm->maybe_child_died(status)) {
        m_remote->disconnect();  // tcp client will be oblivous to death
        if (nullptr == m_src) {
            LOG_INFO(Lgr) << "Sdr_player exited while not playing anything?";
            return true;
        }
        LOG_INFO(Lgr)
            << "Sdr_player exited while playing {" << m_src->name() << "}";

        // Attempting RESTART up to k times if we are supposed to still be
        // running
        if ((ChildPhase::running == m_cm->cmd_phase())
            || (ChildPhase::started == m_cm->cmd_phase())) {
            LOG_INFO(Lgr) 
                << "Sdr_player Attempt to restart for source {"
                << m_src->name() << "}";
            // TODO: restart, count up
            play( m_src );
        } else {
            // Not supposed to be running - clear m_src ptr.
            m_src = nullptr;
        }
        return true;
    }
    return false;
    */
    return true;
}


/// Do an elementary test of the remote gqrx and switch receiver
/// mode as needed to match source. If this fails, then mark
/// the player as unusable and rethrow the error.
///
void Sdr_player::set_program()
{
    if (!m_src) { return; }
    m_freq = m_src->freq_hz();
    try {
        bool d = m_remote->get_dsp();
        if (m_src->medium() == Medium::radio) {
            if (!d) {
                m_remote->set_freq(m_freq);
                m_remote->start_dsp();
                LOG_INFO(Lgr)
                    << "Sdr_player:  Enable receiver @ " << m_freq;
            } else {
                unsigned long f = m_remote->get_freq();
                if (f != m_freq) {
                    m_remote->set_freq( m_freq );
                    LOG_INFO(Lgr)  << "Sdr_player"
                                   << "  Change frequency to " << m_freq;
                }
            }
        }
        else if (d) {  // playing, but not in radio mode?!
            m_remote->stop_dsp();
            LOG_INFO(Lgr) << "Sdr_player: Switched Rx to Standby mode" ;
        }
    } catch(Player_exception &ex) {
        LOG_ERROR(Lgr)
            << "Sdr_player set_program() failure: " << ex.what();
        mark_unusable(true);
        throw;
    }
}

/// Check signal strength, returning one of: lowlow, low, good, unavailable
/// Will not throw.
///
Smeter Sdr_player::check_signal()
{
    Smeter strength = Smeter::good;
    const time_t SETTLING_SECS = 5; // unreliable unless running this long
    const unsigned CHECK_FREQ = 150; // log s-level every this many checks

    try {
        bool settled = (m_cm->uptime() > SETTLING_SECS);
        double s = m_remote->get_smeter();
        ++m_check_count;
        if ((s < m_low_s)  && settled) {
            if (s < m_low_low_s) {
                strength = Smeter::lowlow;
                LOG_WARNING(Lgr)
                    << "Sdr_player s=" << s << " for {" << m_src->name()
                    << "}; station lost?" ;
            } else {
                strength = Smeter::low;
                LOG_WARNING(Lgr)
                    << "Sdr_player reception is weak s=" << s 
                    << " for {" << m_src->name() << "}";
            }
        } else if (0==(m_check_count % CHECK_FREQ) and settled) {
            LOG_INFO(Lgr) << "Sdr_player s-level = " << s;
        }
        m_last_s = s;
    }
    catch(...) {
        strength = Smeter::unavailable;
    }
    return strength;
}
