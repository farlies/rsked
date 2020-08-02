/**
 * Part of the rsked package. Monitor volume monitoring for output channels.
 * If the volume is nil for more than D (~30) seconds, then shared memory
 * is updated to indicate TOO_QUIET.
 *
 * References:
 * - https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/
 * - https://gavv.github.io/articles/pulseaudio-under-the-hood/
 * - http://0pointer.net/lennart/projects/pavumeter/
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

#include <signal.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <math.h>
#include <iostream>
#include <string>
#include "vumonitor.hpp"
#include "itimer.hpp"
#include "logging.hpp"
#include "configutil.hpp"
#include "version.h"
#include <boost/program_options.hpp>

/// Prototypes for low level PA callbacks:
static void cgxi_cb(pa_context*,const pa_server_info*,void*);
static void cgsi_cb(pa_context*,const pa_sink_info *,int,void*);
static void cstt_cb(pa_context*, void *);
static void strr_cb(pa_stream*, size_t, void *);
static void strt_cb(pa_stream*, int, void *);
static void strs_cb(pa_stream*, void *);


/// Official application name
const char *AppName = "vumonitor";

/// Faults opening or updating shm
class shm_exception : public std::exception {
public:
    const char *what() const noexcept {return "Shared memory runtime error.";}
};

/// Flag will be set to true if we must exit main loop due to
/// signal or condition.
///
bool Terminate = false;

/// Log the version and compilation information.
/// If force==false, this will only print every HOUR=3600s.
/// with the intention of getting this info in every log file.
///
void log_banner(bool force)
{
    static time_t last=0;
    time_t now = time(0);
    if ( ((now - last) < 3600) and not force ) {
        return; 
    }
    LOG_INFO(Lgr) << AppName << " version "
                  << VERSION_STR "  built "  __DATE__ " " __TIME__ ;
    last = now;
}

///////////////////////////////// Monitor ///////////////////////////////////////


/// This class implements the main loop of the monitor.
///
class VU_monitor
{
private:
    std::string m_device_name {};
    std::string m_device_description {};
    pa_mainloop* m_mainloop { nullptr };
    pa_context *m_context { nullptr };
    pa_stream  *m_stream { nullptr };
    bool m_terminate  { false };
    bool m_debug { false };
    //
    key_t m_shmkey {0};
    int   m_shm_id {0};              // id of shared memory
    VU_status *m_shm_status {nullptr}; // the shared memory
    float m_level { 0.0F };
    float m_decay_rate { 0.005F };
    int m_timeout_ms { 40000 }; // for main loop [-1: blocking]
    unsigned long m_checks {0};
    unsigned long m_max_samples {0};
    size_t m_accum_samples {0};
    IntervalTimer m_quiet_timer;   // flag too quiet if this expires
    VU_announce m_last_announce { VU_NA }; // last declared quiet status
    //
    void check_quiet();
    void do_fade();

public:
    void context_ready(pa_context *);
    void newstream( const pa_sink_info * );
    void force_clear_status();
    int  mainloop_iterate();
    void read_sample(pa_stream *, size_t );
    void run_mainloop( pa_mainloop* );
    void set_debug(bool p) { m_debug = p; }
    void setup_shm();
    void stream_state( pa_stream * );
    void terminate() { m_terminate = true; }
    time_t timeout_secs() const { return m_quiet_timer.timeout_secs(); };
    void update_status(VU_announce);
    //
    VU_monitor(key_t,time_t);
    ~VU_monitor();
    VU_monitor(const VU_monitor&) = delete;
    void operator=(VU_monitor const&) = delete;
};


/// Establish shared memory with a block to hold struct vu_status
/// Do not throw since this is called by ctor.
///
void VU_monitor::setup_shm()
{
    m_shm_id = shmget( m_shmkey, sizeof(VU_status), (IPC_CREAT|0660) );
    //
    if (m_shm_id==(-1)) {
        LOG_ERROR(Lgr) << "Failed to get shared memory: "
                      << strerror(errno);
    } else {
        errno=0;
        m_shm_status=static_cast<VU_status*>( shmat(m_shm_id,NULL,0) ); // RW
        
        if ((void*)(-1)==m_shm_status) {
            LOG_ERROR(Lgr) << "Failed to attach shared memory: "
                          << strerror(errno);
            m_shm_status = nullptr;
        } else {
            LOG_INFO(Lgr) << "Shared memory created with key: " << m_shmkey;
            force_clear_status();
        }
    }
}


/// CTOR: specify a timeout in seconds
///
VU_monitor::VU_monitor(key_t k, time_t secs) 
    : m_shmkey(k), m_quiet_timer(secs)
{
    setup_shm();
}


/// DTOR:  detach from shared memory
///
VU_monitor::~VU_monitor()
{
    if (m_shm_status) {
        shmdt((const void*) m_shm_status);
    }
}

/// Make sure the status is neutral.
///
void VU_monitor::force_clear_status()
{
    if (!m_shm_status) return;

    m_shm_status->m_quiet = VU_NA;
    m_shm_status->m_lvl = 0.0F;
    m_shm_status->m_ts = time(0);
}

/// Update shared memory.  This assumes that ONLY vumonitor will
/// update the shared memory and only ONE vumonitor is running.
///
void VU_monitor::update_status(VU_announce p)
{
    if (!m_shm_status) return;

    m_shm_status->m_quiet = p;
    m_shm_status->m_lvl = m_level;
    m_shm_status->m_ts = time(0);
    if (p == m_last_announce) return;
    
    // log a change to status
    switch(p) {
    case VU_TOO_QUIET:
        LOG_WARNING(Lgr)  << "TOO QUIET check #"  << m_checks
                          << "  max_samples=" << m_max_samples;
        break;
    case VU_DETECTED:
        LOG_INFO(Lgr) << "Audio output detected again";
        break;
    case VU_NA:
    default:
        LOG_INFO(Lgr) << "VU level unavailable--stay tuned.";
    }
    m_last_announce = p;
}


/// Called when we received a PA_CONTEXT_READY
///
void VU_monitor::context_ready(pa_context *pc)
{
    if (m_stream) {
        LOG_ERROR(Lgr) << "CONTEXT_READY but already have stream!?";
        return;
    }
    if (not m_device_name.empty()) {
        auto op = pa_context_get_sink_info_by_name(pc, m_device_name.c_str(),
                                                   cgsi_cb, this);
        pa_operation_unref(op);
    }
    else {
        auto op = pa_context_get_server_info(pc, cgxi_cb, this);
        pa_operation_unref(op);
    }
}


/// Called when we know the name and description of the stream.
/// Invoke pa_stream_new, setup state and read callbacks, and
/// connect record.
///
void
VU_monitor::newstream( const pa_sink_info *psink )
{
    if (not psink) return;
    const pa_sample_spec &ss = psink->sample_spec;
    const pa_channel_map &cmap = psink->channel_map;

    m_device_name = psink->monitor_source_name;
    m_device_description = psink->description;
    
    LOG_INFO(Lgr) << "Source name: " << m_device_name;
    LOG_INFO(Lgr) << "Device description: " << m_device_description;

    pa_sample_spec spec { PA_SAMPLE_FLOAT32, ss.rate, ss.channels };
    m_stream = pa_stream_new( m_context, AppName, &spec, &cmap);
    pa_stream_set_read_callback( m_stream, strr_cb, this);
    pa_stream_set_state_callback( m_stream, strs_cb, this);
    pa_stream_connect_record( m_stream, m_device_name.c_str(),
                              /*attr*/ NULL, (enum pa_stream_flags) 0);
}


