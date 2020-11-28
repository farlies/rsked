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

/// *** Note This is meant to be included only by player.cc ***

/// This player has no audio output; it is a placeholder for silent periods
///
class Silent_player : public Player {
private:
    std::string m_name {"silent player"};
    PlayerState _state {PlayerState::Stopped};
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
};

/// Mostly nothing to do.
Silent_player::Silent_player() { }
Silent_player::~Silent_player() { }
bool Silent_player::completed() { return false; }
bool Silent_player::is_usable() { return true; }
void Silent_player::initialize( Config&, bool ) {}
void Silent_player::exit() { _state = PlayerState::Stopped; }
void Silent_player::pause() { _state = PlayerState::Paused; }
void Silent_player::resume() { _state = PlayerState::Playing; }
PlayerState Silent_player::state() { return _state; }
void Silent_player::stop() { _state = PlayerState::Stopped; }
void Silent_player::play( spSource ) { _state = PlayerState::Playing; }
bool Silent_player::check() { return true; }

/// Currently Playing Check: we only play Medium::off sources
///
bool Silent_player::currently_playing( spSource src )
{
    return( src && (src->medium() == Medium::off) );
}

