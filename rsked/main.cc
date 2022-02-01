/**
 * Rsked package scheduling application. Runs any of various
 * audio sources on a weekly schedule.
 *
 * Expects a configuration file, by default ~/.config/rsked/rsked.json,
 * which should reference a schedule file, e.g.  ~/.config/rsked/schedule.json 
 *
 * The program is designed to run for an extended period (months).
 * It responds to signals at runtime:
 *    TERM, INT (^c) -- cleanup child processes and exit
 *    HUP -- reread the schedule file immediately (TODO)
 *    USR1 -- user button 1 pressed meaning snooze/resume
 */

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
#include <unistd.h>
#include <signal.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <fstream>

#include "version.h"
#include "main.hpp"
#include "rsked.hpp"
#include "logging.hpp"
#include "jobutil.hpp"
#include "configutil.hpp"
#include "childmgr.hpp"

namespace po = boost::program_options;

std::unique_ptr<Rsked> Main::rsked {};

/// Our official name, used in pid file
const char *AppName { "rsked" };

/// Config file unless changed on the command line
std::string DefaultConfigPath { "~/.config/rsked/rsked.json" };

/// Log the version and compilation information.
/// If force==false, this will only print every LOG_INTERVAL_SECS,
/// with the intention of getting this info in every log file.
///
void log_banner(bool force)
{
    constexpr const time_t LOG_INTERVAL_SECS { 600 };

    static time_t last=0;
    time_t now = time(0);
    if ( ((now - last) < LOG_INTERVAL_SECS) and not force ) {
        return; 
    }
    LOG_INFO(Lgr) << AppName << " version "
                  << VERSION_STR "  built "  __DATE__ " " __TIME__ ;
    last = now;
}

/// Flag will be set to true if we must exit main loop due to
/// signal or condition.
///
bool Terminate = false;
int  gTermSignal = 0;

/// detected Button1 press (SIGUSR1) at given time
bool Button1 = false;

/// detected HUP - reload requested
bool ReloadReq = false;


/// Signal handler function for various signals.
/// This is handled in the main loop.
///
void my_signal_handler(int s)
{
    if ((s == SIGTERM) || (s == SIGINT) || (s==SIGQUIT)) {
        Terminate = true;
        gTermSignal = s;
    }
    else if (s == SIGUSR1) {
        Button1 = true;
    }
    else if (s == SIGHUP) {
        ReloadReq = true;
    }
}

/// Handle SIGTERM and SIGINT by flagging Terminate
///
void setup_term_handler()
{
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = my_signal_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}


////////////////////////////////////////////////////////////////////


/// Top Level Function initializes and finalizes logging, creates
/// a Main object and has it track schedule until stopped by signal.
///
int main(int ac, char **av)
{
    key_t key_id = IPC_PRIVATE;
    int return_code = 0;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help","option information")
        ("shmkey",po::value<int>(),"shared memory key for status info")
        ("config",po::value<std::string>(),"use a particular config file")
        ("console","echo log to console in addition to log file")
        ("debug","show debug level messages in logs")
        ("schedule",po::value<std::string>(),"override schedule json file")
        ("test","test configuration and exit without running")
        ("version","print version identifier and exit without running");
    po::variables_map vm;
    try {
        po::store( po::parse_command_line(ac,av,desc),vm);
        po::notify(vm);
    } catch( const std::exception &err) {
        std::cerr << "Fatal command line error: " << err.what() << std::endl;
        exit(13);
    }
    if (vm.count("help")) {
        std::cout << desc << "\n";
        exit(0);
    }
    if (vm.count("version")) {
        std::cout << AppName << " version "
                  << VERSION_STR "  built "  __DATE__ " " __TIME__  << "\n";
        exit(0);
    }
    bool test_mode = (vm.count("test") > 0);
    if (not test_mode) {
        if (is_running(AppName)) {  // only one rsked is allowed
            std::cerr  << "Abort: only one copy of " << AppName
                       << " may be running at a time." << std::endl;
            exit(2);
        }
        mark_running(AppName);      // create a pid file
    } else {
        std::cerr << ";;; Test mode\n";
    }
    if (vm.count("shmkey")) {
        key_id = vm["shmkey"].as<int>();
    }
    //
    setup_term_handler();
    auto  logpath = expand_home( "~/logs/rsked_%5N.log" );
    int log_mode = (vm.count("console") or vm.count("test")
                   ? (LF_FILE|LF_CONSOLE) : LF_FILE);
    if (test_mode) log_mode = LF_CONSOLE;
    //
    if (vm.count("debug")) { log_mode |= LF_DEBUG; }
    init_logging( AppName, logpath.c_str(), log_mode );
    log_banner(true);
    // -------------------------- RUN --------------------------
    try  {
        Main::rsked = std::make_unique<Rsked>(key_id,test_mode);
        if (vm.count("config")) {
            std::string cstr { vm["config"].as<std::string>() };
            Main::rsked->configure(cstr,vm);
        } else {
            Main::rsked->configure(DefaultConfigPath,vm);
        }
        if (!test_mode) {
            Main::rsked->track_schedule(); // <==MAY RUN "FOREVER"==
        }
    }
    catch (Config_error &ex) {
        return_code = 1;
        LOG_ERROR(Lgr) << "main: fatal error--" << ex.what();
    }
    catch (std::exception &ex) {
        return_code = 2;
        LOG_ERROR(Lgr) << "main: fatal runtime error--" << ex.what();
    }
    catch (...) {
        return_code = 3;
        LOG_ERROR(Lgr) << "main: unexpected fatal error";
    };
    Main::rsked.reset();
    // ---------------------------------------------------------
    Child_mgr::kill_all();
    LOG_INFO(Lgr) << "Exiting on signal " << gTermSignal;
    if (not test_mode) {
        finish_logging();
        mark_ended(AppName);
    }
    return return_code;
}

