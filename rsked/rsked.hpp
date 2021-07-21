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

#include <sys/types.h>
#include <sys/shm.h>

#include <memory>
#include <string.h>

#include <boost/program_options.hpp>

#include "config.hpp"
#include "source.hpp"
#include "schedule.hpp"
#include "player.hpp"

/// TODO: Awkwardly these live in main.cc:
extern const char* AppName;
extern void log_banner(bool);

class Player_manager;
class VU_runner;


/// A few compiled-in parameters (not in config file):
///
enum {
    Announcement_max_secs = 4,      // longest rsked announcement, seconds
    Earliest_announcement_hr = 7,   // none before 7=7:00 AM
    Latest_announcement_hr = 21,    // none after 21=9:00 PM
    SnzS = 3600,                    // snooze duration, seconds
    MMotd = 30                      // longest message duration, seconds
};


///////////////////////////////// Rsked //////////////////////////////////


/// Encapsulate the major supervisory functions of the rsked application.
///
class Rsked {
private:
    std::unique_ptr<Config> m_config; // current configuration object
    std::unique_ptr<Schedule> m_sched; // current schedule object
    std::unique_ptr<VU_runner> m_vu_runner;
    std::unique_ptr<Player_manager> m_pmgr;
    boost::filesystem::path m_schedpath; // names the schedule file
    key_t m_shmkey;                  // shared memory key
    bool m_test;                     // true: in test mode (no side effects)
    int   m_shm_id {0};              // id of shared memory
    uint32_t  *m_shm_word {nullptr}; // address of shared memory word
    struct timespec m_rest = {2,0};  // {sec, nsec}
    //
    spPlay_slot m_cur_slot {};       // current slot
    spPlayer m_cur_player {};        // current player
    spPlay_slot m_susp_slot {};      // suspended slot
    spPlayer m_susp_player {};       // suspended player
    //
    time_t snooze1_secs  { SnzS };  // snooze; default one hour, in seconds
    bool m_snoozing  { false };      // have we been snoozing?
    time_t m_snooze_until { 0 };     // 0, or time to resume play when in snooze
    time_t m_check_enabled_time {0}; // when we start VU checking
    time_t m_vu_delay { 24 };        // max windup time for src to be audible
    std::string m_cfgversion {"?"};  // config file's version
    //
    bool check_playback_level();
    void enter_snooze();
    void exit_snooze();
    void maybe_start_playing();
    void play_announcement( spPlay_slot );
    void play_announcement( const char* );
    void play_current_slot( spPlay_slot );
    void play_greeting();
    void reload_schedule();
    void resume_play();
    bool snoozep();
    void suspend_play();
    void time_limited_play( spPlayer, spSource, time_t);
    void toggle_snooze();
    void update_status(uint32_t);
    //
public:
    Rsked(key_t k, bool test);
    Rsked(const Rsked&) = delete;
    void operator=(Rsked const&) = delete;
    ~Rsked();
    //
    void configure( const std::string&,
                    const boost::program_options::variables_map& );
    std::shared_ptr<const ResPathSpec> get_respathspec() const;
    const std::string& get_config_version() const { return m_cfgversion; }
    void track_schedule();
};
