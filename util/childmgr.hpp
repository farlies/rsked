#pragma once

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

#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <mutex>
#include <list>
#include <memory>
#include <climits>
#include <utility>

#include <boost/filesystem.hpp>
#include <boost/circular_buffer.hpp>
#include "logging.hpp"

#include "cmexceptions.hpp"

/// Some symbolic values used in Child_mgr:
///
enum { NOTAPID=(-1) };

/// Commanded and observed phases of child operation:
///
enum class ChildPhase {
    gone,                       //! process terminated
    running,                    //! process running
    paused,                     //! process stopped
    unknown                     //! state cannot be determined
};

/// Diagnoses of run state:
///
enum class RunCond {
    okay,                       //! cmd phase and obs phase agree
    badExit,                    //! obs gone with non-zero exit code
    runTooLong,                 //! obs running beyond time limit
    runTooShort,                //! obs gone (exit=0) finished too quickly
    sigKilled,                  //! obs gone due to signal
    unexpectedPause,            //! obs paused but should be some other ph
    unknown,                    //! unable to get process info
    wrongState                  //! obs running when it should not be
};

/// Optional pty object for child
class Pty_controller;

///////////////////////////////////////////////////////////////////////

/**
 * Child_mgr
 *   Class that manages a child process: track exit history, kill,
 * check, pause, or continue. Each instance is associated with a binary
 * path and a set of arguments. The child process may be run 0 or more
 * times. A child process has expected run time bounds (wall clock);
 * check_child() will not whether observed run time violates the bounds.
 *
 * This class will takeover the SIGCHLD handler on creation of the first
 * instance, and assumes that no other code will change that handler.
 *
 * Typical usage:
 *
 *   spCM cm = Child_mgr::create( binary_pathname );   // shared pointer
 *      // nothing running yet
 *   cm->set_min_run( 2 );
 *   cm->set_max_run( 1800 );
 *   cm->clear_args();
 *   cm->add_arg("-c");
 *   cm->add_arg( my_param );
 *   cm->start_child();
 *      //  fork/exec binary
 *      //  expect get_pid() > 0, and (soon) last_obs_phase()==running
 *
 *   cm->check_child()
 *      //  checks and returns true if all is well
 *
 *   cm->kill_child()
 *      # signals child to die.
 *
 * Caution:  not completely thread safe.
 */
class Child_mgr : public std::enable_shared_from_this<Child_mgr>
{
private:
    static std::list<std::shared_ptr<Child_mgr>> c_instances;
    static std::mutex c_mutex;
    static bool CM_ready;
    static std::shared_ptr<Child_mgr> find_child( pid_t );
    static void sigchld_handler(int);
    static void setup_sigchld_handler();
    //
    pid_t m_pid {NOTAPID};             // last pid seen running
    pid_t m_old_pid {NOTAPID};         // pid before that, if any
    int m_exit_status {0};             // last exit status of child
    int m_exit_reason {0};             // CLD_EXITED or CLD_KILLED
    int m_terminate   {0};             // last kill signal (0 if none)
    unsigned m_updates {0};            // signals handled
    ChildPhase m_cmd_phase = ChildPhase::gone;  // commanded phase
    ChildPhase m_obs_phase = ChildPhase::gone;  // observed phase
    std::vector<std::string> m_args {};   // cached args
    boost::filesystem::path m_chdir {}; // working directory for executable
    boost::filesystem::path m_bin_path; // application pathname
    time_t m_start_time {0};           // time of last start
    time_t m_kill_time {0};            // time we last tried to kill
    time_t m_exit_time {0};            // time of observed exit
    time_t m_min_run {0};              // normal run at least this many secs
    time_t m_max_run {INT_MAX};        // maximum time of a run; int32
    time_t m_max_death_latency {2};    // maximum seconds for child to die
    boost::circular_buffer<time_t> m_fails { 5 };  // abnormal exit times
    std::string m_name {};             // user friendly name (optional)
    std::unique_ptr<Pty_controller> m_pty {};   // pseudoterminal
    //
    bool check_child_gone( RunCond &);
    bool check_child_paused( RunCond &);
    bool check_child_running( RunCond &);
    void launch_child_binary(std::vector<const char*>&);
    void postmortem( int, int );
    void update_status( siginfo_t & );
    Child_mgr();
    Child_mgr( const boost::filesystem::path );

public:
    static const char* cond_name( RunCond );
    static const char* phase_name( ChildPhase );
    static void kill_all();
    static void ListInstances();
    static void purge();
    static unsigned run_count();
    //
    void add_arg(const char *);
    void add_arg( const std::string& );
    void add_arg( int );
    bool check_child(RunCond&);
    void clear_args();
    void clear_status();
    ChildPhase cmd_phase() const { return m_cmd_phase; }
    bool completed() const;
    void cont_child();
    unsigned fails_since(time_t) const;
    int get_exit_reason() const { return m_exit_reason; }
    int get_exit_status() const { return m_exit_status; }
    const std::string & get_name() const { return m_name; }
    pid_t get_pid() const { return m_pid; }
    void kill_child(bool force=false);
    int last_exit_status() const { return m_exit_status; }
    ChildPhase last_obs_phase() const { return m_obs_phase; }
    bool running() const;
    void set_max_run( time_t );
    void set_min_run( time_t );
    void set_binary( const boost::filesystem::path & );
    void set_name(const std::string&);
    void set_wdir(const boost::filesystem::path &);
    void signal_child(int);
    void start_child();
    void stop_child();
    unsigned updates() { return m_updates; }
    time_t uptime() const;
    // pseudoterminal methods
    void enable_pty();
    bool has_pty() const;
    const std::string& pty_remote_name() const;
    void set_pty_read_timeout( long, long );  // secs, usecs
    void set_pty_write_timeout( long, long );  // secs, usecs
    void set_pty_window_size(unsigned,unsigned);
    ssize_t pty_read_nb( std::string&, ssize_t ); // max bytes
    ssize_t pty_write_nb( const std::string& );
    // factory
    template<typename... Ts>
    static std::shared_ptr<Child_mgr> create(Ts&&... params)
    {
        auto p
         = std::shared_ptr<Child_mgr>(new Child_mgr(std::forward<Ts>(params)...));
        c_instances.emplace_back(p->shared_from_this());
        if (not CM_ready) { setup_sigchld_handler(); }
        return p;
    }
    ~Child_mgr();
};

/// smart pointer to a Child_mgr:
using spCM = std::shared_ptr<Child_mgr>;
