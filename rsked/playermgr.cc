/**
 * Player Manager : configure and select players for sources.
 *
 * See the comments marked *EXTEND* for (3) sections that must be
 * updated when types of players are added or removed.
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
#include "logging.hpp"
#include "player.hpp"
#include "playermgr.hpp"
#include "schedule.hpp"
#include "config.hpp"

////////////////////////////////////////////////////////////////////////////
/// *EXTEND*

#include "oggplayer.hpp"
#include "mp3player.hpp"
#include "mpdplayer.hpp"
#include "nrsc5player.hpp"
#include "sdrplayer.hpp"
#include "silentplayer.hpp"
#include "vlcplayer.hpp"
// *EXTEND*

/// The default priority list for players. User policy may override.
/// NOTE: Every potentially usable player should have an entry here.
///
std::vector<std::string> RankedPlayers {
    "Vlc_player",
    "Mpd_player",
    "Ogg_player",
    "Mp3_player",
    "Nrsc5_player",
    "Sdr_player",
    // *EXTEND*
    SilentName
};

#define INSTALL_PLAYERS \
    install_player( config, std::make_shared<Vlc_player>(), testp);\
    install_player( config, std::make_shared<Mpd_player>(), testp);\
    install_player( config, std::make_shared<Ogg_player>(), testp);\
    install_player( config, std::make_shared<Mp3_player>(), testp);\
    install_player( config, std::make_shared<Nrsc5_player>(), testp);\
    install_player( config, std::make_shared<Sdr_player>(), testp);

    // ^ *EXTEND* ^

////////////////////////////////////////////////////////////////////////////
///                             Player_manager

/// Pro forma destructor is required even though pure virtual interface.
Player::~Player() { }


/// Name of the annunciator player
constexpr const char *AnnName {"Annunciator"};



/// The Inet_checker is a private static member of class Player_manager
/// Players may invoke Player_manager::inet_available().
///
Inet_checker Player_manager::c_ichecker {};


/// CTOR for Player_manager
Player_manager::Player_manager()
{
    // prepare the Silent Player -- used for OFF mode
    spPlayer nplay = std::make_shared<Silent_player>();
    m_players[ nplay->name() ] = nplay;
    nplay->install_caps(m_prefs);
}

/// DTOR for Player_manager
Player_manager::~Player_manager()
{ }

/// Static member invokes Inet_checker
///
bool Player_manager::inet_available()
{
    return c_ichecker.inet_ready();
}

/// Configure and install player by its stated name given shared pointer.
/// * May throw various player errors
///
void Player_manager::install_player( Config& config, spPlayer pp, bool testp )
{
    if (pp) {
        pp->initialize(config, testp);
        m_players[ pp->name() ] = pp;
    } else {
        LOG_ERROR(Lgr) << "Player_manager: attempt to install null player";
    }
}

/// Configure the player manager using the config object by creating
/// and initializing new players. (The silent player is configured in
/// the CTOR.)  N.b. rsked may continue to hold shared pointers to
/// players that were created by previous calls to this method.
///
/// In addition to player configurations, it will also configure the
/// Inet_checker. If argument testp is true, the configuration will
/// avoid side effects on the system.
///
/// The annunciator is a player reserved for announcements and will never
/// play normal programming...
///
/// * May throw various player errors
///
void Player_manager::configure( Config& config, bool testp )
{
    install_player( config, std::make_shared<Ogg_player>(AnnName,0),testp);
    INSTALL_PLAYERS
    c_ichecker.configure( config );
    configure_prefs( config );
    check_minimally_usable();
}


/// Import user preferences from the configuration JSON.
/// Structure:   medium > encoding > [ players... ]
///
void Player_manager::load_json_prefs(Config& cfg)
{
    Json::Value jppref = cfg.get_root()["player_preference"];
    if (jppref.isNull()) return; // none provided
    if (not jppref.isObject()) {
        LOG_ERROR(Lgr) << "Unexpected player_preference syntax";
        throw Player_config_exception();
    }
    try {
        for (auto medname : jppref.getMemberNames()) {
            Medium med = strtomedium(medname);
            auto jmed = jppref[medname];
            if (not jmed.isObject()) {
                LOG_ERROR(Lgr) << "Unexpected player_preference syntax";
                throw Player_config_exception();
            }
            for (auto encname : jmed.getMemberNames()) {
                Encoding enc = strtoencoding(encname);
                auto jenc = jmed[encname];
                if ( not jenc.isArray() ) {
                    LOG_ERROR(Lgr) << "Unexpected player_preference syntax";
                    throw Player_config_exception();
                }
                unsigned i=1;
                for (auto jplayer : jenc) {
                    std::string pname = jplayer.asString();
                    // Validate it is a known player
                    if (RankedPlayers.end() ==
                        std::find(RankedPlayers.begin(),RankedPlayers.end(),pname)) {
                        LOG_ERROR(Lgr) << "Unknown player: " << pname;
                        throw Player_config_exception();
                    }
                    LOG_INFO(Lgr) << "Player preference for " << medname
                                  << "." << encname << " (" << i++ << ") "
                                  << pname;
                    m_prefs.add_player( med, enc, pname );
                }
            }
        }
    } catch( Schedule_error& ) {
        LOG_ERROR(Lgr) << "Player_manager: defective player preferences";
        throw Player_config_exception();
    }
}

/// Initializes player preference policy, m_prefs, based on
/// any user-specified policy, followed by default capabilities.
///
void Player_manager::configure_prefs(Config& config)
{
    // Read user prefs from config here. These are checked first at run time.

    if (config.get_schema() < "1.1") {
        LOG_WARNING(Lgr) <<
            "Player_manager: older schema, no user preference support";
    } else {
        load_json_prefs( config );
    }

    // Install full capabilities of all enabled players next.
    // First player installed has default priority 1, and so forth.
    for ( auto pn : RankedPlayers ) {
        spPlayer sp = m_players[ pn ];
        if (sp) {
            sp->install_caps(m_prefs);
        } else {
            LOG_ERROR(Lgr) << "Player_mgr could not find player " << pn;
        }
    }
}


/// Retrieve the annunciator (usually *ogg* player), which is used
/// exclusively for announcements. This player should be distinct from
/// any of the operational players, since they may be suspended during
/// announcements. If no usable annunciator is found, returns a ptr
/// to the Silent player.
///
spPlayer Player_manager::get_annunciator()
{
    spPlayer pp = m_players[ AnnName ];
    if (pp) {
        if (pp->is_usable()) {
            return pp;
        }
        LOG_ERROR(Lgr) << "Annunciator (" << pp->name() << ") is unusable";
    } else {
        LOG_ERROR(Lgr) << "Annunciator is unavailable.";
    }
    return m_players[ SilentName ];
}


/// Retrieve the best available player for the source src.
///
/// If the src argument is null, or the src medium is "Off" then
/// return the silent player.
///
/// If no suitable player can be determined, or none is
/// usable, then return a *null* shared pointer.
///
/// Will precheck network sources.  This assumes that Internet is
/// required and might preclude playing fully functional LAN
/// streams. To get around this, adjust the external inet checker
/// to probe only the required network resources.
///
/// * Will NOT throw
///
spPlayer
Player_manager::get_player( spSource src )
{
    if (!src) { return m_players[SilentName]; }

    spPlayer pp {};
    Medium medium = src->medium();
    Encoding encoding = src->encoding();

    if ((medium == Medium::stream) and not c_ichecker.inet_ready()) {
            LOG_WARNING(Lgr) << "Internet seems unavailable, cannot play "
                          "stream "   << src->name();
            return pp;
    }
    unsigned np = m_prefs.player_count(medium,encoding);
    //LOG_DEBUG(Lgr) << np << " player options available";
    for (unsigned j=0; j<np; j++) {
        std::string pname;
        if (m_prefs.get_player(medium, encoding, j, pname )) {
            pp = m_players[ pname ];
            if (pp and pp->is_usable()) {
                return pp;
            }
        }
    }
    pp.reset();
    LOG_ERROR(Lgr) << "No usable players for " << media_name(medium)
                   << ":" << encoding_name(encoding);
    return pp;
}


/// Check that at least one player is potentially usable.
/// * May throw a Player_startup_exception if none usable.
///
void Player_manager::check_minimally_usable()
{
    spPlayer annunciator = m_players[AnnName];
    if (not annunciator or
        not annunciator->is_usable()) {
            LOG_WARNING(Lgr) << "Player_mgr: Annunciator is not available, "
                "which is highly undesirable.";
    }

    unsigned nu=0;
    for ( auto pn : RankedPlayers ) {
        spPlayer sp = m_players[ pn ];
        if (sp and sp->is_usable()) {
            ++nu;
        }
    }
    if (nu) {
        if (1 == nu) {
            LOG_WARNING(Lgr) << "Player_mgr: only *1* usable player";
        } else {
            LOG_INFO(Lgr) << "Player_mgr: " << nu << " usable players";
        }
    } else {
        LOG_ERROR(Lgr) << "Player_mgr: NONE of the players seems usable.";
        throw Player_startup_exception();
    }
}

/// Invoked periodically to check if any players are in the wrong
/// state.  The check() method of the player is responsible for taking
/// any corrective action as appropriate. Typically, a player may be
/// restarted (up to a certain number of times) on a given source
/// before that source is marked as "failed" (making it taboo for a while).
/// In some cases the player itself may be marked as defective.
///
/// Typically called for side effects, but returns true iff all
/// players check out okay.
///
bool Player_manager::check_players()
{
    check_inet(); // players may invoke Player_manager::inet_available()
    unsigned nc=0, ngood=0;
    for ( auto pair : m_players ) {
        spPlayer sp = pair.second;
        nc++;
        if (sp) {
            if (sp->check()) {
                ngood++;
            } else {
                if (sp->is_enabled()) {
                    LOG_DEBUG(Lgr) << "Player_manager: check fails for "
                                   << pair.first;
                }
            }
        }
    }
    LOG_DEBUG(Lgr) << "Player_manager: " << ngood << "/" << nc
                   << " players are okay";
    return (nc == ngood);
}

/// Check internet availability.
///
bool Player_manager::check_inet()
{
    if (not c_ichecker.inet_ready()) {
        LOG_WARNING(Lgr) << "Internet seems to be unavailable";
        return false;
    }
    return true;
}

/// Stop all players via calls to their exit() method.
/// This includes the annunciator.
///
void Player_manager::exit_players()
{
    for ( auto sp : m_players ) {
        if (sp.second) {
            sp.second->exit();
        }
    }
}
