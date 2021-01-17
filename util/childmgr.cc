/* Each instance of the Child_mgr class manages a child process.
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

#include "childmgr.hpp"
#include "chpty.hpp"
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////

/// Init static members of Child_mgr
///
std::list<std::shared_ptr<Child_mgr>> 
   Child_mgr::c_instances;  // list of all instances

std::mutex  Child_mgr::c_mutex;
bool Child_mgr::CM_ready { false };

/* Note that the global list c_instances needs to be accessed:
 * 1. on creation of a new instance  (append to end)
 * 2. on destruction of an instance  
 * 3. when searching or listing instances
 * 4. when a SIGCHLD signal arrives affecting an instance
 * The signal handling case deserves some care. It is treacherous
 * to include any sort of lock or other blocking call in a signal
 * handler since deadlock may occur. The signal handler for SIGCHLD
 * thus operates without grabbing the lock. The destructor does not
 * modify the list structure, but zeroes out the entry if found.
 */

/// Static Method. Remove all dead (not running or null) tracked
/// instances of Child_mgr.
/// Note: this function is NOT thread safe.
///
void Child_mgr::purge()
{
    c_instances.remove_if( [](spCM &p){ 
            return (not p or not p->running()); } );
}

/// Static method. Return the number of child processes that are
/// marked as *running* (globally).
///
unsigned Child_mgr::run_count()
{
    unsigned rc=0;
    for (auto pcm : c_instances) {
        if (pcm->running()) {
            rc++;
        }
    }
    return rc;
}

/// Static Method. Kill all known processes and wait for them to die.
/// Purge the c_instances list.  To kill, try first with SIGINT; if
/// that fails use SIGTERM.  If that fails, give up.
///
void Child_mgr::kill_all()
{
    const unsigned int WAIT_SECS {3};  // wait for SIGCHLD, seconds
    unsigned int wait_secs;
    unsigned nalive;
    bool forcep=false;
    do {
        nalive = 0;
        for (auto pcm : c_instances) {
            if (pcm->running()) {
                nalive++;
                pcm->kill_child(forcep);
            }
        }
        for (wait_secs=WAIT_SECS; wait_secs; wait_secs=sleep(wait_secs));
        nalive = run_count();
        if (nalive) {
            if (forcep)  {
                LOG_ERROR(Lgr) << "Cannot kill " << nalive 
                               << " process(es), giving up.";
                break;
            }
            forcep = true;
            LOG_INFO(Lgr) << "Waiting for " << nalive 
                          << " process(es) to die...";
            ListInstances();
        }
    }
    while (nalive);
    purge();
}



/// CTOR. Private.
///
Child_mgr::Child_mgr()
    : m_bin_path()
{
}

/// CTOR with binary path. Private.
///
Child_mgr::Child_mgr( const boost::filesystem::path bp )
    : m_bin_path(bp)
{
}


/// DTOR.  The child should have been killed prior to this, e.g. via
/// kill_child or kill_all.  Any SIGCHLD signal received after this
/// will be ignored.
///
Child_mgr::~Child_mgr()
{
}


/// Log all child_mgr instances and their most recent status.
/// This will lock the child list while it does so.
///
void Child_mgr::ListInstances()
{
    std::lock_guard<std::mutex> lock(c_mutex);  // grab lock
    unsigned child_index {0};
    for (auto pcm : c_instances) {
        if (pcm) {
            LOG_DEBUG(Lgr) << " (" << child_index++ << ") " << pcm->get_name()
                           << " pid=" << pcm->get_pid()
                           << "  last_observed_phase="
                           << phase_name( pcm->last_obs_phase() );
        }
    }
}


/// Return a shared pointer to the child_mgr associated with the given
/// pid This will lock the child list while it searches.  Returns a
/// shared nullptr if no such child is registered.
///
spCM Child_mgr::find_child( pid_t pid )
{
    for (auto pcm : c_instances) {
        if (pcm and (pcm->get_pid() == pid)) {
            return pcm;
        }
    }
    return spCM(nullptr);
}

