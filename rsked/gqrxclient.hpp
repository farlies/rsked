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

#include <iostream>
#include <iomanip>
#include <string>

#include <boost/asio.hpp>
#include <boost/optional.hpp>

#include "logging.hpp"
#include "radio.hpp"

class Sdr_player;

/**
 * Implements the extended gqrx remote protocol using Boost::asio
 */
class gqrx_client {
private:
    std::string m_host {"127.0.0.1"};
    std::string m_service {"7356"};
    boost::asio::io_service m_io_service {};
    boost::asio::ip::tcp::socket m_socket { m_io_service };
    bool m_connected {false};
    unsigned m_readtimeout {5};  // seconds
    Sdr_player *m_player;        // back pointer to player
    bool raw_transaction( const char*, std::string & );
    bool raw_cmd( const char* );
    void readWithTimeout( std::string&, unsigned );
    void mark_unusable(bool);
//
public:
    gqrx_client( Sdr_player* );
    ~gqrx_client();
    gqrx_client( const gqrx_client& ) = delete;
    void operator=( gqrx_client const&) = delete;
    //
    bool connect();
    bool connected() const { return m_connected; }
    void disconnect();
    freq_t get_freq();
    bool get_dsp();
    double get_smeter();
    void set_freq( freq_t );
    void set_hostport( const std::string&, unsigned );
    void start_dsp();
    void stop_dsp();
};

