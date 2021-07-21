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
#include "player.hpp"
#include "childmgr.hpp"
#include "schedule.hpp"

class Mpd_client;

/// MPD Plays most media types, including mp3 streams.
/// The MPD API is embedded in class Mpd_client.
/// If m_run_mpd is true, this process will start its own
/// mpd process using executable m_mpd_binpath. The
/// configuration file for mpd must be: .config/mpd/mpd.conf.
/// Note: this will kill any already running mpd if a pid file
/// was specified in the configuration.
///
class Mpd_player : public Player_with_caps {
private:
    std::string m_name;
    std::unique_ptr<Mpd_client> m_remote;
    spSource m_src {};  // store the source we are playing
    PlayerState m_state { PlayerState::Stopped };
    boost::filesystem::path m_socket;
    std::string m_hostname;
    unsigned m_port;
    unsigned m_volume;     // as a percentage [0-100]
    bool m_enabled {true};
    bool m_run_mpd  {true};
    bool m_usable {true};
    bool m_debug {false};
    bool m_testmode {false};
    unsigned m_stall_counter {0};
    unsigned m_stalls_max {4};
    unsigned m_last_elapsed_secs {0};
    spCM m_cm;
    boost::filesystem::path m_bin_path {};
    time_t m_last_unusable { 0 };
    time_t m_recheck_secs { 2*60*60 }; // willing to recheck every 2 hrs
    //
    bool any_mpd_running();
    void assure_connected();
    void cap_init();
    void check_not_stalled();
    void mark_unusable();
    void shutdown_mpd();
    bool try_connect( bool probe_only=false );
    void try_start();
public:
    Mpd_player();
    Mpd_player(const char*);
    virtual ~Mpd_player();
    Mpd_player(const Mpd_player&) = delete;
    void operator=(Mpd_player const&) = delete;
    //
    virtual const std::string& name() const { return m_name; }
    virtual bool completed();
    virtual bool currently_playing( spSource );
    virtual void exit();
    virtual void initialize( Config&, bool );
    virtual bool is_usable();
    virtual void pause();
    virtual void play( spSource );
    virtual void resume();
    virtual PlayerState state();
    virtual void stop();
    virtual bool check();
    virtual bool is_enabled() const;
    virtual bool set_enabled( bool );
};


