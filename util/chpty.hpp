#pragma  once
/// Pseudoterminal manager object for ChildMgr class

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


/// Normal usage pattern:
///
/// Parent:
///    Pty_controller pty {};
///    pty.open()
///    fork()
///
/// Child:
///    child_init()
///    exec()
///
/// Parent:
///    write_nb() ...
///    read_nb() ...
///    close_pty()

#include <string>
#include <termios.h>
#include <sys/ioctl.h>

#include "cmexceptions.hpp"


///////////////////////////////////////////////////////////////////

/// Represent the controlling side of a pseudoterminal.
/// The buffer capacity of pty is about 4kb in either direction.
///
class Pty_controller {
private:
    enum { non_fd = (-1), libc_err=(-1) };
    int m_cfd { non_fd };            // controller file descriptor
    int m_rfd { non_fd };            // remote file descriptor (child)
    int m_errno {0};
    struct termios m_termios {};
    bool m_valid_termios {false};
    struct winsize m_winsize {24, 80, 0, 0};
    struct timeval m_rtimeout { 0, 10'000 }; // secs, usecs
    struct timeval m_wtimeout { 0, 10'000 }; // secs, usecs
    std::string m_remote_name {};
public:
    int can_read();
    int can_write();
    void child_init();
    void close_pty();
    const std::string& remote_name() const { return m_remote_name; }
    int last_errno() const { return m_errno; }
    void open_pty();
    void set_read_timeout( long, long );  // secs, usecs
    void set_write_timeout( long, long );  // secs, usecs
    void set_window_size(unsigned,unsigned);
    ssize_t read_nb( std::string&, ssize_t ); // max bytes
    ssize_t write_nb( const std::string& );
    //
    Pty_controller();
    ~Pty_controller();
};
