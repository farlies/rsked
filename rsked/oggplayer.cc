/// The ogg player runs ogg123 on a playable slot.

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

#include "oggplayer.hpp"
#include "config.hpp"
#include "schedule.hpp"

/// Where to find binary unless otherwise set in Config:
static const boost::filesystem::path DefaultBinPath {"/usr/bin/ogg123"};

//////////////////////////////////////////////////////////////////////////////

/// CTOR
Ogg_player::Ogg_player() : Base_player("Ogg_player")
{
    LOG_INFO(Lgr) << "Created an Ogg_player";
    m_cm->set_min_run( 2 );    // shortest ogg we might ever play, seconds
}

/// CTOR with name.
/// You may set a shortest run length, which might be 0 for a brief announcment.
///
Ogg_player::Ogg_player( const char* nm, time_t min_run_secs )
    : Base_player( nm )
{
    LOG_INFO(Lgr) << "Created an Ogg_player: " << m_name;
    m_cm->set_min_run( min_run_secs );
}

/// DTOR.  Baseplayer will kill any child process.
Ogg_player::~Ogg_player()
{
}

/// Init: extract binary path for ogg player
///
void Ogg_player::initialize( Config& cfg, bool /* testp */ )
{
    namespace fs = boost::filesystem;

    cfg.get_bool("Ogg_player", "enabled" ,m_enabled);
    if (not m_enabled) {
        LOG_INFO(Lgr) << "Ogg_player '" << m_name << "' (disabled)";
        // if not enabled, we do not check the rest of the configuration
        return;
    }

    fs::path binpath { DefaultBinPath };
    cfg.get_pathname("Ogg_player","ogg_bin_path",
                 FileCond::MustExist, binpath);
    m_cm->set_binary( binpath );
    //
    fs::path wkdir {"."};
    if (cfg.get_pathname("Ogg_player","working_dir",
                         FileCond::MustExistDir, wkdir)) {
        m_wdir = wkdir;
        m_cm->set_wdir( m_wdir );
    }

    LOG_INFO(Lgr) << "Ogg_player '" << m_name << "' initialized";
}


/// Play the given slot (if you can). Setting the src to nullptr stops
/// the player and resets the source.
/// NOTE:  No support for ogg streams yet.
///
void Ogg_player::play( spSource src )
{
    if (!src) {
        m_src = src;
        stop();
        return;
    }
    if (src->encoding() != Encoding::ogg) {
        LOG_ERROR(Lgr) << m_name <<  " cannot play this type of source: "
                       << encoding_name( src->encoding() );
        return;
    }
    m_src = src;
    LOG_INFO(Lgr) << m_name << " play: {" << m_src->name() << "}";
    m_cm->clear_args();
    m_cm->add_arg( "-q" );  // quietly -- no console play
    if (not m_device_type.empty()) {
        m_cm->add_arg( "-d" );
        m_cm->add_arg( m_device_type );
    }
    if (m_src->repeatp()) {
        m_cm->add_arg( "--repeat" );  // repeat this indefinitely
        LOG_INFO(Lgr) << m_name << " will repeat the program for entire period";
    }
    if (m_src->medium()==Medium::playlist) {
        m_cm->add_arg("--list");      // for playlist
    }
    if (m_src->medium()==Medium::stream) {
        m_cm->add_arg( m_src->resource().c_str() );
    } else {
        boost::filesystem::path path;
        m_src->res_path( path );       // returns false if not exist...
        m_cm->add_arg( path.c_str() ); // might be expanded path
    }
    m_cm->start_child();
    m_pstate = PlayerState::Playing;
}

