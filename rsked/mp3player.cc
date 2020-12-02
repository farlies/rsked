/**
 * This mp3 player runs mpg321 on a playable mp3 source.
 * Note: will not handle mp4.
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

#include "mp3player.hpp"
#include "config.hpp"
#include "schedule.hpp"

/// Where to find binary unless otherwise set in Config:
static const boost::filesystem::path DefaultBinPath {"/usr/bin/mpg321"};


/// Establish baseline capabilities. Shared by all ctors.
void Mp3_player::cap_init()
{
    clear_caps();
    add_cap(Medium::file,       Encoding::mp3);
    add_cap(Medium::directory,  Encoding::mp3);
    add_cap(Medium::playlist,   Encoding::mp3);
    add_cap(Medium::stream,     Encoding::mp3);
    //
    std::string cstr;
    cap_string( cstr );
    LOG_DEBUG(Lgr) << m_name << " " << cstr;
}

/// CTOR
///  Quirk: mpg321 will exit(0) immediately if fed a resource that is not
///  a valid mp3 file/stream. We set a nonzero min_run to flag this as a
///  failure. Contrast to ogg123, which will exit(1) if fed a bad file.
///
Mp3_player::Mp3_player() : Base_player("Mp3_player")
{
    LOG_INFO(Lgr) << "Created an Mp3_player";
    m_cm->set_min_run( 2 );    // shortest mp3 we might ever play, seconds
    cap_init();
}

/// CTOR with name
Mp3_player::Mp3_player( const char* nm )
    : Base_player(nm)
{
    LOG_INFO(Lgr) << "Created an Mp3_player: " << m_name;
    m_cm->set_min_run( 2 );    // shortest mp3 we might ever play, seconds
    cap_init();
}

/// DTOR.  Baseplayer will kill any child process.
Mp3_player::~Mp3_player()
{
}


/// Init: extract binary path for mp3 player
///
void Mp3_player::initialize( Config& cfg, bool /* testp */ )
{
    const char *section=m_name.c_str();

    cfg.get_bool(section, "enabled" ,m_enabled);
    if (not m_enabled) {
        LOG_INFO(Lgr) << "Mp3_player '" << m_name << "' (disabled)";
    }

    boost::filesystem::path binpath { DefaultBinPath };
    cfg.get_pathname(section, "mp3_bin_path", FileCond::MustExist, binpath );
    m_cm->set_binary( binpath );
    LOG_INFO(Lgr) << m_name << " initialized";
}


/// Play the given slot (if you can).
/// If src is null, then stop the player.
///
void Mp3_player::play( spSource src )
{
    if (!src) {
        m_src = src;
        stop();
        return;
    }
    if ((src->medium() != Medium::stream) 
        and (src->medium() != Medium::file)) {
        LOG_ERROR(Lgr) << m_name << " cannot play this type of slot: "
                       << media_name(src->medium());
        return;
    }
    m_src = src;
    LOG_INFO(Lgr) << m_name << " play: {" << m_src->name() << "}";
    m_cm->clear_args();
    m_cm->add_arg( "-q" );  // quietly -- no console play
    if (m_src->repeatp()) {
        m_cm->add_arg( "--loop" );  // repeat this a lot, but not forever
        m_cm->add_arg( "100" );     // since it might be a bad resource
        LOG_INFO(Lgr) << "Mp3_player will repeat the program up to 100x";
    }
    const Medium rt = m_src->medium();
    if (rt==Medium::directory) {
        m_cm->add_arg( "-B" );  // recursively descend directories
    }
    if (rt==Medium::playlist) {
        m_cm->add_arg( "--list" );  // this resource is a playlist
    }
    if (rt==Medium::stream) {
        m_cm->add_arg( m_src->resource().c_str() );
        LOG_DEBUG(Lgr) << "Stream URL: " << m_src->resource();
    } else {
        boost::filesystem::path path;
        m_src->res_path( path );       // returns false if not exist...
        m_cm->add_arg( path.c_str() ); // might be expanded path
    }
    m_cm->start_child();
    m_pstate = PlayerState::Playing;
}