/// Peek into the stream to get a pointer to floats.  Retain the peak
/// amplitude in the sample.  For multichannel data, this is
/// structured as a sequence of samples for each channel, e.g. for 2
/// channels: L0,R0, L1,R1, L2,R2, L3,R3, ... but we don't really care
/// about this for detecting silence.
///
void
VU_monitor::read_sample(pa_stream *pstream, size_t len)
{
    const void *pvoid {nullptr};
    if (len > m_max_samples) { m_max_samples = len; }  // debug
    if (len) { m_accum_samples += len; }
    if (pa_stream_peek( pstream, &pvoid, &len ) < 0) {
        LOG_ERROR(Lgr) << "pa_stream_peek() failed: "
                  <<  pa_strerror(pa_context_errno(m_context));
        return;
    }
    const float *d = static_cast<const float*>( pvoid );
    unsigned nsamples = static_cast<unsigned>((len/sizeof(float)));
    while (nsamples) {
        float v = fabs(*d);
        if (v > m_level) m_level = v; // retain peak
        --nsamples;
        ++d;
    }
    pa_stream_drop( pstream );
}


/// If the stream becomes ready then update the timing info.
///
void VU_monitor::stream_state( pa_stream *s )
{
    pa_operation *op { nullptr };
    switch (pa_stream_get_state(s)) {
    case PA_STREAM_READY:
        op = pa_stream_update_timing_info(m_stream, strt_cb, this);
        pa_operation_unref( op );
        break;
    case PA_STREAM_FAILED:
        LOG_ERROR(Lgr)  << "PA Stream failure";  // ugh--how to respond?
        break;
    case PA_STREAM_TERMINATED:
        terminate();
        break;
    default:                    // CREATING, UNCONNECTED, ...
        break;
    }
}


