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

#include <memory>
#include <sys/types.h>
#include <sys/wait.h>
#include "source.hpp"
#include "playpref.hpp"

class Config;


/// Problem configuring player
struct Player_config_exception : public Player_exception {
    const char* what() const throw() { return "Player configuration exception"; }
};

/// Problem starting player binary
struct Player_startup_exception : public Player_exception {
    const char* what() const throw() { return "Player startup exception"; }
};

/// Problem communicating with player (notably for Sdr_player socket comms)
struct Player_comm_exception : public Player_exception {
    const char* what() const throw() { return "Player communication exception"; }
};

/// Problem operating the player (notably for Sdr_player commands failing)
struct Player_ops_exception : public Player_exception {
    const char* what() const throw() { return "Player operations exception"; }
};

/// Problem with the media/resource selected, e.g. nonexistent playlist or URL
struct Player_media_exception : public Player_exception {
    const char* what() const throw() { return "Player media exception"; }
};


/**
 * Abstract Player interface. Real players inherit from this and define
 * concrete methods for each member function.
 */
class Player {
public:
    virtual ~Player()=0;
    //
    virtual const std::string& name() const = 0;
    virtual bool completed()=0;
    virtual bool currently_playing( spSource )=0;
    virtual void exit()=0;
    virtual void initialize( Config&, bool )=0;
    virtual bool is_usable()=0;
    virtual void pause()=0;
    virtual void play( spSource )=0;
    virtual void resume()=0;
    virtual PlayerState state()=0;
    virtual void stop()=0;
    virtual bool check()=0;
    //
    virtual bool has_cap( Medium, Encoding ) const = 0;
    virtual void cap_string( std::string & ) const = 0;
    virtual void install_caps( Player_prefs& ) const = 0;
    virtual bool is_enabled() const = 0;
    virtual bool set_enabled( bool )= 0;
};

/// Shared pointer to a Player.
using spPlayer = std::shared_ptr<Player>;


/// Player classes can inherit from Player_with_caps instead of
/// directly from pure Player gaining the std::set implementation
/// of the capabilities interface.
///
class Player_with_caps : public Player {
private:
    Player_cap_set m_capset {};
protected:
    void add_cap( Medium, Encoding );
    void clear_caps();
public:
    virtual bool has_cap( Medium, Encoding ) const;
    virtual void cap_string( std::string & ) const;
    virtual void install_caps( Player_prefs& ) const;
};

