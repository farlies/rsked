#pragma once

/// Define Vlc_player class. VLC Plays most media types, including
/// mp3, mp4, ogg streams.

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
#include "main.hpp"

/// This process will always start its own vlc process using
/// executable m_vlc_binpath. The configuration file for vlc is
/// usually .config/vlc/vlcrc.  This driver won't explicitly use it,
/// but it must have functional settings.
///
/// Tested with vlc 3.0.9.
///
class Vlc_player : public Player_with_caps {
private:
    enum { MaxResponse=4000, RecheckSecs=2*60*60 };
    std::string m_name; // user friendly name of player
    spSource m_src {};  // store the source we are playing
    PlayerState m_state { PlayerState::Stopped };
    unsigned m_volume;     // as a percentage [0-100]
    unsigned m_obsvol {0};  // observed playback volume
    boost::filesystem::path m_library_path {}; // music library location
    std::string m_library_uri {"file://"}; // music library location
    std::string m_last_resp {}; // last response from vlc
    std::string m_obsuri {}; // observed playing URI
    bool m_enabled {true};
    bool m_usable {true};
    bool m_debug {false};
    bool m_testmode {false};
    unsigned m_stall_counter {0};
    unsigned m_stalls_max {7};
    unsigned long m_last_elapsed_secs {0};
    spCM m_cm;
    boost::filesystem::path m_bin_path {};
    boost::filesystem::path m_lib_path {};
    time_t m_last_unusable { 0 };
    time_t m_recheck_secs { RecheckSecs }; // willing to recheck every 2 hrs
    //
    void assure_running();
    void cap_init();
    void check_not_stalled();
    bool check_status();
    void do_command( const std::string&, bool );
    void mark_unusable();
    void set_volume();
    void shutdown_vlc();
    void try_start();
    bool verify_playing_uri( const std::string& ) const;
public:
    Vlc_player();
    Vlc_player(const char*);
    virtual ~Vlc_player();
    Vlc_player(const Vlc_player&) = delete;
    void operator=(Vlc_player const&) = delete;
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
