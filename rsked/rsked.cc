/// Main class for the rsked application "Rsked"

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

#include "logging.hpp"
#include "rsked.hpp"
#include "config.hpp"
#include "configutil.hpp"
#include "playermgr.hpp"
#include "schedule.hpp"
#include "status.h"
#include "vurunner.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"

/// TODO: pack these into Rsked singleton
extern bool Terminate;
extern bool Button1;
extern bool ReloadReq;

/// Default paths
const boost::filesystem::path DefaultSchedulePath
{"~/.config/rsked/schedule.json"};

const boost::filesystem::path DefaultMotdPath
{"~/.config/rsked/motd"};

namespace po = boost::program_options;

/// CTOR
/// @arg key  status out shared memory key passed from command line
///
Rsked::Rsked(key_t status_key, bool test)
    : m_config( std::make_unique<Config>("~/.config/rsked/rsked.json") ),
      m_sched( std::make_unique<Schedule>() ),
      m_vu_runner( std::make_unique<VU_runner>() ),
      m_pmgr( std::make_unique<Player_manager>() ),
      m_schedpath( expand_home(DefaultSchedulePath) ),
      m_shmkey(status_key),
      m_test(test)
{
    spSource src {};      // null will get the silent player
    m_cur_player = m_pmgr->get_player( src );

    // shared memory setup is indicated
    m_shm_id = shmget( m_shmkey, sizeof(uint32_t), (IPC_CREAT|0660) );
    if (m_shm_id==(-1)) {
        LOG_ERROR(Lgr) << "Failed to get Status shared memory: "
                      << strerror(errno);
    } else {
        errno=0;
        m_shm_word = (uint32_t*) shmat(m_shm_id,NULL,0); // RW
        if ((void*)(-1)==m_shm_word) {
            LOG_ERROR(Lgr) << "Failed to attach Status shared memory: "
                          << strerror(errno);
            m_shm_word = nullptr;
        } else {
            LOG_INFO(Lgr) << "Status shared memory region created with key "
                          << m_shmkey;
            *m_shm_word = RSK_INITIALIZING;
        }
    }
}

/// DTOR
/// Stop all players. Unique pointer members do most of the work.
///
Rsked::~Rsked()
{
    if (m_shm_word) {
        shmdt((const void*) m_shm_word);
    }
}

/// Configure the application from a file indicated by p with
/// a program options variables map vm (which may override settings
/// in the config file).
///
void Rsked::configure(const std::string& p, const po::variables_map &vm)
{
    constexpr const char* GSection { "General" };
    m_config->set_config_path( p );
    m_config->read_config();    // may throw

    // schema - we only take 1.0 or 1.1
    std::string schema = m_config->get_schema();
    if ((schema != "1.0") and (schema != "1.1")) {
        LOG_ERROR(Lgr) << "Invalid schema '" << m_config->get_schema()
                       << "' for file " << p;
        throw Config_error();
    }

    // application
    std::string appname {};
    if (not m_config->get_string(GSection,"application",appname)
        or appname != "rsked") {
        LOG_ERROR(Lgr) << "Invalid application in config file " << p;
        throw Config_error();
    }

    // version
    m_config->log_about();
    m_cfgversion = "?";
    if (not m_config->get_string(GSection,"version",m_cfgversion)) {
        LOG_ERROR(Lgr) << "No declared version in config file " << p;
        throw Config_error();
    }

    // Retrieve the schedule path from config and attempt to load it--
    // unless the program options specifies a schedule (use it instead of
    // the value in m_config; no need to do shell expansion on it).
    if (vm.count("schedule")) {
        m_schedpath = vm["schedule"].as<std::string>();
    } else {
        m_config->get_pathname(GSection,"sched_path",FileCond::MustExist,
                           m_schedpath);
    }
    m_sched->load( m_schedpath );

    // load player configurations
    m_pmgr->configure( *m_config, m_test );

    // configure the VU monitor
    m_vu_runner->configure( *m_config, m_test );
}

