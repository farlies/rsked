#pragma once
/// File: mpdclient.hpp
/// MPD player client for rsked.

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

#include <string>
#include <vector>
#include <mpd/client.h>

#include "player.hpp"

//////////////////////////////////////////////////////////////////////////
/// Some Exception classes--all derived from Player_exception.


/// Thrown on problem with MPD
struct Mpd_exception : public Player_exception {
    const char* what() const throw() { return "Generic MPD exception"; }
};

/// Problem connecting to MPD
struct Mpd_connect_exception : public Mpd_exception {
    const char* what() const throw() { return "MPD connection exception"; }
};

/// Problem controlling play mode (run/pause/stop)
struct Mpd_run_exception : public Mpd_exception {
    const char* what() const throw() { return "MPD run operation exception"; }
};

/// Problem controlling the queue
struct Mpd_queue_exception : public Mpd_exception {
    const char* what() const throw() { return "MPD queue operation exception"; }
};

/// Problem with playlists
struct Mpd_playlist_exception : public Mpd_exception {
    const char* what() const throw() {return "MPD playlist operation exception";}
};

/// Problem with search
struct Mpd_search_exception : public Mpd_exception {
    const char* what() const throw() { return "MPD search operation exception"; }
};

struct mpd_connection;
struct mpd_song;

enum class Mpd_opt { NoPrint, Print };

//////////////////////////////////////////////////////////////////////////

/// Class to manage MPD
///
class Mpd_client {
private:
    mpd_connection *m_conn { nullptr };
    std::string m_socket_path;
    unsigned m_port;
    unsigned m_timeout_ms {2000};
    unsigned m_elapsed_secs {0};
    PlayerState m_obs_state {PlayerState::Stopped};
    unsigned m_server_vers[4] {0,0,0,0};
    void log_status( mpd_status *);
public:
    bool check_status( Mpd_opt );
    void clear_queue();
    void connect();
    bool connected() const { return m_conn; }
    enum mpd_error diag_error( const char*, enum mpd_server_error& );
    enum mpd_server_error diag_server_error( const char* );
    void disconnect();
    unsigned elapsed_secs() const { return m_elapsed_secs; }
    int enqueue( const std::string & );
    int enqueue( const mpd_song* );
    void enqueue_playlist( const std::string & );
    PlayerState obs_state() const { return m_obs_state; }
    void pause();
    void play();
    void play(unsigned);
    void play_pos(unsigned);
    unsigned search_album(const std::string&, std::vector<std::string>& ); 
    void set_repeat_mode(bool);
    void set_connection_params( const std::string&, unsigned port=0);
    void set_volume( unsigned );
    void stop();
    void unpause();
    bool verify_playing_uri(const std::string &);
    //
    Mpd_client();
    Mpd_client( const char* );
    Mpd_client( const char*, unsigned /*port*/);
    ~Mpd_client();
    Mpd_client( const Mpd_client& ) = delete;
    void operator=( const Mpd_client& ) = delete;
};
