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

#include <ctime>
#include <string>
#include <iostream>
#include <fstream>

#include "inetcheck.hpp"
#include "util/config.hpp"
#include "logging.hpp"

namespace fs = boost::filesystem;


// CTOR
//   default status_file="$XDG_RUNTIME_DIR/netstat"
//
Inet_checker::Inet_checker()
    : m_status_path(std::string(getenv("XDG_RUNTIME_DIR"))+"/netstat")
{
}


/// Adjust the refresh time to some non-negative number of seconds.
/// * May throw std::invalid_argument
///
void Inet_checker::set_refresh_secs(time_t t)
{
    if (t > 0) {
        m_refresh_secs = t;
    } else {
        throw std::invalid_argument("Inet_checker set refresh time < 0");
    }
}


/// Retrieve the enable switch, status path and update interval
/// from the Config object.
/// * May throw Config exception
///
void Inet_checker::configure(Config &cfg)
{
    // check if enabled
    cfg.get_bool("Inet_checker","enabled",m_enabled);
    if (not m_enabled) {
        LOG_WARNING(Lgr) << "Inet_checker will be disabled per configuration";
    }
    // establish refresh interval
    long rt = m_refresh_secs;
    if (cfg.get_long("Inet_checker","refresh",rt)) {
        if (rt >= 0) {
            m_refresh_secs = rt;
        }
    }
    cfg.get_pathname("Inet_checker","status_path",
                     FileCond::NA, m_status_path);
}

/// Attempt to retrieve and return the current inet status from status
/// file.  On success the fields m_last_status and m_last_check will
/// be updated.  If the status file cannot be read, m_last_status will
/// be returned.  Note that the file might not exist after a reboot,
/// and will not be updated during certain hours--see crontab for the
/// update schedule.
///
/// * Will NOT throw
///
bool Inet_checker::get_current_status()
{
    if (not fs::exists(m_status_path)) {
        LOG_DEBUG(Lgr)
            << "Inet_checker: no such file: " << m_status_path;
        return m_last_status;
    }
    try {
        fs::ifstream sfile(m_status_path);
        int j=0;
        sfile >> j;
        m_last_status = (j==0);
        m_last_check = time(0);
    }
    catch (std::exception &e) {
        LOG_ERROR(Lgr) << "Inet_checker: error reading from "
                       << m_status_path << ": " << e.what();
    }
    // m_file_writetime = last_write_time(m_config_path);
    return m_last_status;
}

/// Is the internet usable? Status is read from the file configured in
/// status_path, a file written by check_inet.sh. If this file was
/// last read more than m_refresh_secs ago then the file will be read,
/// otherwise the last cached status is used.  If this information is
/// unavailable, blithely return true.
/// * will NOT throw
///
bool Inet_checker::inet_ready()
{
    if (not m_enabled) {
        LOG_DEBUG(Lgr) << "Inet_checker disabled";
        return true;
    }
    time_t dt = (time(0) - m_last_check);
    if (dt >= m_refresh_secs) {
        LOG_DEBUG(Lgr) << "Time to reload status file";
        return get_current_status();
    }
    LOG_DEBUG(Lgr) << "Inet_checker using cached value " << dt
                   << " < " << m_refresh_secs;
    return m_last_status;
}
