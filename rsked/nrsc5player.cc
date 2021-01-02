/// The nrsc5 player runs nrsc5123 on a playable slot.

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
void Nrsc5_player::cap_init()
{
    clear_caps();
    add_cap(Medium::radio,     Encoding::wfm);
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

/// DTOR.  Baseplayer will kill any child process.
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


/// Play the given source (a station).
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
                  << freq_mhz << " MHz, HD1"
                  << ", SDR device " << m_device_index;
    //
    m_cm->clear_args();
    m_cm->add_arg( "-q" );  // run quietly -- no console logging
    //
    m_cm->add_arg( "-d" );  // device index (0 is typical)
    m_cm->add_arg( m_device_index );
    //
    m_cm->add_arg( freq_mhz ); //
    m_cm->add_arg( "0" );     // HD program. TODO: make a src argument
    m_cm->start_child();
    m_pstate = PlayerState::Playing;
}