/// Signal handler function for child process death.
/// Scan all children to see which one(s) have changed status.
/// Note: Calling waitid() here "consumes" the event.
/// This function will not throw, perform I/O, or call any other
/// signal-unsafe functions.
///
void Child_mgr::sigchld_handler(int sig)
{
    if (SIGCHLD != sig) return;
    siginfo_t status;
    constexpr int event_mask = WEXITED|WSTOPPED|WCONTINUED|WNOHANG;
    memset(&status,0,sizeof(status));
    while (0 == waitid(P_ALL, 0, &status, event_mask) ) {
        if (0 == status.si_pid) break; // no state change
        // Find the instance 
        spCM pcm = find_child( status.si_pid );
        if (pcm) {
            pcm->update_status( status );
        }
        memset(&status,0,sizeof(status));
    }
}

/// Set up the SIGCHLD handler. No attempt is made to preserve any
/// prior signal handler for SIGCHLD.
///
void Child_mgr::setup_sigchld_handler()
{
    // prepare signal handler
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = sigchld_handler;
    if (-1 == sigaction(SIGCHLD, &sa, NULL)) { // unlikely
        LOG_ERROR(Lgr) << "Child_mgr setup_sigchld_handler failed: "
                       << strerror(errno);
        CM_ready = false;
    } else {
        CM_ready = true;
    }
}


/// Class method retrieves phase name
///
const char*
Child_mgr::phase_name( ChildPhase p )
{
    switch (p) {
    case ChildPhase::gone:
        return "gone";
        break;
    case ChildPhase::running:
        return "running";
        break;
    case ChildPhase::paused:
        return "paused";
        break;
    default:
        return "unknown";
    }
}

/// Class method retrieves run condition name
///
const char*
Child_mgr::cond_name( RunCond r )
{
    switch(r) {
    case RunCond::okay:
        return "okay";
        break;
    case RunCond::badExit:
        return "badExit";
        break;
    case RunCond::runTooLong:
        return "runTooLong";
        break;
    case RunCond::runTooShort:
        return "runTooShort";
        break;
    case RunCond::sigKilled:
        return "sigKilled";
        break;
    case RunCond::unexpectedPause:
        return "unexpectedPause";
        break;
    case RunCond::wrongState:
        return "wrongState";
        break;
    case RunCond::unknown:
    default:
        return "unknown";
        break;
    }
}


/// Return the number of failures at or after time pt
///
unsigned Child_mgr::fails_since( time_t pt ) const
{
    unsigned matches = 0;
    for (auto it = m_fails.begin(); it != m_fails.end(); ++it) {
        if (*it >= pt) { ++matches; }
    }
    return matches;
}

/// Return the number of seconds the child has apparently been running.
/// If the process is not running, return the duration of the last run.
/// If the process did not run, return 0.
///
time_t Child_mgr::uptime() const
{
    if (m_start_time > 0) {
        if (running()) {
            return (time(0) - m_start_time);
        }
        if (m_exit_time > m_start_time) {
            return (m_exit_time - m_start_time);
        }
    }
    return 0;
}

/// Check run status and return true if child is running.
///
bool Child_mgr::running() const
{
    return (m_obs_phase==ChildPhase::running);
}

/// Returns true if the observed phase is "gone".
/// NOTE: This does not imply *successful* completion....
///
bool Child_mgr::completed() const
{
    return (m_obs_phase == ChildPhase::gone);
}

/// Set the smallest number of seconds that a run should take.
/// This might be 0.  This parameter is used to detect runs that
/// terminate abnormally quickly.
///
void Child_mgr::set_min_run( time_t secs )
{
    m_min_run = secs;
}

/// Sets the child working directory to child_path.
///
void Child_mgr::set_wdir( const boost::filesystem::path &child_path )
{
    m_chdir = child_path;
}


