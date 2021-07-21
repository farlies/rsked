/**
 * This module has the brief implementations needed by classes
 * supporting the player preference mechanism:
 * - Player_prefs
 * - Player_cap_set
 * - Player_with_caps
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

#include <algorithm>
#include "player.hpp"


//////////////////////////////////////////////////////////////////////////////
///                             Player_prefs

/// Return number of players, n, capable of rendering Medium+Encoding
/// These are indexed [0...n-1] in get_player().
///
unsigned Player_prefs::player_count( Medium m, Encoding e ) const
{
    auto it = m_srcmap.find( me_to_key(m,e) );
    if (it == m_srcmap.end()) {
        return 0;
    }
    return static_cast<unsigned>( it->second.size() );
}

/// Retrieve the jth player for the m+e mode, setting n on success
/// and returning true.  If not available, then return false.
///
bool
Player_prefs::get_player( Medium m, Encoding e, unsigned j, std::string &pstr)
    const
{
    auto it = m_srcmap.find( me_to_key(m,e) );
    if (it == m_srcmap.end()) {
        return false;
    }
    unsigned pc = static_cast<unsigned>(it->second.size());
    if (j >= pc) {
        return false;
    }
    pstr = it->second[j];
    return true;
}

/// Append the player name for the medium+encoding. Will be lower priority than
/// any player already added for that m+e.
///
void Player_prefs::add_player(Medium m, Encoding e, const std::string &name)
{
    int key = me_to_key( m, e );

    // exists check
    auto it = m_srcmap.find(key);
    if (it != m_srcmap.end()) {
        auto vec = it->second;
        if (std::find(vec.begin(),vec.end(),name) != vec.end()) {
            return;             // already present
        }
    }
    // emplace at end.
    m_srcmap[key].emplace_back( name );
}

/// Remove all players, leaving an empty set.
///
void Player_prefs::clear_all()
{
    m_srcmap.clear();
}


//////////////////////////////////////////////////////////////////////////////
////                           Player_cap_set

/// Add a cap. Idempotent.
///
void Player_cap_set::add_cap( Medium m, Encoding e)
{
    m_caps.insert( PlayCap(m,e) );
}

/// Remove all caps.
void Player_cap_set::clear_caps()
{
    m_caps.clear();
}

/// Check if a cap is present (true) or not (false).
///
bool Player_cap_set::has_cap( Medium m, Encoding e) const
{
    return m_caps.find(PlayCap(m,e)) != m_caps.end();
}

/// Fill str with a list of capabilities suitable for printing.
///
void Player_cap_set::cap_string( std::string &str ) const
{
    str.clear();
    for (const PlayCap & cap : m_caps) {
        str += media_name(cap.first);
        str += ':';
        str += encoding_name(cap.second);
        str += ',';
    }
    str.resize(str.size()-1);
}

/// Add all capabilities to the Player_prefs under the given name.
///
void
Player_cap_set::install_caps( const std::string &name, Player_prefs &prefs )
    const
{
    for (const PlayCap & cap : m_caps) {
        prefs.add_player( cap.first, cap.second, name );  // m,e,n
    }
}



//////////////////////////////////////////////////////////////////////////////
////                           Player_with_caps


/// Add a capability to the set.
void Player_with_caps::add_cap(Medium m, Encoding e)
{
    m_capset.add_cap( m, e );
}

/// Clear all capabilities from the set.
void Player_with_caps::clear_caps()
{
    m_capset.clear_caps();
}


/// Implements Player API
bool Player_with_caps::has_cap(Medium m, Encoding e) const
{
    return m_capset.has_cap(m,e);
}

/// Implements Player API
void Player_with_caps::cap_string( std::string &s ) const
{
    m_capset.cap_string( s );
}

/// Implements Player API
void Player_with_caps::install_caps( Player_prefs &p )
    const
{
    m_capset.install_caps( name(), p );
}
