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

////////////////////////////////////////////////////////////////////////////

#include "vumonitor/vumonitor.hpp"
#include "vurunner.hpp"
#include "util/childmgr.hpp"
#include "util/config.hpp"
#include "util/configutil.hpp"

/// CTOR. Contruct a VU_runner with the default binary path for vumonitor.
/// This will not start the vumonitor--call configure to do so.
///
VU_runner::VU_runner()
    : m_binpath( expand_home("~/bin/vumonitor") ),
      m_vu_checker(nullptr)
{
}

/// DTOR.
/// Kill the child process, if any.
/// VU_checker dtor will detach any shared memory.
///
VU_runner::~VU_runner()
{
    m_cm->kill_child();
}


/// Determine if possible whether output has been suspiciously quiet.
///
/// @return \c true  if we are monitoring VU and the monitor has flagged
/// too quiet. \c false is returned if either audio has been heard recently
/// or the VU_runner has been disabled.
///
bool VU_runner::too_quiet()
{
    if (not m_enabled or not m_vu_checker) return false;

    // check child process, restarting if needed
    if (VStatus::OK != check_vumonitor()) {
        return false;           // had to restart: skip this check
    }

    // Assess whether vu checker data is timely
    constexpr const int STALENESS_THRESHOLD {20};  // seconds
    constexpr const unsigned STALENESS_WARN_FREQ {120}; // polls
    time_t vtime = m_vu_checker->last_time();
    if ((time(0) - vtime) > STALENESS_THRESHOLD) {
        if (0==(m_staleness_warnings % STALENESS_WARN_FREQ)) {
            LOG_WARNING(Lgr) << "VU_monitor information is stale--ignoring";
        }
        m_staleness_warnings++;
        return false;
    }
    // LOG_DEBUG(Lgr) << "VU_runner checking too_quiet()";
    // do the check
    return m_vu_checker->too_quiet();
}


/// Configure the vu runner.
/// \arg \c cfg  Reference to an initialized Config object with parameters.
/// \arg \c test_only  If true, no child process is created, we just check config.
///
void VU_runner::configure( Config &cfg, bool test_only )
{
    // check if enabled
    cfg.get_bool("VU_monitor","enabled",m_enabled);
    if (not m_enabled) {
        LOG_WARNING(Lgr) << "VU_monitor will be disabled per configuration";
        return;
    }

    // establish timeout
    cfg.get_unsigned("VU_monitor","timeout",m_quiet_timeout);

    // get the binary path for vumonitor
    cfg.get_pathname("VU_monitor","vu_bin_path",FileCond::MustExist, m_binpath);

    // Pick a fresh key
    m_key = ftok( m_binpath.c_str(), 'V' );
    if (-1 == m_key) {
        // this could happen if Linux could not stat the binpath--fatal anyway.
        m_key = 54321;
    }
    LOG_DEBUG(Lgr) << "vumonitor shared memory key: " << m_key;
    if (test_only) return;

    // start vumonitor child process
    if (not start_vumonitor()) {
        m_enabled = false;
        LOG_ERROR(Lgr) << "VU_monitor will be disabled-process failed to start";
        return;
    }

    // create VU checker to watch shared memory
    m_vu_checker = std::make_unique<VU_checker>(m_key);
    if (not m_vu_checker->attached()) {
        m_enabled = false;
        m_cm->kill_child();
        LOG_ERROR(Lgr) << "VU_monitor will be disabled-shared memory error";
        return;
    }

}
      

/// Start the child vumonitor. Will kill any vumonitor already running, as
/// identified by its pid file.
///
/// \return \c true is returned if vumonitor started normally or was already running.
/// \c false is returned if vumonitor fails to start, or it starts but dies quickly.
///
bool VU_runner::start_vumonitor()
{
    if (m_cm->running()) {
        LOG_WARNING(Lgr) << "VU_monitor already running?";
        return true;
    }
    m_cm->set_name("vumonitor");
    m_cm->set_binary( m_binpath );
    m_cm->clear_args();
    m_cm->add_arg("--shmkey");
    m_cm->add_arg( std::to_string(m_key) );
    m_cm->add_arg("--timeout");
    m_cm->add_arg( std::to_string(m_quiet_timeout) );
    //
    try {
        m_cm->start_child();
        sleep(2);               // time to let child to run or die
    } catch(...) {
        LOG_ERROR(Lgr) << "could not start vumonitor application";
        return false;
    }
    RunCond status { RunCond::okay };
    if (not m_cm->check_child(status)) {
        LOG_ERROR(Lgr) << "child rsked did not start normally: "
                       << Child_mgr::cond_name(status);
        return false;
    }
    LOG_INFO(Lgr) << "VU_monitor started child process";
    return true;
}

/// Call this periodically to verify that a vumonitor process is running.
/// If that process is dead, log an error and attempt to start again.
/// If found stopped, attempt to continue.
///
/// \return VStatus::Restarted if intervention was required,
///    VStatus::Okay if vumonitor was found running or
///    VStatus::Disabled if vumonitor is disabled.
///
VU_runner::VStatus
VU_runner::check_vumonitor()
{
    if (not m_enabled or not m_cm) {
        return VStatus::Disabled;          // moot
    }

    RunCond status { RunCond::okay };
    if (m_cm->check_child( status )) {
        return VStatus::OK;
    }
    m_vumonitor_errors++;
    LOG_WARNING(Lgr) << "VU_monitor problem (" << m_vumonitor_errors
                     << "): " << Child_mgr::cond_name(status);

    VStatus retval=VStatus::Unknown;
    switch (m_cm->last_obs_phase()) {
    case ChildPhase::unknown:
    case ChildPhase::running:
        break;                  // eh?
    case ChildPhase::gone:
        start_vumonitor();      // try restart
        retval = VStatus::Restarted;
        break;
    case ChildPhase::paused:
        retval = VStatus::Restarted;
        try {
            m_cm->cont_child();  // weird, but continue it; may throw
        } catch(...) {
            LOG_ERROR(Lgr) << "VU_monitor attempt to CONT child process failed.";
        }
        break;
    }
    return retval;
}