/// Set the user friendly name string to nm.
///
void Child_mgr::set_name( const std::string &nm )
{
    m_name = nm;
}

/// Set the largest number of seconds that a run should take.  This
/// parameter is used to detect runs that take too long to complete.
/// On most 32-bit platforms this is at most 2147483647, which is
/// just over 68 years.
///
void Child_mgr::set_max_run( time_t secs )
{
    m_max_run = secs;
}

/// Set a particular binary path
void Child_mgr::set_binary( const boost::filesystem::path &p )
{
    m_bin_path = p;
}

/// Add an argument. All arguments are *copied* into m_args
/// against the possibility that they might go away before use.
///
void Child_mgr::add_arg(const char *a)
{
    m_args.push_back(a);
}

/// Add an argument from a string
///
void Child_mgr::add_arg(const std::string& s)
{
    m_args.push_back(s);
}

/// Add an integer argument
///
void Child_mgr::add_arg( int i )
{
    m_args.push_back(std::to_string(i));
}


/// Clear all arguments
void Child_mgr::clear_args()
{
    m_args.clear();
}

/// Child exited with given status, reason=CLD_EXITED, (0 is
/// considered a normal exit) or was killed, reason=CLD_KILLED.
/// Track run time if it was a non-zero status or ran too briefly.
///
/// N.B. This is called within SIGCHLD signal handler, so no I/O or other
/// unsafe function calls. It should never throw an exception.
///
void Child_mgr::postmortem( int status, int reason )
{
    m_old_pid = m_pid;
    m_pid = NOTAPID;
    m_exit_status = status;
    m_exit_reason = reason;
    m_obs_phase = ChildPhase::gone;
    m_exit_time = time(0);                  // time of exit is now
    time_t run_secs = (m_exit_time - m_start_time); // how long did it run?
    //
    if ((m_exit_status != 0) or (run_secs < m_min_run)) {
        m_fails.push_back( m_exit_time );   // remember this failure time
    }
    if (m_pty) { // If there is a pty, close it immediately (safe).
        m_pty->close_pty();
    }
}


/// Check the child process status, updating obs_phase:
///    gone, paused, running, completed, unknown
/// Member _pid may be zeroed if the process no longer observed.
///
/// N.B. This is called within the SIGCHLD handler, so no I/O or other
/// unsafe function calls.
///
/// * Will not throw
///
void Child_mgr::update_status( siginfo_t & infop )
{
    ++m_updates;
    if ((m_pid==NOTAPID) or (0==infop.si_pid)) return;

    switch(infop.si_code) {
    case 0:
        m_obs_phase = ChildPhase::running; // process is running
        break;
    case CLD_EXITED:
    case CLD_KILLED:
        postmortem( infop.si_status, infop.si_code );
        break;
    case CLD_STOPPED:
        m_obs_phase = ChildPhase::paused;
        break;
    case CLD_CONTINUED:
        m_obs_phase = ChildPhase::running;
        break;
    default:
        m_obs_phase = ChildPhase::unknown;
    }
}


/// Send a signal to the child; caller is responsible for the results.
/// May throw. Do not use this to kill, pause, or continue the child--
/// use the designated methods instead.
/// * May throw CM_exception
///
void Child_mgr::signal_child(int sig)
{
    // If we lack a valid pid, then just return
    if (m_pid == NOTAPID) {
        LOG_WARNING(Lgr) << "Child_mgr cannot signal " << m_name << ": no pid";
        throw CM_nochild_exception();
    }
    if (0 ==  kill(m_pid, sig)) {
        LOG_INFO(Lgr) << "Child_mgr signalled " << m_name << " pid=" << m_pid
                      << " with signal " << sig;
    }
    else {
        LOG_WARNING(Lgr) 
            << "Child_mgr failed to signal " << m_name << "pid=" << m_pid 
            << ": " << strerror(errno);
        throw CM_signal_exception();
    }
}

