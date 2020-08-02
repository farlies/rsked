#pragma once

/// You may optionally configure this in rsked.json like:
/// { ...
///     "Inet_checker" : {
///        "enabled" : true,
///        "status_path" : "/run/user/1000/netstat",
///        "refresh" : 60
///    }, ...
///
/// Without configuration, it will be enabled with reasonable defaults.
///

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

#include <boost/filesystem.hpp>

class Config;


class Inet_checker {
private:
    bool m_enabled {true};
    boost::filesystem::path m_status_path;
    time_t m_last_check {0};
    bool m_last_status {true};
    time_t m_refresh_secs {60};
    bool get_current_status();
public:
    time_t refresh_secs() const { return m_refresh_secs; }
    void set_refresh_secs(time_t);
    time_t last_check_time() const { return m_last_check; }
    boost::filesystem::path status_path() const { return m_status_path; }
    void configure( Config& );
    bool enabled() const { return m_enabled; }
    bool inet_ready();
    //
    Inet_checker();
};
