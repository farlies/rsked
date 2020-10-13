/// Player Manager and Silent_player

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

#include "logging.hpp"
#include "player.hpp"
#include "playermgr.hpp"
#include "oggplayer.hpp"
#include "mp3player.hpp"
#include "mpdplayer.hpp"
#include "sdrplayer.hpp"
#include "silentplayer.hpp"
#include "schedule.hpp"
#include "config.hpp"


/// pro forma destructor even though pure virtual
Player::~Player() { }


////////////////////////////////////////////////////////////////////////////
///                             Player_manager

/// The Inet_checker is a private static member of class Player_manager
/// Players may invoke Player_manager::inet_available().
///
Inet_checker Player_manager::c_ichecker {};

/// CTOR for Player_manager
Player_manager::Player_manager()
{
    // prepare the Silent Player -- used for OFF mode
    // m_null_player = std::move( std::make_shared<Silent_player>() );
    m_null_player = std::make_shared<Silent_player>();
}

/// DTOR for Player_manager
Player_manager::~Player_manager()
{
    // players will auto delete when last pointer to them disappears.
}

/// Static member invokes Inet_checker
///
bool Player_manager::inet_available()
{
    return c_ichecker.inet_ready();
}



/// Configure the player manager using the config object by creating
/// and initializing new players. The null player is unaffected by
/// this method.  N.b. rsked may continue to hold shared pointers to
/// players that were created by previous calls to this method.  In
/// addition to player configurations, it will also configure the
/// Inet_checker. If argument testp is true, the configuration will
/// avoid side effects on the system.
///
void Player_manager::configure( Config& config, bool testp )
{
    m_annunciator = std::make_shared<Ogg_player>("Annunciator",0);
    m_annunciator->initialize(config, testp);
    //
    m_mpg321 = std::make_shared<Mp3_player>();
    m_mpg321->initialize(config,testp);
    //
    m_gqrx = std::make_shared<Sdr_player>();
    m_gqrx->initialize(config,testp);
    //
    m_ogg123 = std::make_shared<Ogg_player>();
    m_ogg123->initialize(config,testp);
    //
    m_mpd = std::make_shared<Mpd_player>();
    m_mpd->initialize(config,testp);
    //
    c_ichecker.configure( config );
}

/// Retrieve the annunciator (usually *ogg* player), which is used
/// exclusively for announcments. This player should be distinct from
/// any of the operational players, since they may be suspended during
/// announcements.
///
spPlayer Player_manager::get_annunciator()
{
    spPlayer pp { m_annunciator };
    if (pp and pp->is_usable()) {
        return pp;
    }
    if (pp) {
        LOG_ERROR(Lgr) << "Annunciator (" << pp->name() << ") is unusable";
    } else {
        LOG_ERROR(Lgr) << "Annunciator is unavailable.";
    }
    return m_null_player;
}

/// Retrieve the right player for the source.  If the src argument is
/// null, or the src medium is "Off" then return the silent player.
/// If no suitable player can be determined, or none is
/// usable, then return a null shared pointer.
///
/// TODO: rework this to respect player choice priority defined by
///    the config file. Basically, the logic should be driven by
///    the declared player capabilities and preferences.
///
spPlayer
Player_manager::get_player( spSource src )
{
    if (!src) {
        return m_null_player;;
    }
    spPlayer pp {};
    Medium medium = src->medium();
    Encoding encoding = src->encoding();

    if (medium == Medium::off) {
        pp = m_null_player;
        return pp;
    }
    if (medium == Medium::radio) {
        pp = m_gqrx;
        if (not pp or not pp->is_usable()) { // might be disabled or broken
            pp.reset();
        }
        return pp;
    }
    if (medium == Medium::stream) {
        if (not c_ichecker.inet_ready()) {
            LOG_WARNING(Lgr) << "Internet seems unavailable, cannot play "
                          "stream "   << src->name();
            pp.reset();
            return pp;
        }
        pp = m_mpd;       // first choice is  MPD
        if (not pp or not pp->is_usable()) { // if MPD is not usble,
            pp = m_mpg321; // mpg321 fallback
        }
        if (not pp or not pp->is_usable()) { // if mpg321 unusable,
            pp.reset();                      // no fallback
        }
        return pp;
    }
    if (src->localp() and ((encoding== Encoding::mp4) or
                           (encoding== Encoding::flac)) ) {
        pp = m_mpd;       // only choice for these encodings is mpd
        if (not pp or not pp->is_usable()) {
            pp.reset();   // no fallback
        }
        return pp;
    }
    if (src->localp() and (encoding== Encoding::mp3)) {
        pp = m_mpd;          // first choice is MPD
        if (not pp or not pp->is_usable()) { // if not usable fallback to mpg321
            pp = m_mpg321;
        }
        if (not pp or not pp->is_usable()) {
            pp.reset();      // no fallback beyond mpg321
        }
        return pp;
    }
    if (src->localp() and (encoding== Encoding::ogg)) {
        pp = m_mpd;          // first choice is MPD
        if (not pp or not pp->is_usable()) { // if not usable fallback to ogg123
            pp = m_ogg123;
        }
        if (not pp or not pp->is_usable()) {
            pp.reset();      // no fallback beyond mpg321
        }
        return pp;
    }
    // handle unexpected media by returning nullptr
    pp = m_null_player;
    LOG_ERROR(Lgr) << "No players for medium " << media_name(medium);
    pp.reset();
    return pp;
}


/// Invoked periodically to check if any players are in the wrong
/// state.  The check method of the player is responsible for taking
/// any corrective action as approriate. Typically, a player may be
/// restarted (up to a certain number of times) on a given source
/// before that source is marked as "failed" (making it taboo for a while).
/// In some cases the player itself may be marked as defective.
///
/// @return true if *every* existing player is now okay
///
bool Player_manager::check_players()
{
    bool rc { true };

    check_inet(); // players may invoke Player_manager::inet_available()

    if (m_ogg123)      { rc = rc and m_ogg123->check(); }
    if (m_mpg321)      { rc = rc and m_mpg321->check(); }
    if (m_gqrx)        { rc = rc and m_gqrx->check(); }
    if (m_mpd)         { rc = rc and m_mpd->check(); }
    if (m_annunciator) { rc = rc and m_annunciator->check(); }
    // don't bother checking the null player--it's okay
    return rc;
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

/// Stop all players.
///
void Player_manager::exit_players()
{
    if (m_ogg123)      { m_ogg123->exit(); }
    if (m_mpg321)      { m_mpg321->exit(); }
    if (m_mpd)         { m_mpd->exit(); }
    if (m_gqrx)        { m_gqrx->exit(); }
    if (m_annunciator) { m_annunciator->exit(); }
}