/// Wait up to wait_us MICROseconds for the observed phase to
/// transition to phase indicated by argument "tgt_phase".  Return
/// true if the observed phase is is the desired target phase.
///
/// * Will NOT throw
///
bool Child_mgr::wait_for_phase( ChildPhase tgt_phase, long wait_us )
{
    time_t secs = wait_us / 1'000'000;
    long     us = wait_us - secs*1000;
    struct timespec rem_ts { secs, us*1000 };

    while (m_obs_phase != tgt_phase) {
        struct timespec wait_ts = rem_ts;
        errno = 0;
        int rc = nanosleep( &wait_ts, &rem_ts );
        if (0 == rc) break;  // full wait accomplished
        if (errno != EINTR) {
            LOG_ERROR(Lgr) << "Child_mgr " << m_name << " wait for phase "
                           << phase_name(tgt_phase)
                           << " failed on error " << errno;
            break;
        } // else: interrupted, continue to wait the remaining rem_ts
    }
    if (m_obs_phase != tgt_phase) {
        LOG_WARNING(Lgr) << "Child " << m_name << "(" << m_pid
                         << ") persists in phase " << phase_name(m_obs_phase)
                         << " instead of transitioning to " << phase_name(tgt_phase);
        return false;
    }
    return true;
}


/// Stop (pause) the child process by sending SIGSTOP. It will signal
/// even if the child is not in a running state. Note that not all
/// applications will handle STOP/CONT signals smoothly. If wait_us > 0
/// then wait for up to that many microseconds for the child to report
/// it has stopped--if this does not occur then throw an exception.
/// If wait_us==0 (or negative) then just signal and return--do not verify.
///
/// * May throw CM_nochild_exception, CM_stop_exception, or CM_signal_exception
///
void Child_mgr::stop_child( long wait_us )
{
    signal_child( SIGSTOP );
    m_cmd_phase = ChildPhase::paused;
    if (0 >= wait_us) {
        return;   // Do not wait for child to stop or verify stopped
    }
    if (not wait_for_phase( ChildPhase::paused, wait_us )) {
        // if the child is apparently GONE then throw
        if (m_obs_phase == ChildPhase::gone) {
            throw CM_nochild_exception();
        }
        throw CM_stop_exception();
    }
}

/// Continue a paused child process by sending child SIGCONT. It will
/// signal the child even if it is not apparently stopped.  If a
/// non-zero wait_us is specified, the function will wait up to that
/// number of MICROseconds for the child to signal that is no longer
/// paused. (Aside: the child might also exit in that period).
/// If no transition is observed then throw.
///
/// * May throw CM_nochild_exception, CM_cont_exception, or CM_signal_exception
///
void Child_mgr::cont_child( long wait_us )
{
    signal_child( SIGCONT );
    m_cmd_phase = ChildPhase::running;
    if (0 == wait_us) {
        return;                 // Do not wait for child to resume
    }
    if (not wait_for_phase( ChildPhase::running, wait_us )) {
        if (m_obs_phase == ChildPhase::gone) {
            throw CM_nochild_exception();
        }
        throw CM_cont_exception();
    }
}