/// Attempt to replace the Schedule with a new one loaded from the
/// configured path, m_sched. On success, it stops the current player
/// if any, and zeroes out the current slot. On failure, the previous
/// schedule is retained.
/// * Will not throw
///
void Rsked::reload_schedule()
{
    ReloadReq = false;
    LOG_INFO(Lgr) << "Rsked:: reloading schedule on signal";
    //
    std::unique_ptr<Schedule> psched = std::make_unique<Schedule>();
    try {
        psched->load(m_schedpath);
        if (! psched->valid()) {
            LOG_ERROR(Lgr) << "Reload of schedule failed--invalid schedule.";
            return;
        }
        m_sched = std::move(psched); // install new schedule
        m_cur_slot.reset();
        if (m_cur_player) {
            m_cur_player->play(nullptr);
        }
    } catch(...) {
        LOG_ERROR(Lgr) << "Reload of schedule failed--keep current schedule.";
    }
}

/// Access the schedule's ResPathSpec via shared ptr.
/// Note that this might be null if no schedule or uninitialized schedule.
///
std::shared_ptr<const ResPathSpec>
Rsked::get_respathspec() const
{
    if (m_sched) {
        return m_sched->get_respathspec();
    }
    LOG_ERROR(Lgr) << "get_respathspec fails due to no loaded schedule!";
    return nullptr;
}


/// Update status in shared memory: RSK_INITIALIZING, RSK_PLAYING, RSK_PAUSED
///
void Rsked::update_status(uint32_t s)
{
    if (m_shm_word) {
        *m_shm_word = s;
    }
}

/// Should we be snoozing?
///
bool Rsked::snoozep()
{
    return ( m_snooze_until && (time(0) < m_snooze_until) );
}

/// Start snooze for prescribed period. The current player, if any
/// is paused, but will remain in m_cur_player.
///
/// * Will not throw
///
void Rsked::enter_snooze()
{
    try {
        m_snooze_until = time(0) + snooze1_secs;
        update_status(RSK_PAUSED);
        // pause current player if any; eat any sigchild event
        if (m_cur_player) {
            m_cur_player->pause();
            sleep(1); // may return immediately if child dies
        }
        LOG_INFO(Lgr) << "Rsked: Snooze for " <<
            (snooze1_secs/60) << " minutes";
        m_snoozing = true;
    } catch(...) {
        LOG_ERROR(Lgr) << "Failed to enter snooze mode";
    }
}

/// Leave snooze right now. If the current player is paused, then
/// it will be resumed on the next check cycle if it is still playing
/// the desired source.
///
/// * Will not throw
///
void Rsked::exit_snooze()
{
    try {
        m_snooze_until = 0;
        update_status(RSK_PLAYING);
        m_snoozing = false;
    } catch(...) {
        LOG_ERROR(Lgr) << "Failed to exit snooze mode";
    }
}


/// Unless the program is "Off", snooze until a given time, then
/// resume program, or unsnooze if already snoozing.  If the current
/// program is "Off" there is no effect.
///
/// * Will not throw.
///
void Rsked::toggle_snooze()
{
    if (!m_cur_slot) {
        LOG_WARNING(Lgr) << "Snooze button pressed while no slot selected.";
        return;
    }
    spSource cur_src = m_cur_slot->source();
    if (cur_src and cur_src->medium()==Medium::off) {
        LOG_WARNING(Lgr) << "Snooze button pressed while in Off mode.";
        return;
    }
    // check whether to unsnooze
    if (m_snooze_until > 0) {
        play_announcement( "%resume" );
        exit_snooze();
    }
    else {
        // time to snooze
        enter_snooze();
        // play the snooze message
        play_announcement( "%snooze1" );
    }
}


