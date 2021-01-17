#include "chpty.hpp"

// define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "logging.hpp"

/// CTOR which will take termios attributes from current STDIN
///
Pty_controller::Pty_controller()
{
    if (tcgetattr(STDIN_FILENO, &m_termios)) {
        m_valid_termios = true;
    }
}


/// DTOR - automatically closes pty controller file
///
Pty_controller::~Pty_controller()
{
    close_pty();
}

/// Open the conroller (master).
/// * may throw Chpty_open_exception
///
void Pty_controller::open_pty()
{
    m_errno=0;
    char buf[128];
    try {
        // If already showing open, throw.
        if (libc_err != m_cfd) { throw Chpty_open_exception(); }

        m_cfd = posix_openpt( O_RDWR | O_NOCTTY );
        if (libc_err == m_cfd) { throw Chpty_open_exception(); }
        if (libc_err == grantpt(m_cfd)) { throw Chpty_open_exception(); }
        if (libc_err == unlockpt(m_cfd)) { throw Chpty_open_exception(); }
        if (0 != ptsname_r( m_cfd, buf, sizeof(buf) )) {
            throw Chpty_open_exception();
        }
        m_remote_name = buf;

    } catch ( Chpty_exception & ) {
        m_errno = errno;
        if (non_fd != m_cfd) {
            close( m_cfd );
            m_cfd = non_fd;
        }
        throw; // rethrow
    }
}

/// Closes the controlling fd, if open.
/// * Does NOT throw.
///
void Pty_controller::close_pty()
{
    if (m_cfd != non_fd) {
        close(m_cfd);
        m_cfd = non_fd;
    }
}


/// Change the read timeout parameter (seconds+microseconds).
/// Conservatively suppose that a context switch is 10 usec
/// so not much point setting this value to less than 20 usec.
/// * will NOT throw
///
void Pty_controller::set_read_timeout( long secs, long usecs )
{
    if ((secs >= 0) and (usecs >= 0)) {
        m_rtimeout.tv_sec = secs;
        m_rtimeout.tv_usec = usecs;
    }
}

/// Change the write timeout parameter (seconds+microseconds).
/// * will NOT throw
///
void Pty_controller::set_write_timeout( long secs, long usecs )
{
    if ((secs >= 0) and (usecs >= 0)) {
        m_wtimeout.tv_sec = secs;
        m_wtimeout.tv_usec = usecs;
    }
}

/// Check if there is data to be read on the controlling file descriptor.
/// waiting up to the read timeout period for some data to arrive.
/// It will ignore signals that may interrupt the wait.
/// Return 0 if no data ready (or pty closed), nonzero otherwise.
///
/// *  May throw Chpty_read_exception
///
int Pty_controller::can_read()
{
    if (non_fd == m_cfd) {
        return 0;
    }
    struct timeval tv = m_rtimeout;
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET( m_cfd, &fds);
        errno = 0;
        //                        RD    WR    XC    time remaining
        int rc = select( m_cfd+1, &fds, NULL, NULL, &tv );
        if (0 < rc) {
            return FD_ISSET( m_cfd, &fds );
        }
        else if (0 == rc) {  // timeout occurred
            return 0;
        }
        else  if (libc_err == rc) {
            if (errno != EINTR) {
                throw Chpty_read_exception();
            }
        }
        LOG_DEBUG(Lgr) << "Pty_controller::can_read() select timeout interrupted.";
    }
}

/// Check if there is space for data to be written on the controlling
/// file descriptor, waiting up to the write timeout period for space
/// to become available.  Return 0 if not ready (or pty closed),
/// nonzero otherwise.
///
/// * May throw Chpty_write_exception
///
int Pty_controller::can_write()
{
    if (non_fd == m_cfd) {
        return 0;
    }
    struct timeval tv = m_wtimeout;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET( m_cfd, &fds);
    //                        RD    WR    XC    timeout
    int rc = select( m_cfd+1, NULL, &fds, NULL, &tv );
    if (libc_err == rc) {
        throw Chpty_write_exception();
    }
    if (0 == rc) {   // timeout occurred
        return 0;
    }
    return FD_ISSET(m_cfd, &fds);
}


