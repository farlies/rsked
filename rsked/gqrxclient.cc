/// Implementation of the gqrx client
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

#include "gqrxclient.hpp"
#include "player.hpp"
#include "sdrplayer.hpp"

using boost::asio::ip::tcp;

/// CTOR for gqrx_client. Note: it does not automatically connect, you
/// must call connect()
///
gqrx_client::gqrx_client( Sdr_player* p )
    : m_player(p)
{
}

/// DTOR for gqrx_client. Will automatically call disconnect().
///
gqrx_client::~gqrx_client()
{
    disconnect();
}

/// Establish host and port (service), e.g. from config file
void
gqrx_client::set_hostport( const std::string &h, unsigned port)
{
    m_host = h;
    m_service = std::to_string(port);
}

/// If attached to a player, mark the player as unusable.
///
void gqrx_client::mark_unusable(bool unusablep)
{
    if (m_player) {
        m_player->mark_unusable( unusablep );
    }
}



/// Retrieve the frequency in Hz.
///
freq_t gqrx_client::get_freq()
{
    unsigned long f=0;
    std::string result;

    raw_transaction( "f\n", result );
    f = std::stoul(result,nullptr); // may throw invalid_argument
    return f;
}

/// Sets receiver frequency in Hz. Return true if command succeeds.
///
void gqrx_client::set_freq( freq_t f )
{
    std::string command("F ");
    std::string result;

    command += std::to_string(f);
    command += "\n";
    raw_transaction( command.c_str(), result );
    if (0!=result.compare("RPRT 0")) {
        LOG_ERROR(Lgr) << "gqrx_client set_freq: gqrx returned '" 
                       << result << "'";
        mark_unusable(true);
        throw Player_ops_exception();
    }
}

/// Return the signal strength (dBFS) e.g. -0.2.  If this fails, it will
/// log an error, but will not mark the player as unusable, nor throw.
///
double gqrx_client::get_smeter()
{
    double f=0.0;
    std::string result;

    raw_transaction( "l STRENGTH\n", result );
    try {
        f = std::stod(result,nullptr); // may throw invalid_argument
    } catch(std::exception &ex) {
        LOG_ERROR(Lgr) << "gqrx_client get_smeter() " << ex.what()
                       << ": " << result;
        // throw Player_ops_exception() ?
    }
    return f;
}

/// Is the Rx currently demodulating a signal?
///
/// \returns true if the radio is actively demodulating a signal
///
bool gqrx_client::get_dsp()
{
    unsigned long r=0;
    std::string result;         // should be '0' or '1'

    try {
        raw_transaction( "d\n", result );
        r = std::stoul(result,nullptr); // may throw invalid_argument
    } catch (std::exception &ex) {
        LOG_ERROR(Lgr) << "gqrx_client get_dsp() " << ex.what()
                       << ": " << result;
    }
    return (r != 0);
}

/// Start conversion and audio output. Return true if command succeeds.
/// The return value from gqrx is unfortunately not yet valid: it always
/// appears to succeed, even if the radio is unplayable. (TODO).
///
void gqrx_client::start_dsp()
{
    std::string result;

    raw_transaction( "DSP1\n", result );
    if (0!=result.compare("RPRT 0")) {
        LOG_ERROR(Lgr) << "gqrx_client start_dsp(): " << result;
        mark_unusable(true);
        throw Player_ops_exception();
    }
}

/// Stop conversion and audio output. Return true if command succeeds.
/// The return value from gqrx is unfortunately not reliable: it always
/// appears to succeed, even if the radio is unplayable.
///
void gqrx_client::stop_dsp()
{
    std::string result;

    raw_transaction( "DSP0\n", result );
    if (0!=result.compare("RPRT 0")) {
        LOG_ERROR(Lgr) << "gqrx_client stop_dsp(): " << result;
        mark_unusable(true);
        throw Player_ops_exception();
    }
}


/**
 * Reads a line from the socket into a string, WITH secs seconds timeout.
 * N.b.: This will only work reliably in a *single-threaded* context.
 * Ref: https://stackoverflow.com/questions/13126776/asioread-with-timeout
 */
void gqrx_client::readWithTimeout( std::string& result_line, unsigned secs)
{
    boost::posix_time::seconds timeout(secs);
    boost::optional<boost::system::error_code> timer_result;
    boost::asio::deadline_timer timer( m_io_service );
    timer.expires_from_now( timeout );
    timer.async_wait([&timer_result]
                     (const boost::system::error_code& error) {
                         timer_result.reset(error); });

    boost::asio::streambuf data;
    boost::optional<boost::system::error_code> read_result;
    boost::asio::async_read_until( m_socket, data, '\n',
           [&read_result,&result_line,&data]
           (const boost::system::error_code& error, size_t)
           {
               if (!error) {
                   std::istream instr(&data);
                   std::getline(instr, result_line);
               }
               read_result.reset(error);
           });
    //
    m_io_service.reset();
    while (m_io_service.run_one())
    {
        if (read_result)
            timer.cancel();
        else if (timer_result)
            m_socket.cancel();
    }
    if (*read_result) {
        throw boost::system::system_error(*read_result);
    }
}


/* Send the request and wait for a response string, &result.
 * n.b. Caller should terminate the request with newline.
 * Returns false if the transaction failed to complete, 
 * and returns true if it did complete
 * - Will not throw.
 */
bool gqrx_client::raw_transaction( const char* req, std::string &result )
{
    bool status=false;
    if (!m_connected) return false;

    try {
        size_t rlen = std::strlen(req);
        boost::asio::write( m_socket, boost::asio::buffer(req,rlen) );
        LOG_DEBUG(Lgr) << "gqrx_client wrote Request: " << req;

        readWithTimeout( result, m_readtimeout );
        status = true;
        LOG_DEBUG(Lgr) << "gqrx_client read Reply (" << result.length()
                       << " bytes): " << result;
        //
    } catch (std::exception &e) {
        LOG_ERROR(Lgr) << "gqrx_client raw_transaction: " << e.what();
        status = false;
    }
    return status;
}


/// send the command but do not wait for a response.
///
bool gqrx_client::raw_cmd( const char* req )
{
    bool status=false;
    if (!m_connected) {
        return false;
    }
    try {
        size_t rlen = std::strlen(req);
        boost::asio::write( m_socket, boost::asio::buffer(req,rlen) );
        status = true;
        // LOG_DEBUG(Lgr) << "Request: " << req; // DEBUG

    } catch (std::exception &e) {
        LOG_ERROR(Lgr) << "gqrx_client raw_cmd: " << e.what();
        status = false;
    }
    return status;
}


/// Try to establish connection. This frequently fails when gqrx is
/// first starting up. Do not throw an error, just return false;
///
bool gqrx_client::connect()
{
    try {
        tcp::resolver resolver(m_io_service);
        boost::asio::connect(m_socket, resolver.resolve({m_host, m_service}));
        m_connected = true;
    }
    catch (std::exception &e) {
        // 1. Connection refused
        LOG_WARNING(Lgr) << "gqrx_client connect: " << e.what();
        m_connected = false;
    }
    return m_connected;
}

/// Issue a disconnect command (q) if currently connected.
/// Don't bother waiting for a response--there won't be one.
/// Close the socket and mark disconnected.
///
void gqrx_client::disconnect()
{
    if (m_connected) {
        boost::asio::write(m_socket, boost::asio::buffer("q\n",2));
        m_socket.close();
        m_connected = false;
        LOG_INFO(Lgr) << "gqrx_client disconnected";
    }
}