/// Signal child process to terminate gracefully.
/// Member m_terminate is set to the signal, e.g. SIGTERM.
/// If this fails, next time use SIGKILL (not child catchable).
/// - cmd_phase always becomes "gone".
/// - obs_phase might be "dying" or "gone".
/// - member m_pid may be updated if it appears to be already gone.
///
/// *  Will NOT throw.
///
void Child_mgr::kill_child( bool force, long wait_us )
{
    if (m_cmd_phase == ChildPhase::gone 
          and m_obs_phase == ChildPhase::gone) {
        return;   // was already noted as dead.
    }
    m_cmd_phase = ChildPhase::gone;

    // If we lack a valid pid, then presume it is somehow already dead
    if (m_pid == NOTAPID) {
        presume_dead( NOTAPID );
        return;
    }
    // If child is STOPPED, (and not force) first attempt to continue
    // the child. (A process cannot handle normal signals when
    // stopped.) Handle any errors from cont_child. If continuation
    // fails, force kill.
    //
    if ((m_obs_phase == ChildPhase::paused) and not force) {
        LOG_DEBUG(Lgr) << m_name << "(" << m_pid 
                       << ") is paused--first awaken, then kill"; 
        try {
            cont_child( wait_us ); // wait microseconds
        }
        catch( std::exception &err ) {
            LOG_ERROR(Lgr) << "Failed to unpause prior to kill: " 
                           << err.what();
            force = true;       // no more Mr. nice guy
        }
        m_cmd_phase = ChildPhase::gone; // maybe was reset to running
    }
    m_terminate = (force ? SIGKILL : SIGTERM);
    m_kill_time = time(0);

    int res = kill( m_pid, m_terminate ); // the system call
    if (0 == res) {
        LOG_DEBUG(Lgr) << "Child_mgr killed " << m_name << " pid=" << m_pid
            << " signal=" << (SIGTERM==m_terminate ? "SIGTERM" : "SIGKILL");
        wait_for_phase( ChildPhase::gone, wait_us );
    }
    else { // not much we can do if we cannot even send the signal
        presume_dead( res );
        LOG_WARNING(Lgr) 
            << "Child_mgr failed to kill " << m_name << " pid=" << m_pid 
            << " (Error " << res << ")  Presume dead.";
    }
}

/// Presume the child is dead and reset various member vars.
///
/// *  Will NOT throw.
///
void Child_mgr::presume_dead( int /* rc */ )
{
    m_obs_phase = ChildPhase::gone;
    m_old_pid = m_pid;
    m_pid = NOTAPID;
    m_terminate = 0;
    /* m_start_time = 0; */ // leave for possible postmortem
}

/// Launch the child process.  Clear args, then add arguments prior to
/// calling this function. Adding args prior to call must be
/// done for *every* invocation.  If a child is already running, it will
/// be killed ungently (sigkill) first.
///
/// * May throw CM_start_exception.
///
void Child_mgr::start_child()
{
    // if necessary, kill any current child process
    if (m_obs_phase != ChildPhase::gone) {
        kill_child(true);           // force; no throw
    }
    //
    std::vector<const char*> argv {};        // captive ptrs to args
    argv.push_back( m_bin_path.c_str() );    // arg0: the binary path
    for (std::string &s : m_args) {
        argv.push_back(s.c_str());
    }
    argv.push_back(nullptr);                 // terminate arg list
    m_cmd_phase = ChildPhase::running;
    //
    m_exit_status = 0;
    m_terminate = 0;
    m_start_time = 0;
    m_exit_time = 0;
    m_kill_time = 0;

    // if so configured, init pty
    if (m_pty) {
        m_pty->open_pty();
    }

    if (-1 == (m_pid = fork())) {
        LOG_ERROR(Lgr) << "Child_mgr for " << m_name << " failed to fork "
                       << m_bin_path ;
        throw CM_start_exception();
    }
    if (m_pid != 0) {
        // nonzero pid:  I am running in the parent process...
        m_start_time = time(0);
        m_obs_phase = ChildPhase::running;
        LOG_INFO(Lgr) << "Child_mgr started " << m_name 
                      << " child pid=" << m_pid ;
        return;
    }
    launch_child_binary(argv);
}

