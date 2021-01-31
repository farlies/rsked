/// The nrsc5 player runs nrsc5 on a playable slot.

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

#include "nrsc5player.hpp"
#include "config.hpp"
#include "schedule.hpp"

/// Where to find binary if not specified in the configuration json:
static const boost::filesystem::path DefaultBinPath {"/usr/local/bin/nrsc5"};

//////////////////////////////////////////////////////////////////////////////

/// Establish baseline capabilities. shared by all ctors
/// This only works on HD modulation on FM stations.
/// AM HD-Radio might be added at some future date...
///
void Nrsc5_player::cap_init()
{
    clear_caps();
    add_cap(Medium::radio,     Encoding::hd1fm);
    add_cap(Medium::radio,     Encoding::hd2fm);
    add_cap(Medium::radio,     Encoding::hd3fm);
    add_cap(Medium::radio,     Encoding::hd4fm);
    //
    std::string cstr;
    cap_string( cstr );
    LOG_DEBUG(Lgr) << m_name << " " << cstr;
}

/// CTOR
Nrsc5_player::Nrsc5_player() : Base_player("Nrsc5_player")
{
    LOG_INFO(Lgr) << "Created an Nrsc5_player";
    m_cm->set_min_run( 7 );    // shortest expected run time
    cap_init();
}

/// CTOR with name.
/// You may set a shortest run length, which might be 0 for a brief announcement.
///
Nrsc5_player::Nrsc5_player( const char* nm, time_t min_run_secs )
    : Base_player( nm )
{
    LOG_INFO(Lgr) << "Created an Nrsc5_player: " << m_name;
    m_cm->set_min_run( min_run_secs );
    cap_init();
}

/// DTOR.  Note: baseplayer dtor will kill any child process.
Nrsc5_player::~Nrsc5_player()
{
}

/// Init: extract binary path for nrsc5 player
///
void Nrsc5_player::initialize( Config& cfg, bool testp )
{
    namespace fs = boost::filesystem;
    const char* myName = m_name.c_str();
    m_test_mode = testp;

    cfg.get_bool( myName, "enabled" ,m_enabled);
    if (not m_enabled) {
        LOG_INFO(Lgr) << "Nrsc5_player '" << m_name << "' (disabled)";
        return;
    }
    fs::path binpath { DefaultBinPath };
    cfg.get_pathname( myName, "nrsc5_bin_path",
                      FileCond::MustExist, binpath);
    m_cm->set_binary( binpath );
    //
    m_device_index = 0;
    cfg.get_unsigned( myName, "device_index", m_device_index );

    // TODO: probe this device e.g. via rtl_test

    LOG_INFO(Lgr) << "Nrsc5_player named '" << m_name << "' initialized";
}


/// Pause: nrsc5 does not explicitly handle SIGSTOP (or any other signal)
/// so die instead, but declare we are in a "Paused" state.
///
void Nrsc5_player::pause()
{
    exit();
    m_pstate = PlayerState::Paused;  // pretend we are "paused"
}

/// If paused, then execute play for the most recent source.
/// * May throw
///
void Nrsc5_player::resume()
{
    spSource src = m_src;
    if (! src) {
        LOG_ERROR(Lgr) << m_name << " asked to resume, but source is UNdefined";
        throw Player_media_exception();
    }
    play( src );
}

/// Exit will terminate external player process, if any.
/// TODO: better to send 'Q' to the player pty first...
/// * will not throw
///
void Nrsc5_player::exit()
{
    LOG_INFO(Lgr) << " signal " << m_name << "to exit (KILL)";
    m_cm->kill_child( true, m_kill_us ); 
    m_pstate = PlayerState::Stopped;
}

/// Play the given source (a station).
/// * May throw
///
void Nrsc5_player::play( spSource src )
{
    if (not m_enabled) {
        LOG_ERROR(Lgr) << m_name << " is disabled--cannot play";
        throw Player_media_exception();
    }
    if (m_test_mode) {
        LOG_DEBUG(Lgr) << m_name << ": play command ignored in test mode";
        return;
    }
    if (!src) {                 // null source is same as STOP
        m_src = src;
        stop();
        return;
    }
    if (not has_cap(src->medium(),src->encoding())) {
        LOG_ERROR(Lgr) << m_name << "cannot play type of source in" << src->name();
        throw Player_media_exception();
    }
    // If already running but playing the wrong station, then stop.
    // If running and playing the right station, then just return.
    if (m_cm->running()) {
        if (m_src and m_src->freq_hz() == src->freq_hz()) {
            return;
        }
        stop();
    }
    // Set up arguments and start child process.
    m_src = src;
    std::string freq_mhz = std::to_string( m_src->freq_mhz() );
    LOG_INFO(Lgr) << m_name << " play: {" << m_src->name() << "}  "
                  << freq_mhz << " MHz, "
                  << encoding_name(m_src->encoding())
                  << ", SDR device " << m_device_index;
    //
    m_cm->clear_args();
    m_cm->add_arg( "-q" );  // run quietly -- no console logging
    //
    m_cm->add_arg( "-d" );  // device index (0 is typical)
    m_cm->add_arg( m_device_index );
    //
    m_cm->add_arg( freq_mhz ); //
    switch (src->encoding()) {
    case Encoding::hd1fm :
        m_cm->add_arg( "0" );
        break;
    case Encoding::hd2fm :
        m_cm->add_arg( "1" );
        break;
    case Encoding::hd3fm :
        m_cm->add_arg( "2" );
        break;
    case Encoding::hd4fm :
        m_cm->add_arg( "3" );
        break;
    default:
        LOG_WARNING(Lgr) << m_name << " asked to play unknown encoding";
        m_cm->add_arg( "0" );   // should never occur, but...
        break;
    }
    m_cm->start_child();
    m_pstate = PlayerState::Playing;
}