/// Called on timer to cause Level to decay to 0 by a constant step (unless reset
/// by audio coming in.
///
void VU_monitor::do_fade()
{
    if (m_level <= 0.0) return;
    if (m_level > m_decay_rate) {
        m_level -= m_decay_rate;
    } else {
        m_level = 0.0;
    }
}


/// Run the main loop, monitoring peak levels from samples that arrive.
///
void  VU_monitor::run_mainloop( pa_mainloop* ml )
{
    int err;
    m_mainloop = ml;

    // create a context
    m_context = pa_context_new(pa_mainloop_get_api(m_mainloop), "VU_Monitor");
    if (!m_context) {
        LOG_ERROR(Lgr) << "pa_context_new returns NULL";
        return;
    }
    err = pa_context_connect( m_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
    if (err) {
        LOG_ERROR(Lgr) << "pa_context_connect fails: " << pa_strerror(err);
        return;
    }

    pa_context_set_state_callback( m_context, cstt_cb, this);

    // run main loop until m_terminate or global Terminate is true;
    while (!m_terminate and !Terminate) {
        m_accum_samples = 0;
        err = mainloop_iterate();
        if (err < 0) {
            LOG_ERROR(Lgr) << "pa_mainloop: " << pa_strerror(err);
            terminate();
            break;
        }
        // std::cout << "accum_samples=" << m_accum_samples << "  level="
        //          << m_level << std::endl;
        do_fade();
        check_quiet();
        log_banner(false);
    }
    // Cleanup
    if (m_stream) {
        pa_stream_disconnect(m_stream);
        pa_stream_unref(m_stream);
    }
    pa_context_disconnect(m_context);
    pa_context_unref(m_context);
}


/// Evaluate Level.  If we observe uninterrupted 0 level checks for
/// more than m_timeout seconds, raises the flag.  As soon as sound
/// returns, lower the flag.
///
void VU_monitor::check_quiet()
{
    bool level_is_zero = (m_level==0.0);
    m_checks++;

    if (m_last_announce == VU_TOO_QUIET) {
        if (not level_is_zero) {    // saw some dBs: clear flag, reset timer
            m_quiet_timer.stop();
            update_status(VU_DETECTED);
        } else {
            update_status(VU_TOO_QUIET); // still too quiet...
        }
        return;
    }
    // vvvvvvvvvvvvvvv *Not* yet flagged as TOO_QUIET vvvvvvvvvvvvvvvvv
    if (level_is_zero) {
        m_quiet_timer.start();
        // if (m_debug) {
        //     LOG_DEBUG(Lgr) << "level=0, starting quiet timer";
        // }
        if (m_quiet_timer.expired()) {
            update_status(VU_TOO_QUIET);
        } else {
            update_status(VU_DETECTED); // time remains before we alarm
        }
    } else {
        m_quiet_timer.stop();
        update_status(VU_DETECTED); // audio continues to be detected
    }
}


/// Execute one trip through the main loop, with timeout. This handles
/// I/O and timer events.
///
int VU_monitor::mainloop_iterate()
{
    int r;
    if (!m_mainloop) return (-1);

    r = pa_mainloop_prepare( m_mainloop, m_timeout_ms );
    if (r < 0) {
        return r;
    }
    r = pa_mainloop_poll(m_mainloop);
    if (r < 0) {
        return r;
    }
    // Dispatch timeout, io & deferred events from the previous poll.
    r = pa_mainloop_dispatch(m_mainloop);
    return r;
}



////////////////////////////////// Callbacks /////////////////////////////////////

/// Handle an answer to the Sink query. If it contains pa_sink_info then we can
/// create a stream via newstream. (Otherwise we cannot do much.)
///
static void
cgsi_cb( pa_context*, const pa_sink_info *sink, 
                         int idx, void *objptr)
{
    if (idx < 0) {
        LOG_ERROR(Lgr) << "detected unknown sink";
    }
    else if (sink and objptr) {
        VU_monitor *mon = (VU_monitor*)(objptr);
        mon->newstream(sink);
    }
}


/// Handle an answer to a Server query. The hope is that it contains a
/// default sink name, and we can then query sink information.
///
static void
cgxi_cb(pa_context *pcontext, const pa_server_info *server, void *obj)
{
    if (not server) {
        LOG_ERROR(Lgr)  << "null server info?";
    }
    else if (not server->default_sink_name) {
        LOG_ERROR(Lgr)  << "missing default sink name?";
    }
    else {
        auto op = pa_context_get_sink_info_by_name(pcontext, server->default_sink_name,
                                                   cgsi_cb, obj);
        pa_operation_unref(op);
    }
}

/// Stream Data Callback.
///
static void
strr_cb( pa_stream *stream, size_t len, void *pmon )
{
    if (pmon) {
        auto mon = static_cast<VU_monitor*>( pmon );
        mon->read_sample( stream, len );
    }
}

/// Context State Callback - will be invoked when the context is ready, and will
/// get sink and server information (handled by callbacks cgsi_cb
/// and cgxi_cb).
///
static void
cstt_cb(pa_context *pcontext, void *pmon)
{
    auto mon = static_cast<VU_monitor*>( pmon );
    switch ( pa_context_get_state(pcontext) ) {
        case PA_CONTEXT_READY:
            mon->context_ready(pcontext);
            break;
        case PA_CONTEXT_FAILED:
            LOG_ERROR(Lgr) << "cstt_cb: context failed";
            mon->terminate();
            break;
        case PA_CONTEXT_TERMINATED:
            LOG_ERROR(Lgr) << "cstt_cb: context terminates";
            mon->terminate();
            break;
    default:
        break;  // unconnected, connecting, authorizing, setting_name
    }
}


/// Stream State Callback.
///
static void strs_cb(pa_stream *s, void *pmon)
{
    if (pmon) {
        auto mon = static_cast<VU_monitor*>( pmon );
        mon->stream_state( s );
    }
}


/// Timing callback -- we are ignoring this for now.
///
static void strt_cb(pa_stream*, int, void *)
{
    // maybe extract latency info
}


//////////////////////////////////////////////////////////////////////////////////

/// Signal handler function for various signals.
/// This is handled in the main loop.
///
void my_sigterm_handler(int s)
{
    if ((s == SIGTERM) || (s == SIGINT)) {
        Terminate = true;
    }
}

/// Handle SIGTERM, SIGINT. Ignore SIGPIPE (from PulseAudio).
///
void setup_signal_handler()
{
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = my_sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

/* This application runs as a series of callbacks invoked from the
 * PulseAudio asynchronous API. Here is the typical sequence
 * establishing callbacks.
 *
 *   main
 *    \_ cstt_cb
 *        \_ cgxi_cb
 *            \_ cgsi_cb
 *                \_ newstream
 *                    |- strs_cb
 *                    |   \_ strt_cb
 *                    |- strr_cb
 *
 */
int main(int ac, char **av)
{
    namespace po = boost::program_options;
    key_t key_id = IPC_PRIVATE;
    int rc=0;
    unsigned timeout_secs=40;
    pa_mainloop *mainloop=nullptr;
    auto  logpath = expand_home( "~/logs/vumonitor_%5N.log" );
    //
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help","option information")
        ("debug","show debug level messages in logs")
        ("shmkey",po::value<int>(),"shared memory key for status info")
        ("timeout",po::value<unsigned>(),"quiet threshold in seconds")
        ("test","test only")
        ("console","echo log to console in addition to log file");
    po::variables_map vm;
    po::store( po::parse_command_line(ac,av,desc),vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << AppName << " - " 
                  << VERSION_STR "  built "  __DATE__ " " __TIME__ "\n";
        std::cout << desc << "\n";
        exit(0);
    }
    if (vm.count("shmkey")) {
        key_id = vm["shmkey"].as<int>();
    }
    if (vm.count("timeout")) {
        timeout_secs = vm["timeout"].as<unsigned>();
    }
    bool test_mode = (vm.count("test") > 0);
    bool console_log = (vm.count("console") > 0);
    int flags = LF_FILE;
    if (console_log) { flags |= LF_CONSOLE; }
    if (test_mode) { flags = LF_CONSOLE; }
    if (vm.count("debug") > 0) { flags |= LF_DEBUG; }

    // Assume it is always a child process of rsked so no running check
    setup_signal_handler();
    init_logging(AppName,logpath.c_str(), flags);
    log_banner(true);

    // create a main loop
    mainloop = pa_mainloop_new();
    if (!mainloop) return (2);

    try {
        VU_monitor Monitor(key_id, timeout_secs);
        LOG_INFO(Lgr)  << "Monitoring audio playback levels";
        LOG_INFO(Lgr)  << "Threshold quiet period: " << Monitor.timeout_secs()
                       << " seconds";
        if (test_mode) {
            Monitor.set_debug(true);
            LOG_INFO(Lgr) << "Test mode enabled";
        }
        Monitor.run_mainloop( mainloop );
    }
    catch(...) {
        pa_mainloop_free(mainloop);
        rc = 1;
    }
    if (Terminate and not rc) {
        LOG_ERROR(Lgr) << "Terminated on signal";
    }
    finish_logging();
    return rc;
}