/// This run only in the child process.
/// Prepare file handles.  Exec the child binary.
///
void Child_mgr::launch_child_binary(std::vector<const char*> &argv)
{
    // TODO: close open files and drop privilege (if needed)

    // Prepare the pty, if enabled
    if (m_pty) {
        m_pty->child_init();
    }

    // Change Directory if indicated by m_chdir; if this fails we still try
    // to execute the child.
    if (not m_chdir.empty()) {
        if (chdir( m_chdir.c_str())) {
            LOG_ERROR(Lgr) << "Child_mgr fails to change working directory to"
                           << m_chdir;
        }
    }
    // Exec the child image -- should not return unless failed to exec.
    // in which case we exit(1) ... this will eventually be noticed
    // by the parent process.
    if (execv( m_bin_path.c_str(), const_cast<char* const*>(&argv[0]) )) {
        LOG_ERROR(Lgr) << "Child_mgr for " << m_name
                       << " failed to exec binary '" << m_bin_path  << "'";
        exit(1);
    }
}

/// Vacate some of the status information on a dead child.
///
void Child_mgr::clear_status()
{
    m_obs_phase = ChildPhase::gone;
    m_exit_reason = 0;  // Not a valid CLD_reason--ignored on check
    m_terminate = 0;
    m_old_pid = m_pid;
    m_pid = NOTAPID;
}

/// See whether the child is currently in an expected state for its
/// command phase.  This may be called periodically to verify the
/// child is acting sanely (returns True).
///
/// False is returned if:
/// (1) dead when it should be running or successfully completed
///   (a) exited with  a non-zero exit code
///   (b) died due to an unexpected signal
///   (c) exited with 0 code but much too quickly
///
/// (2) running when it shouldn't be
///   (a) should be gone because it was killed (dying)
///   (b) should have exited by now (uptime > m_max_time)
///   (b) should be paused because m_cmd_phase==paused
/// 
/// (3) paused when it should be running or gone
///
bool Child_mgr::check_child(RunCond &cond)
{
    bool rc=true;
    switch (m_obs_phase) {
    case ChildPhase::gone:
        rc = check_child_gone(cond);
        break;
    case ChildPhase::running:
        rc = check_child_running(cond);
        break;
    case ChildPhase::paused:
        rc = check_child_paused(cond);
        break;
    case ChildPhase::unknown:
        rc = false;
        cond = RunCond::unknown;
        break;
    }
    return rc;
}

/// Check child where the last observed state was "running".
///
bool Child_mgr::check_child_running(RunCond &cond)
{
    if (m_cmd_phase == ChildPhase::running) {
        // (2b)
        if (uptime() > m_max_run) {
            LOG_WARNING(Lgr) << "Child_mgr " << m_name << " pid=" << m_pid
                             << " running, "
                "but should be finished by now, limit=" << m_max_run;
            cond = RunCond::runTooLong;
            return false;
        }
        cond = RunCond::okay;
        return true;
    }
    // if child was signalled to kill it, allow a little time for it to die
    if (m_cmd_phase == ChildPhase::gone) {
        if ((time(0) - m_kill_time) < m_max_death_latency) {
            cond = RunCond::okay;
            return true;
        }
    }
    
    // Running when it should be gone, paused, or dying
    LOG_WARNING(Lgr) << "Child_mgr " << m_name << "pid=" << m_pid
                     << " running, "
        "but should be in phase: " << phase_name( m_cmd_phase );
    cond = RunCond::wrongState; // TODO: maybe too ambiguous
    return false;
}

