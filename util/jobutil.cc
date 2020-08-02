/* Mange PID files inthe users home directory
 */

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

#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "jobutil.hpp"


namespace fs = boost::filesystem;

///////////////////////////////////////////////////////////////////////////


/// Compute effective pid file path. If the XDG_RUNTIME_DIR environment
/// variable is set, then the pid fill will be stored there. Otherwise
/// it will be placed in the home directory indicated by environment
/// variable HOME, if set. If neither environment variable is set
/// things are really messed up, but the path will be in current working
/// dir.
///
void get_pid_path( const char* prog, fs::path &ppath )
{
    std::string rtdir(getenv("XDG_RUNTIME_DIR"));
    std::string homedir(getenv("HOME"));
    std::string fname(prog);
    fname += ".pid";
    if (not rtdir.empty()) {
        ppath = fs::path(rtdir);
    }
    else if (not homedir.empty()) {
        ppath = fs::path(homedir);
    }
    else {
        std::cerr << "Note: pid file will be placed in working dir.\n";
        ppath = ".";
    }
    ppath /= fname;
}

/// Attempt to read a pid from the ppath, storing in pid if successful.
/// Return true on success, else false.
///
bool read_pid( const fs::path &ppath, pid_t &pid )
{
    pid = 0;
    fs::ifstream ifs( ppath );
    if (!ifs.fail()) {
        ifs >> pid;
        return (pid > 0);
    }
    return false;
}

/// Check to see if this pid exists by sending it a 0 signal.  Return
/// true if it seems to exist (doesn't mean it is in a good state).
///
bool is_live_pid( pid_t pid )
{
    if (0==kill(pid,0)) {
        return true;
    }
    if (errno == ESRCH) {
        return false;
    }
    return true;
}


/////////////////////////////////// API ///////////////////////////////////

/// Test for the presence of the pidfile named by prog.
/// Return 0 if no live pid found, or the running pid if found.
///
pid_t is_running(const char *prog)
{
    pid_t pid=0;
    fs::path pidpath;
    get_pid_path(prog,pidpath);

    if (fs::exists(pidpath)) {
        if (read_pid( pidpath, pid )) {
            bool livep =  is_live_pid( pid );
            std::cerr << "PID file " << pidpath << " with PID=" << pid
                      << (livep ? " and is alive" : " but is dead") << std::endl;
            return (livep ? pid : 0);
        } else {
            std::cerr << "Failed to read a live pid from "
                      << pidpath << std::endl;
            return 0;
        }
    }
    std::cerr << "PID file " << pidpath << " does not exist." << std::endl;
    return 0;
}


/// Create and return a PID file handle for the pidfile named by prog.
/// Return (-1) if we could not create this file, e.g. because
/// it is already locked by a live process.
///
int mark_running( const char *prog )
{
    fs::path pidpath;
    pid_t my_pid = getpid();
    get_pid_path(prog,pidpath);

    fs::ofstream  pofs( pidpath );
    if (pofs.fail()) {
        return (-1);
    }
    pofs << my_pid;
    std::cerr << "Marked " << my_pid << " for program " << prog << std::endl;
    return 0;
}


/// Given a pid file handle created by mark_running,
/// close it and delete the file.  Not an error if this
/// pidfile does not exist.
///
void mark_ended( const char *prog )
{
    try {
        fs::path pidpath;
        get_pid_path(prog,pidpath);
        fs::remove( pidpath );
    } catch( fs::filesystem_error &ex ) {
        std::cerr << "Failed to delete pidfile for " << prog
                  << ": " << ex.what() << std::endl;
    }
}
