#pragma once

/// This may be included in a client (e.g. rsked) that tracks the output
/// of VU_monitor.

/*   Part of the rsked package.
 *
 *   Copyright 2020 Steven A. Harp
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
 *
 */

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logging.hpp"

/// values for m_quiet in VU_status
enum VU_announce { VU_DETECTED=0, VU_TOO_QUIET=1, VU_NA=2 };

/// Shared memory struct:
///
struct VU_status {
    uint32_t m_quiet{0};        // nonzero: quiet too long
    time_t m_ts {0};            // last update time
    float m_lvl{0.0};           // average output level
};


/// This class will attach to the vumonitor shared memory as needed
/// and has methods
/// 0. attached()   true if shared memory has been attached
/// 1. too_quiet()  true if it has been quiet too long
/// 2. avg_level()  recent output level (decays over a few seconds)
/// 3. last_time()  the timestamp of the last update (secs since epoch)
///
class VU_checker {
private:
    key_t  m_shmkey;
    int m_shm_id {0};
    VU_status *m_status {nullptr};

public:
    time_t last_time() {
        return (m_status ? m_status->m_ts : 0);
    }

    float avg_level() {
        return (m_status ? m_status->m_lvl : 0.0F);
    }

    bool too_quiet() {
        return (m_status ? (m_status->m_quiet == VU_TOO_QUIET) : false);
    }

    bool attached() {
        return (m_status != nullptr);
        // if false this object is not usable
    }
    //
    VU_checker( key_t k )
        : m_shmkey(k)
    {
        errno=0;
        m_shm_id = shmget( m_shmkey, sizeof(VU_status), (IPC_CREAT|0660) );
        //
        if (m_shm_id==(-1)) {
            LOG_ERROR(Lgr) << "VU_checker Failed to get shared memory: "
                           << strerror(errno);
        } else {
            errno=0;
            m_status=static_cast<VU_status*>( shmat(m_shm_id,NULL,0) ); // RW
        
            if ((void*)(-1)==m_status) {
                LOG_ERROR(Lgr) << "VU_checker Failed to attach shared memory: "
                               << strerror(errno);
                m_status = nullptr;
            } else {
                LOG_INFO(Lgr) << "VU_checker Shared memory attached with key: "
                              << m_shmkey;
            }
        }
    }

    ~VU_checker() {
        if (m_status) {
            shmdt((const void*) m_status);
        }
    }
    VU_checker( const VU_checker& ) = delete;
    void operator=( const VU_checker& ) = delete;
};
