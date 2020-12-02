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

#include "player.hpp"
#include "childmgr.hpp"


/// Base class for simple media players like Mp3_player and Ogg_player
/// that involve just starting an external process.
/// Not instantiable.
///
class Base_player : public Player_with_caps {
protected:
    spSource m_src {};
    const unsigned _max_restarts { 2 };
    const time_t _restart_interval { 10 };
    PlayerState m_pstate { PlayerState::Stopped };
    std::string m_name { "Base_player" };
    std::string m_device { };   // audio device e.g. "hw:0,0"
    std::string m_device_type { };  // api, e.g. "alsa"
    boost::filesystem::path m_wdir {};
    bool m_enabled { true };
    spCM  m_cm { Child_mgr::create(m_name) };
    bool attempt_restart();
    bool maybe_restart(RunCond);
public:
    Base_player();
    Base_player(const char *);
    virtual ~Base_player();
    Base_player(const Base_player&) = delete;
    void operator=(Base_player const&) = delete;
    //
    virtual const std::string& name() const { return m_name; }
    virtual bool completed();
    virtual bool currently_playing( spSource );
    virtual void exit();
    virtual void initialize( Config&, bool )=0;
    virtual bool is_usable();
    virtual void pause();
    virtual void play( spSource )=0;
    virtual void resume();
    virtual PlayerState state();
    virtual void stop();
    virtual bool check();
    virtual bool is_enabled() const;
    virtual bool set_enabled( bool );
};