/// Check child where the last observed state was "gone".
/// If the cmd_phase is also "gone" then it sets cond:=RunCond::okay,
/// and returns true.
///
/// If the process exited with status=0 and ran for at last m_min_run
/// seconds this is also returns true with cond:=RunCond::okay.
///
/// If the process exited with status==0 but run duration was < m_min_run,
/// then return false and cond:=RunCond::runTooShort.
///
/// If the process exited with status!=0 return false and cond:=RunCond::badExit.
/// The numerical exit status may be accessed with get_exit_status().
///
/// If the process was killed by an unhandled signal return false
/// with cond:=RunCond::sigKilled.  The fatal signal may be accessed
/// with get_exit_status().
///
bool Child_mgr::check_child_gone(RunCond &cond)
{
    if (m_cmd_phase == ChildPhase::gone){
        cond = RunCond::okay;
        return true;
    }
    // Dead.
    if (CLD_EXITED == m_exit_reason) {   // child called exit()
        // LOG_DEBUG(Lgr) << "Child_mgr " << m_name <<" pid=" << m_old_pid
        //                << " (" << m_bin_path
        //                << ") exit(" << m_exit_status << ")";
        if (0==m_exit_status) {
            if (uptime() > m_min_run) {
                cond = RunCond::okay;
                return true;
            } else {
                cond = RunCond::runTooShort; // that was too quick
                return false;
            }
        }
        cond = RunCond::badExit;
    }
    if (CLD_KILLED == m_exit_reason) { // child smote by signal
        LOG_DEBUG(Lgr) << m_name << " pid=" << m_old_pid 
                       << " (" << m_bin_path
                       << ") killed by sig(" <<  m_exit_status
                       << "), should be " << phase_name(m_cmd_phase);
        cond = RunCond::sigKilled;
    }
    return false;
}

/// Check child where the last observed state was "paused"
///
bool Child_mgr::check_child_paused(RunCond &cond)
{
    if ((m_cmd_phase == ChildPhase::paused)
        and (m_obs_phase == ChildPhase::paused)) {
        cond = RunCond::okay;
        return true;
    }
    // (3) Paused when it should be running, or gone
    LOG_WARNING(Lgr) << "Child_mgr " << m_name << "pid=" << m_pid << " is paused, "
        "but should be in phase: " << phase_name( m_cmd_phase );
    cond = RunCond::unexpectedPause;
    return false;
}

///////////////////////////////////////////////////////////////////////////
////                            Pty Methods

/// Enable a pseudoterminal for the child. Call this before starting
/// the child. This will not affect any currently running child. Once
/// enabled, this option may not be disabled.  The pty will automatically
/// be closed when the Child_mgr is deleted.
///
void Child_mgr::enable_pty()
{
    if (m_pty) return;
    if (running()) {
        LOG_WARNING(Lgr) << "Pty will not be available to current child.";
    }
    m_pty = std::make_unique<Pty_controller>();
}


/// Return true iff there is currently a pty attached.
///
bool Child_mgr::has_pty() const
{
    return ( static_cast<bool>( m_pty ) );
}



/// Retrieve the device name of the pseudoterminal.
/// * May throw
///
const std::string& Child_mgr::pty_remote_name() const
{
    if (not m_pty) throw CM_no_pty_exception();
    return m_pty->remote_name();
}

/// Reads will timeout after this period.
/// * May throw
///
void Child_mgr::set_pty_read_timeout( long secs, long usecs )
{
    if (not m_pty) throw CM_no_pty_exception();
    m_pty->set_read_timeout( secs, usecs );
}

/// Writes will timeout after this period.
/// * May throw
///
void Child_mgr::set_pty_write_timeout( long secs, long usecs )
{
    if (not m_pty) throw CM_no_pty_exception();
    m_pty->set_write_timeout( secs, usecs );
}

/// Set the size of the screen seen by the child
/// * May throw
///
void Child_mgr::set_pty_window_size(unsigned rows, unsigned cols)
{
    if (not m_pty) throw CM_no_pty_exception();
    m_pty->set_window_size(rows, cols);
}

/// Read a string from the child, up to maxbytes in length
/// * May throw
///
ssize_t Child_mgr::pty_read_nb( std::string &s, ssize_t maxbytes)
{
    if (not m_pty) throw CM_no_pty_exception();
    return m_pty->read_nb(s, maxbytes);
}

/// Write the string to the child's stdin.
/// * May throw
///
ssize_t Child_mgr::pty_write_nb( const std::string &s )
{
    if (not m_pty) throw CM_no_pty_exception();
    return m_pty->write_nb(s);
}
