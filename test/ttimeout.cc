/* Tests asio timeout hack.  The easiest way to execute this test
 * is to first start up netcat listening on port 7356.  After
 * connection by this test program, one can either
 * (a) type a line into netcat, followed by Enter, or
 * (b) do nothing and wait for a timeout
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <boost/asio.hpp>
#include <boost/optional.hpp>

using boost::asio::ip::tcp;

class tclient {
private:
    std::string m_host {"127.0.0.1"};
    std::string m_service {"7356"};
    boost::asio::io_service m_io_service {};
    boost::asio::ip::tcp::socket m_socket { m_io_service };
    bool m_connected {false};
    void readWithTimeout( std::string&,
                          const boost::asio::deadline_timer::duration_type&);
//
public:
    tclient() {};
    ~tclient();
    tclient( const tclient& ) = delete;
    void operator=( tclient const&) = delete;
    //
    bool connect();
    bool connected() const { return m_connected; }
    void disconnect();
    bool raw_transaction( const char*, std::string & );
};


/// disconnect
tclient::~tclient()
{
    disconnect();
}


/**
 * Read a line from the socket into a string, WITH a timeout.
 * Ref: https://stackoverflow.com/questions/13126776/asioread-with-timeout
 * N.b.: This will only work reliably in a *single-threaded* context.
 * Replaced calls to get_io_service() with get_executor() for boost 1.70+
 */
void tclient::readWithTimeout( std::string& result_line,
          const boost::asio::deadline_timer::duration_type& expiry_time)
{
    boost::optional<boost::system::error_code> timer_result;
    boost::asio::deadline_timer timer( m_io_service );
    timer.expires_from_now(expiry_time);
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


/// Try to establish connection. This might fail, but
/// do not throw an error--just return false.
///
bool tclient::connect()
{
    try {
        tcp::resolver resolver(m_io_service);
        boost::asio::connect(m_socket, resolver.resolve({m_host, m_service}));
        m_connected = true;
    }
    catch (std::exception &e) {
        // 1. Connection refused
        std::cerr << "tclient connect: " << e.what();
        m_connected = false;
    }
    return m_connected;
}

/// Close the socket and mark disconnected.
///
void tclient::disconnect()
{
    if (m_connected) {
        m_socket.close();
        m_connected = false;
        std::cout << "tclient disconnected\n";
    }
}


/* Send the request and wait for a response string, &result.
 * n.b. Caller should terminate the request with newline.
 */
bool tclient::raw_transaction( const char* req, std::string &result )
{
    bool status=false;
    if (!m_connected) return false;

    try {
        size_t rlen = std::strlen(req);
        boost::asio::write( m_socket, boost::asio::buffer(req,rlen) );
        std::cout << "tclient wrote Request: '" << req;

        boost::posix_time::seconds timeout(5);
        readWithTimeout( result, timeout );
        status = true;
        std::cout << "tclient read Reply (" << result.length()
                  << " bytes): " << result << "\n";
        //
    } catch (std::exception &e) {
        std::cerr << "tclient raw_transaction: " << e.what() << "\n";
        status = false;
    }
    return status;
}



int main()
{
    using namespace std;
    string answer;
    tclient test_client;

    cout << "Start listener on 7356/TCP\n";
    test_client.connect();
    test_client.raw_transaction("EHLO\n",answer);
    test_client.disconnect();
    cout << "Received answer: '" << answer << "'\n";

    return 0;
}
