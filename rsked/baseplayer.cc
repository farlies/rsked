/// The ogg player runs ogg123 on a playable slot.


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

#include "baseplayer.hpp"
#include "schedule.hpp"

/// CTOR
Base_player::Base_player()
{}

/// CTOR with name
Base_player::Base_player( const char* nm )
    : m_name(nm)
{
    m_cm->set_name(nm);
}

/// DTOR
Base_player::~Base_player()
{
    m_cm->kill_child( true, m_kill_us );  // will never throw
}

/// Usability
bool Base_player::is_usable()
{
    return m_enabled;
}

/// Enabledness
bool Base_player::is_enabled() const
{
    return m_enabled;
}

/// enabled==true: Enable
/// enabled==false; Disable
///
/// When disabled, the player exits and goes to state Disabled.
/// When enabled, the player goes to state Stopped.
/// Return the new value of the enabled flag.
///
bool Base_player::set_enabled( bool enabled )
{
    bool was_enabled = m_enabled;
    if (was_enabled and not enabled) {
        exit();
        m_enabled = false;
        m_pstate = PlayerState::Disabled;
        LOG_WARNING(Lgr) << m_name << " is being Disabled";
    }
    else if (enabled and not was_enabled) {
        m_pstate = PlayerState::Stopped;
        m_enabled = true;
        LOG_WARNING(Lgr) << m_name << " is being Enabled";
    }
    return m_enabled;
}


/// Exit: Terminate external player, if any.
/// * Will NOT throw
///
void Base_player::exit()
{
    LOG_INFO(Lgr) << "forcing " << m_name << " to exit";
    m_cm->kill_child( true, m_kill_us );
    m_pstate = PlayerState::Stopped;
}


/// If playing a file, directory, or playlist, then signal STOP.
///
/// * Might throw a CM_exception
///
void Base_player::pause()
{
    // verify m_src is not mode streaming
    if (!m_src) return;
    if (m_src->medium() == Medium::stream) {
        LOG_WARNING(Lgr) << m_name <<
            "--cannot pause while streaming; exit instead.";
        exit();
        return;
    }
    if (m_pstate == PlayerState::Playing) {
        if (m_cm->running()) {
            m_cm->stop_child( m_pause_us );
            m_pstate = PlayerState::Paused;
            LOG_DEBUG(Lgr) << m_name << " paused";
        }
    }
}

/// If paused, then signal CONT.
///
void Base_player::resume()
{
    if (m_pstate == PlayerState::Paused) {
        ChildPhase cph = m_cm->last_obs_phase();
        if (ChildPhase::paused == cph) {
            m_cm->cont_child( m_resume_us );
            m_pstate = PlayerState::Playing;
        } else {
            LOG_WARNING(Lgr)
                << m_name <<  " resume() called but child was in phase "
                << Child_mgr::phase_name(cph);
        }
    } else {
        LOG_WARNING(Lgr) << m_name << " resume() called but not paused.";
    }        
}

/// Return the current PlayerState
///
PlayerState Base_player::state()
{
    return m_pstate;
}


/// Means the same as exit for this player. N.B. not the same as "pause".
///
void Base_player::stop()
{
    exit();
}


/// Does a check to verify the child process is playing (or completed
/// playing) the resource named in src.
/// Returns false if:
/// -- player is playing something else
/// -- player is in the wrong state
///
/// It is up to the caller to rectify the problem if false is
/// returned.
///
bool Base_player::currently_playing( spSource src )
{
    if ( m_src != src ) {
        // LOG_WARNING(Lgr) << m_name << " playing something else.";
        return false;
    }
    ChildPhase ophase = m_cm->last_obs_phase();
    if ((m_cm->cmd_phase() == ophase)
         or (ophase==ChildPhase::gone and not m_src->repeatp())) {
        return true;
    }
    if (src) {
        LOG_DEBUG(Lgr) << m_name << " is NOT currently playing " << src->name();
    }
    LOG_DEBUG(Lgr) << m_name << " command phase:  "
                   << Child_mgr::phase_name( m_cm->cmd_phase() );
    LOG_DEBUG(Lgr) << m_name << " observed phase: "
                   << Child_mgr::phase_name( ophase );
    // in the wrong phase
    return false;
}

/// Did the player exit? Could have completed playing, or stopped on error.
///
bool Base_player::completed()
{
    return m_cm->completed();
}

/// Return true if this player is okay, false if not so.
/// Some bad states can be corrected--if so, true is returned.
///
bool Base_player::check()
{
    RunCond status { RunCond::okay };

    if (m_cm->check_child(status)) {
        // running, gone, or paused as we expect it to be
        return true;
    }

    // some problem detected...
    LOG_WARNING(Lgr) << m_name << " -- abnormal condition detected: "
                     << Child_mgr::cond_name(status);
    bool rc  { true };

    // select corrective action if possible
    switch( status ) {

    case RunCond::badExit:
    case RunCond::sigKilled:
    case RunCond::runTooShort:
        rc = maybe_restart(status);
        break;

    case RunCond::runTooLong:
    case RunCond::wrongState:
        // TODO: maybe terminate?
        break;

    case RunCond::unexpectedPause:
        // TODO: continue ?
        break;

    case RunCond::unknown:
    default:
        break;
    }
    return rc;
}

/// If the player /should/ be still running (i.e. not completed)
/// attempt to restart it, *unless* it has failed abnormally more
/// than max_restarts in the last 10 seconds
///
bool Base_player::maybe_restart(RunCond status)
{
    ChildPhase obs_ph=m_cm->last_obs_phase();

    // only handle cases where child process is actually gone
    if (obs_ph != ChildPhase::gone) { return false; }

    // If we are playing (a file) and the repeat option is off,
    // *and* the run was not too short, then this is probably okay:
    // it just finished playing the file.
    //
    if (not m_src->repeatp() and not (status==RunCond::runTooShort)) {
        // tell cm it should not be running so next check won't return here
        m_cm->kill_child( true, m_kill_us ); // force
        if (nullptr == m_src) {
            LOG_INFO(Lgr) << m_name << " exited while not playing anything";
        } else {
            LOG_INFO(Lgr) << m_name << " completed playing {"
                          << m_src->name() << "}";
        }
        return true;
    }
    return attempt_restart();
}

/// Attempting restart if we are supposed to still be
/// running. Restarts are limited to m_max_restarts in
/// m_restart_interval seconds, after which the source is marked as
/// failed. This is the most likely explanation, although the
/// alternative of a bad player is also possible--difficult to
/// distinguish these cases in general.
///
bool Base_player::attempt_restart()
{
    LOG_INFO(Lgr) << m_name << " exited while playing {"
                  << m_src->name() << "}";

    if (ChildPhase::running == m_cm->cmd_phase())
    {
        unsigned nrestarts = m_cm->fails_since( time(0) - m_restart_interval );
        LOG_INFO(Lgr) << m_name << ": "  << nrestarts << " restarts in the last "
                      << m_restart_interval << " seconds";
        if ( nrestarts < m_max_restarts) {
            LOG_INFO(Lgr) << m_name << " Attempt to restart player on {"
                          << m_src->name() << "}";
            play( m_src );
        } else {
            LOG_ERROR(Lgr) << m_name << " Too many failures to attempt another restart";
            m_src->mark_failed(); // mark it as suspect
            m_src.reset();        // abandon this source
            return false;
        }
    } else {
        // Not supposed to be running - clear m_src ptr.
        m_src = nullptr;
    }
    return true;
}
