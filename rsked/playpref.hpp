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

#include <set>
#include <unordered_map>
#include <vector>

#include "source.hpp"


////////////////////////////////////////////////////////////////////

/// For every supported medium/encoding combination, keep a list in
/// priority order of the *names* of players capable of playing that
/// combination.
///
class Player_prefs {
private:
    using Player_list = std::vector<std::string>;
    std::unordered_map<int,Player_list>  m_srcmap {};
    int me_to_key (Medium m, Encoding e) const {
        return( (1 << static_cast<int>(m))
                + (0x100 << static_cast<int>(e)) );
    }
public:
    void add_player( Medium, Encoding, const std::string& );
    void clear_all();
    bool get_player( Medium, Encoding, unsigned, std::string& ) const;
    unsigned player_count( Medium, Encoding ) const;
};


/// Tracks play capabilities. This mostly wraps a std::set for a
/// Medium/Encoding pair.
///
class Player_cap_set {
private:
    using PlayCap = std::pair<Medium,Encoding>;
    std::set<PlayCap> m_caps {};
public:
    void add_cap( Medium, Encoding );
    void cap_string( std::string & ) const;
    void clear_caps();
    bool has_cap( Medium, Encoding ) const;
    void install_caps( const std::string &, Player_prefs& ) const;
};