/// Determine if there is no audio output when output is expected.
/// In this situation, mark the current source defective and attempt
/// to get an alternate source. Return true means "okay" playback level.
///
/// * Will not throw (?? TODO ??)
///
bool Rsked::check_playback_level()
{
    // Under various conditions we do NOT check VU monitor:
    if (!m_vu_runner) return true;                     // defunct monitor
    if (!m_vu_runner->enabled()) return true;          // disabled monitor
    if (!m_cur_slot) return true;                      // no slot yet
    spSource cur_src = m_cur_slot->source();
    if (m_snoozing or !m_cur_player or !cur_src) return true;  // in snooze
    if (cur_src->medium() == Medium::off) return true; // off: silence expected
    if (cur_src->may_be_quiet()) return true;          // dead-air okay this src
    if (time(0) < m_check_enabled_time) return true;   // too soon
    //
    if (m_vu_runner->too_quiet()) {
        // problem detected
        LOG_WARNING(Lgr) << "Current source {" << cur_src->name()
                         << "} is too quiet";
        cur_src->mark_failed(true);
        if (m_cur_player) {
            LOG_WARNING(Lgr) << "Stop player " << m_cur_player->name();
            try {
                m_cur_player->stop();
            } catch(const std::exception &ex) {
                LOG_ERROR(Lgr) << "Problem stopping " << m_cur_player->name()
                               << ": " << ex.what();
            }
        }
        return false;
    }
    return true;   // audio output observed in the recent past
}

/// This is for very brief rsked internal announcements. It does not pause
/// any current program, so do this prior to calling if necessary.
/// Play a single announcement given its name.  This uses an exclusive
/// ogg player so the announcement source must be ogg mode.
/// Do not play any announcements before 7am or after 9pm.
///
/// * Will not throw
///
void Rsked::play_announcement( const char* sname )
{
    if (! sname or ! m_sched) {
        LOG_ERROR(Lgr) << "play_announcement missing name or schedule";
        return;
    }
    spPlayer player;
    try {
        spSource src = m_sched->find_viable_source( sname );
        if (!src) {
            LOG_WARNING(Lgr) << "Could not find announcement audio '"
                             << sname << "'";
            return;
        }
        // Check for a waking hour--suppress announcement if outside bounds.
        time_t tnow = time(0);
        struct tm *ptm = localtime(&tnow);
        if (ptm and
            ((ptm->tm_hour < Earliest_announcement_hr)
             or (ptm->tm_hour >= Latest_announcement_hr))) {
            LOG_WARNING(Lgr) << "Announcement {" << sname
                             << "} suppressed at this time of day";
            return;
        }
        player = m_pmgr->get_annunciator();
        if (player and player->is_usable()) {
            time_limited_play( player, src, Announcement_max_secs );
            LOG_INFO(Lgr) << "Announcement {" << sname << "} complete";
        } else {
            LOG_ERROR(Lgr) << "Failed to get a usable annunciator for message "
                           << sname;
        }
        player.reset();
    } catch (...) {
        LOG_ERROR(Lgr) << "play_announcement(" << sname << ") failed.";
    }
    try {   // exit the announcement player on failure
        if (player) {
            player->exit();
        }
    } catch(...) {};
}

/// Play a *scheduled* announcement given the schedule slot, if the
/// announcement source is available.  The slot is marked as completed.
/// Any currently playing program is paused while the announcement plays
/// and resumed afterwards.
/// * May throw Player_exception
///
/// TODO: << What if some different announcement is still playing? >>
///
void Rsked::play_announcement( spPlay_slot slot )
{
    if (!slot) return;          // null slot is really an error.
    LOG_DEBUG(Lgr) << "play_announcement " << slot->name()
                   << " (" << slot.get() << ")";

    // The OFF source indicates a missing dynamic announcement.
    // If schedule could not find a viable source, it returns OFF.
    // Any other source has been checked and found to be viable.
    //
    if (Medium::off == slot->source()->medium()) {
        slot->set_complete();
        LOG_INFO(Lgr) << "Announcement {" << slot->name()
                      << "}: content not available, mark complete";
        return;
    }
    // If the announcement is already playing, determine if it is
    // complete. If not complete, just return.  If complete, (or the
    // player is mysteriously gone) mark it so for today, resume the
    // previous player (if paused) then return.

    if (m_cur_slot == slot) {
        LOG_DEBUG(Lgr) << "play_announcement: already playing announcement";
        if (!m_cur_player or m_cur_player->completed()) {
            slot->set_complete();
            LOG_DEBUG(Lgr) << "play_announcement: " << m_cur_player->name()
                           << " has completed. spSlot=" << slot;
            slot->describe();
            resume_play();
        } else {
            LOG_DEBUG(Lgr) << "play_announcement: " << m_cur_player->name()
                           << " has not yet completed. spSlot=" << slot;
        }
        return;
    }
    // Okay, we have a playable announcement that is not currently
    // playing.  Suspend the current player, and make the annunciator
    // the current player.
    suspend_play();
    //
    m_cur_player = m_pmgr->get_annunciator();
    m_cur_slot = slot;
    m_cur_player->play( m_cur_slot->source() );
}

