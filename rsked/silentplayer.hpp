#pragma once

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

#include "config.hpp"
#include "schedule.hpp"

///////////////////////////////////////////////////////////////////////////
///                               Silent Player 

/// *** Note This is meant to be included only by playermgr.cc ***

constexpr const char *SilentName = "Silent_player";

/// This player has no audio output; it is a placeholder for silent periods
/// It is always usable, never registers completed, and cannot be disabled.
///
class Silent_player : public Player {
private:
    std::string m_name { SilentName };
    PlayerState m_state {PlayerState::Stopped};
public:
    Silent_player();
    virtual ~Silent_player();
    //
    virtual const std::string& name() const { return m_name; }
    virtual bool completed();
    virtual bool currently_playing( spSource );
    virtual void exit();
    virtual void initialize( Config&, bool );
    virtual bool is_usable();
    virtual void pause();
    virtual void play( spSource );
    virtual void resume();
    virtual PlayerState state();
    virtual void stop();
    virtual bool check();
    //
    virtual bool has_cap( Medium, Encoding ) const;
    virtual void cap_string( std::string & ) const;
    virtual void install_caps( Player_prefs& ) const;
    virtual bool is_enabled() const;
    virtual bool set_enabled( bool );
};

/// Mostly nothing to do.
Silent_player::Silent_player() { }
Silent_player::~Silent_player() { }
bool Silent_player::completed() { return false; }
bool Silent_player::is_usable() { return true; }
void Silent_player::initialize( Config&, bool ) {}
void Silent_player::exit() { m_state = PlayerState::Stopped; }
void Silent_player::pause() { m_state = PlayerState::Paused; }
void Silent_player::resume() { m_state = PlayerState::Playing; }
PlayerState Silent_player::state() { return m_state; }
void Silent_player::stop() { m_state = PlayerState::Stopped; }
void Silent_player::play( spSource ) { m_state = PlayerState::Playing; }
bool Silent_player::check() { return true; }
bool Silent_player::is_enabled() const { return true; }
bool Silent_player::set_enabled(bool) { return true; }

/// Currently Playing Check: we only play Medium::off sources
///
bool Silent_player::currently_playing( spSource src )
{
    return( src && (src->medium() == Medium::off) );
}

/// Only plays "off"
bool Silent_player::has_cap( Medium m, Encoding ) const
{
    return( m == Medium::off );
}

/// Only plays "off"
void Silent_player::cap_string( std::string &cstr ) const
{
    cstr = "off:none";
}

/// Plays anything "off"
void Silent_player::install_caps( Player_prefs &prefs ) const
{
    prefs.add_player( Medium::off, Encoding::none, m_name );
    prefs.add_player( Medium::off, Encoding::ogg, m_name );
    prefs.add_player( Medium::off, Encoding::mp3, m_name );
    prefs.add_player( Medium::off, Encoding::mp4, m_name );
    prefs.add_player( Medium::off, Encoding::flac, m_name );
    prefs.add_player( Medium::off, Encoding::wfm, m_name );
    prefs.add_player( Medium::off, Encoding::nfm, m_name );
    prefs.add_player( Medium::off, Encoding::mixed, m_name );
}
