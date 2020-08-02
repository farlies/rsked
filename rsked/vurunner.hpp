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

#include <memory>
#include <boost/filesystem.hpp>
#include <sys/types.h>
#include <sys/ipc.h>

#include "childmgr.hpp"

class VU_checker;

class Config;

/// This object runs the vumonitor to track audio output for dropout
///
class VU_runner {
private:
    bool m_enabled {true};
    key_t m_key {12345};
    unsigned m_quiet_timeout {40}; // seconds
    unsigned m_vumonitor_errors {0};
    unsigned m_staleness_warnings {0};
    boost::filesystem::path m_binpath;
    spCM m_cm { Child_mgr::create("VU_runner") };
    std::unique_ptr<VU_checker> m_vu_checker;
    bool start_vumonitor();
public:
    enum class VStatus { OK, Restarted, Disabled, Unknown };
    VStatus check_vumonitor();
    void configure( Config&, bool /*test_only*/ );
    bool enabled() const { return m_enabled; }
    bool too_quiet();
    //
    VU_runner();
    ~VU_runner();
};