/// Suspend (pause) the current player, if any.  Its state is cached in
/// the m_susp_player and m_susp_slot.
/// * May throw Player_exception
///
void Rsked::suspend_play()
{
    LOG_INFO(Lgr) << "Suspending regularly scheduled programming";
    if (m_cur_player) {
        PlayerState ps = m_cur_player->state();
        if (ps == PlayerState::Playing) { m_cur_player->pause(); }
    } else {
        LOG_DEBUG(Lgr) << "suspend_play: no current player";
    }
    if (m_cur_slot) {
        LOG_DEBUG(Lgr) << "suspend_play of slot " << m_cur_slot->name();
    } else {
        LOG_DEBUG(Lgr) << "suspend_play: no current slot";
    }
    m_susp_player = m_cur_player;
    m_susp_slot = m_cur_slot;
}

/// Restore suspended player from m_susp* members. Will resume play.
/// * May throw Player_exception
///
void Rsked::resume_play()
{
    LOG_INFO(Lgr) << "Resuming regularly scheduled programming";
    m_cur_player = m_susp_player;
    m_cur_slot = m_susp_slot;
    if (m_cur_slot) {
        LOG_DEBUG(Lgr) << "resume_play of slot " << m_cur_slot->name();
    } else {
        LOG_DEBUG(Lgr) << "resume_play: no suspended slot";
    }
    if (m_cur_player) {
        LOG_DEBUG(Lgr) << "resume_play of player " << m_cur_player->name();
        m_cur_player->resume();
    } else {
        LOG_DEBUG(Lgr) << "resume_play: no suspended player";
    }
}


/// Play the source on the player (which is typically the
/// annunciator), waiting for completion up to n_secs seconds. The
/// player is always stopped prior to return.
///
/// * May throw a Player exception
///
void Rsked::time_limited_play( spPlayer player, spSource src, time_t n_secs )
{
    if (!player or !src) {
        LOG_WARNING(Lgr) << "Time limited play--invalid argument";
        return;
    }
    time_t start = time(0);
    player->play( src );
    do {
        sleep(1);
        if ((time(0) - start) > n_secs) {
            LOG_WARNING(Lgr) << "Exceded time limit playing " << src->name();
            break;
        }
    } while (not player->completed());
    // Always shutdown player after completion OR timeout...
    player->play(nullptr);
}


/// Play boot-up audio, selected per hour of day.
/// Don't play anything before 5 or after 10.
/// * Will not throw.
///
void Rsked::play_greeting()
{
    time_t tnow = time(0);
    struct tm *ptm = localtime(&tnow);
    if (ptm) {
        int h= ptm->tm_hour;
        if (h < 12 and h > 5) {
            LOG_DEBUG(Lgr) << "Play startup announcement for hour=" << h;
            play_announcement("%goodam");
        }
        else if (h >= 12 and h < 18) {
            LOG_DEBUG(Lgr) << "Play startup announcement for hour=" << h;
            play_announcement("%goodaf");
        }
        else if (h >=18 and h < 22) {
            LOG_DEBUG(Lgr) << "Play startup announcement for hour=" << h;
            play_announcement("%goodev");
        } else {
            LOG_INFO(Lgr) << "Suppress startup announcement hour=" << h;
        }
    }
}


///////////////////////////////// MAIN LOOP ////////////////////////////////////

