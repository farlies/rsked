#pragma once
/**
 * Control various possible player components.
 */

/*   Part of the rsked package.
 *
 *   Copyright 2020 Steven A. Harp
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
 *
 */


#include "player.hpp"
#include "inetcheck.hpp"

/// This object will retrieve/create a player for any given source.
///
class Player_manager {
private:
    Player_prefs m_prefs {};
    std::unordered_map<std::string,spPlayer> m_players {};
    void install_player( Config&, spPlayer, bool /*testp*/ );
    void load_json_prefs( Config& );
    static Inet_checker c_ichecker;
public:
    Player_manager();
    ~Player_manager();
    //
    bool check_inet();
    void check_minimally_usable();
    void configure( Config&, bool /* testp */); 
    void configure_prefs(Config&);
    bool check_players();
    void exit_players();
    spPlayer get_annunciator();
    spPlayer get_player( spSource );
    static bool inet_available();
};

