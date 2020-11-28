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
#include "radio.hpp"

class gqrx_client;


/// This is the high level Player interface to SDR via the gqrx
/// application.
///
class Sdr_player : public Player {
    friend class gqrx_client;
private:
    spSource m_src {};          // last assigned source object
    freq_t m_freq { 0 };        // last assigned frequency
    bool m_enabled { true };
    bool m_usable { true };
    time_t m_last_unusable { 0 };
    time_t m_recheck_secs { 8*60*60 }; // 8 Hours
    boost::filesystem::path m_config_work {};
    boost::filesystem::path m_config_gold {};
    double m_last_s { -1000.0 };
    double m_low_s { -20.0 };
    double m_low_low_s { -40.0 };
    unsigned m_check_count {0};   // number of times s-level was checked
    PlayerState m_state { PlayerState::Stopped };
    std::unique_ptr<gqrx_client> m_remote;
    std::string m_name { "Sdr_player" }; // must match config section
    spCM m_cm;
    //
    bool check_demod();
    bool check_play();
    Smeter check_signal();
    bool cont_gqrx();
    void mark_unusable(bool);
    bool probe_sdr(Config&);
    void set_program();
    void setup_gqrx_config();
    void try_connect();
    void try_start();
public:
    Sdr_player();
    virtual ~Sdr_player();
    Sdr_player(const Sdr_player&) = delete;
    void operator=(Sdr_player const&) = delete;
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
};