/// Loop indefinitely making the receiver track the schedule.  Handle
/// various sporadic tasks like user button presses and reload requests.
/// Periodically checks all players and the playback level for the
/// current player.  Return if the Terminate flag was set by a signal
/// handler.
/// * May throw?
///
void Rsked::track_schedule()
{
    update_status(RSK_PLAYING);
    play_greeting();
    // m_sched->debug(true);  // debug the schedule

    // Run forever, tracking schedule.
    LOG_INFO(Lgr) << "Tracking schedule.";

    for (;;) {
        if (Terminate) { break; } // must exit rsked
        if ( nanosleep( &m_rest, nullptr) ) {
            LOG_INFO(Lgr) << "Sleep interrupted";
        }
        if (Terminate) { break; } // must exit rsked

        m_pmgr->check_players();   // check all player processes

        if (ReloadReq or not m_sched) {
            reload_schedule();
            continue;
        }
        if (Button1) {
            LOG_INFO(Lgr) << "Snooze button pressed.";
            Button1 = false;
            toggle_snooze();    // might enter or exit snooze mode
        }
        if (snoozep()) {        // true: we should be snoozing...
            m_snoozing = true;
            continue;
        }
        if (m_snoozing) {       // but snoozep() is false: stop snoozing
            exit_snooze();
            play_announcement("%resume");
        }
        if (!m_sched) {         // need a schedule for the following
            LOG_WARNING(Lgr) << "Schedule is missing!";
            continue;
        }
        maybe_start_playing();
        check_playback_level(); // may mark cur source as defective
        log_banner(false);
    }
}

/// Pick a slot from the schedule and start the appropriate player,
/// The chosen source might fail; in this case try alternate sources
/// until one works; ultimately the OFF source will always work.
/// Announcements are handled slightly differently; see the method
/// play_announcement(spPlay_slot) for details.  Catches all
/// std::exceptions including Player_exceptions and CM_exceptions.
///
/// * Does not throw
///
void Rsked::maybe_start_playing()
{
    for (;;) {
        try {
            spPlay_slot spslot = m_sched->play_now();
            if (spslot->is_announcement()) {
                play_announcement( spslot );
            } else {
                play_current_slot( spslot );
            }
            break;
        }
        catch (Player_media_exception&) {
            // Media exceptions we blame the Source.
            spSource src = m_cur_slot->source();
            if (src) {
                src->mark_failed();
            }
        }  // all other Player exceptions as well as CM exceptions
        catch( std::exception &ex ) {
            LOG_WARNING(Lgr) << "Player threw " << ex.what();
            m_cur_player.reset();
        }
    }
}

/// Attempt to play spslot, a non-announcement--will occupy m_cur_slot.
/// If the current player is already playing the slot's Source, allow
/// it to continue.
/// * May throw various Player_exceptions
///
void Rsked::play_current_slot(spPlay_slot spslot)
{
    m_cur_slot = spslot;
    spSource cur_src = m_cur_slot->source();

    // Check if m_cur_player exists and is playing cur_src.
    // If so, return, possibly after unpausing the player.
    if (m_cur_player and  m_cur_player->currently_playing(cur_src)) {
        // if it is paused, resume it.
        if (PlayerState::Paused == m_cur_player->state()) {
            LOG_INFO(Lgr) << "Resume player " << m_cur_player->name();
            m_cur_player->resume();
        }
        return; // success
    }
    if (cur_src) { // note: null source is possible with an empty slot
        LOG_INFO(Lgr) << "Selected source {" << cur_src->name() << "}";
        if (cur_src->may_be_quiet()) {
            LOG_WARNING(Lgr)
                << "Source may be quiet for extended periods.";
        }
    }
    // If the current player is not playing the chosen source, stop the
    // current player and forget it.
    if (m_cur_player) {
        LOG_INFO(Lgr) << "Stop player " << m_cur_player->name();
        m_cur_player->stop();
        m_cur_player.reset();
    }
    // Ask the player mgr for the right player for the source.
    m_cur_player = m_pmgr->get_player( cur_src );
    if (m_cur_player) {
        LOG_INFO(Lgr) << "Selected player " << m_cur_player->name();
        m_cur_player->play( cur_src );
        update_status((Medium::off==cur_src->medium())
                      ? RSK_OFF : RSK_PLAYING);
        m_check_enabled_time = (time(0) + m_vu_delay);
        return;  // success
    } else {
        if (cur_src) {
            LOG_ERROR(Lgr) << "No usable player found for source {"
                           << cur_src->name() << "}";
            cur_src->mark_failed();
        } else {
            LOG_ERROR(Lgr) << "No usable player found for null Source!";
        }
    }
}