/// Write string to the remote program, but do not block for more than
/// m_wtimeout. Multiple calls to write will automatically be issued
/// for an especially long string.  Returns the number of bytes
/// written (0 if timeout).  Any error on write, or a closed pty will
/// throw an exception.
///
/// * May throw Chpty_write_exception
///
ssize_t Pty_controller::write_nb( const std::string &str )
{
    if (non_fd == m_cfd) {
        throw Chpty_write_exception();
    }
    ssize_t nb = static_cast<ssize_t>(str.size());
    ssize_t nwritten = 0;
    const char *s = str.c_str();
    while (nwritten < nb) {
        if (0 == can_write()) break;
        ssize_t rc = write( m_cfd, s, nb );
        if (libc_err == rc) {
            throw Chpty_write_exception();
        }
        if (0 == rc) break;
        nwritten += rc;
        nb -= rc;
        s += rc;
    }
    return nwritten;
}


/// Read from the pty, *appending* into string dst. Note that dst is
/// NOT initially cleared by this method.  Stop reading when no more
/// data is available within the read timeout, or once dst reaches
/// maxlen, an argument which should be less than string.max_size().
/// Return the total number of bytes in dst after reading is complete.
///
/// n.b. Most commonly the caller *will* incur at least one
/// Pty_controller read-timeout with this call since it lingers to
/// make sure nothing remains--so do not make that timeout too
/// long. Use the method set_read_timeout() if the default value is
/// inappropriate. Its also worth remembering that ptys are line
/// oriented, so a dangling fragment of a line might not be
/// transmitted until the remote side follows it with a newline.
///
/// * May throw Chpty_read_exception()
///
ssize_t Pty_controller::read_nb( std::string &dst, ssize_t maxlen )
{
    if (non_fd == m_cfd) {
        throw Chpty_read_exception();
    }
    ssize_t ndst = static_cast<ssize_t>(dst.size());
    bool retry = false;
    //
    while (ndst < maxlen) {
        if (not retry and (0 == can_read())) { // timeout of can_read
            break;
        }
        errno = 0;
        ssize_t rc = read( m_cfd, m_read_buf, sizeof(m_read_buf) );
        if (libc_err == rc) {
            if (EINTR == errno) { // interrupted: worth trying read again
                retry = true;
                continue;
            } else {     // any other error will throw
                throw Chpty_read_exception();
            }
        }
        if (0 == rc) {   // managed to read nothing: very unlikely
            break;       // treat it like no-more-data and exit loop
        }
        dst.append( m_read_buf, static_cast<size_t>(rc) );
        ndst += rc;
    }
    return ndst;
}



////////////////////////////////////////////////////////////////////////
///                            In Child

/// Permit adjustment of pty virtual size. This only has effect
/// if called before child_init!
///
void Pty_controller::set_window_size(unsigned rows, unsigned cols)
{
    m_winsize.ws_row = static_cast<unsigned short>(rows);
    m_winsize.ws_col = static_cast<unsigned short>(cols);
}

/// This is to be called in the newly forked CHILD *before* executing
/// the target program. It opens m_rfd, sets terminal attributes,
/// and attaches  stdin/stout/stderr.
///
/// * May throw Chpty_*_exceptions
///
void Pty_controller::child_init()
{
    // set session id
    if (libc_err == setsid()) { throw Chpty_open_exception(); }
    close_pty();      // not needed in child
    m_rfd = open( m_remote_name.c_str(), O_RDWR );
    if (libc_err == m_rfd) {
        throw Chpty_open_exception();
    }

    // If valid, set remote tty attributes NOW
    if (m_valid_termios) {
        if (libc_err == tcsetattr( m_rfd, TCSANOW, &m_termios )) {
            throw Chpty_termio_exception();
        }
    }
    // Set window size
    if (libc_err == ioctl(m_rfd, TIOCSWINSZ, &m_winsize)) {
        throw Chpty_ioctl_exception();
    }

    // Redirect stdin, stdout, stderr streams to the pty
    if (STDIN_FILENO != dup2(m_rfd,STDIN_FILENO)) {
        throw Chpty_dup2_exception();
    }
    if (STDOUT_FILENO != dup2(m_rfd,STDOUT_FILENO)) {
        throw Chpty_dup2_exception();
    }
    if (STDERR_FILENO != dup2(m_rfd,STDERR_FILENO)) {
        throw Chpty_dup2_exception();
    }
    if (m_rfd > STDERR_FILENO) {
        close(m_rfd);
        m_rfd = non_fd;
    }
}
