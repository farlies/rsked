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


#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>

#include "util/config.hpp"
#include "util/configutil.hpp"
#include "util/jobutil.hpp"
#include "cooling.hpp"
#include "status.h"
#include "version.h"

namespace fs = boost::filesystem;

/// Flag will be set to true if we must exit main loop due to
/// signal or condition. Note the signal used to terminate.
///
bool g_Terminate = false;
int  g_TermSignal {0};

/// We were signaled HUP: reload configuration
bool g_ReloadReq = false;



/// Signal handler function for various signals records the signal in
/// globals g_Terminate or HUP.  These are monitored in the run loop.
/// Other signals (e.g. USR1, have no effect).
void my_sigterm_handler(int s)
{
    if ((s == SIGTERM) || (s == SIGINT) || (s==SIGQUIT)) {
        g_Terminate = true;
        g_TermSignal = s;
    }
    else if (s == SIGHUP) {
        g_ReloadReq = true;
    }
}


/// Return name associated with shared memory status word
///
const char* rsk_modename( uint32_t s )
{
    switch (s) {
    case RSK_PLAYING:
        return "Playing";
    case RSK_INITIALIZING:
        return "Initializing";
    case RSK_OFF:
        return "Off";
    case RSK_PAUSED:
        return "Paused";
    default:
        return "Unknown";
    }
}

/// Best effort to kill off processes matching pattern,
/// m_kill_pattern, if set.  Only processes whose names exactly match
/// the pattern will be affected.  Warning: this will block until any kill
/// actions complete.
///
void Cooling::kill_aux_processes()
{
    stop_mpd_player();
    //
    if (! m_kill_pattern.empty() ) {
        std::string pcmd("/usr/bin/pkill -x -TERM ");
        pcmd += m_kill_pattern;
        LOG_INFO(Lgr) << "kill_aux_processes matching: " << m_kill_pattern;
        int rc1 = system( pcmd.c_str() );
        if (WIFEXITED(rc1)) {
            int rc = WEXITSTATUS(rc1);
            if (rc > 1) {  // status=1 means no matching processes
                LOG_ERROR(Lgr) << "kill_aux_processes--pkill exits with status " << rc;
            }
        } else { // abnormal:  stopped, killed by signal, continued
            LOG_ERROR(Lgr) << "kill_aux_processes: pkill had unexpected problems.";
        }
    }
}


/// Use mpc to stop mpd if the m_mpd_stop_cmd is set.
///
void Cooling::stop_mpd_player()
{
    if (m_mpd_stop_cmd.empty()) return;
    //
    LOG_INFO(Lgr) << "Stop mpd from playing with: " << m_mpd_stop_cmd;
    int rc2 = system(m_mpd_stop_cmd.c_str());
    if (WIFEXITED(rc2)) {
        int rc = WEXITSTATUS(rc2);
        if (rc > 0) {
            LOG_ERROR(Lgr) << "kill_aux_processes--mpd stop exits with status "
                           << rc;
        }
    } else {
        LOG_INFO(Lgr) << "kill_aux_processes: " << m_mpd_stop_cmd
                      << "  had unexpected problems";
    }
}

//////////////////////////////// Cooling /////////////////////////////////////

/// CTOR. Just records the program options.
/// 1. Test mode
/// 2. Console log option
/// 3. Configuration file name
///
Cooling::Cooling(boost::program_options::variables_map &vm)
    : m_last_blink(boost::posix_time::microsec_clock::universal_time()),
      m_cfg_path (expand_home("~/.config/rsked/cooling.json")),
      m_rsked_bin_path( expand_home("~/bin/rsked") ),
      m_rsked_cfg_path( expand_home("~/.config/rsked/rsked.json") )
{
    m_test_mode = (vm.count("test") > 0);
    m_console_log = (vm.count("console") > 0);
    if (vm.count("config") > 0) {
        m_cfg_path = expand_home(vm["config"].as<std::string>());
    }
}


/// DTOR
Cooling::~Cooling()
{
    teardown_gpio();
    teardown_shm();
    // rsked child is killed by Child_mgr DTOR
    finish_logging();
}

/// Install a plain function that handles signals SIGTERM, SIGINT, SIGHUP, SIGUSR1.
/// This does not depend on the existence of any instance of Cooling.
///
void Cooling::setup_sigterm_handler()
{
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = my_sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL); // ignored currently
}


/// Return the number of errors encountered.
///
unsigned Cooling::error_count() const
{
    return( m_rsked_errors + m_gpio_errors );
}

/// Attempt to load the config file and populate parameters.  This can
/// throw a config error if the file is missing or contents are
/// invalid. Does not run anything or have filesystem or GPIO side effects,
/// but it WILL modify this Cooling instance.
///
/// * May throw a Cooling_config_exception.
///
void Cooling::initialize(bool debug)
{
    Config cfg(m_cfg_path.c_str());
    m_debug = debug;

    cfg.read_config();  // may throw

    if (cfg.get_schema() != "1.1") {
        LOG_ERROR(Lgr) << "Invalid schema ('" << cfg.get_schema()
                       << "') in " << m_cfg_path;
        throw Cooling_config_exception();
    }

    // Logging -- don't do this if it has been done once already
    if (not m_logging_up) {
        auto  logpath = expand_home( "~/logs/cooling_%5N.log" );
        int flags = LF_FILE;
        if (m_console_log) { flags |= LF_CONSOLE; }
        if (m_test_mode) { flags = LF_CONSOLE; }
        if (m_debug) { flags |= LF_DEBUG; }
        init_logging( AppName, logpath.c_str(), flags );
        m_logging_up = true;
    }
    // General: version, application
    cfg.log_about();
    m_cfgversion = "?";
    cfg.get_string("General","version",m_cfgversion);
    m_cfgdesc = "?";
    cfg.get_string("General","description",m_cfgdesc);
    //
    std::string cfg_appname;
    cfg.get_string("General","application", cfg_appname);
    if (cfg_appname != AppName) {
        LOG_WARNING(Lgr) << "unexpected application name: '"
                         << cfg_appname << "' in " << m_cfg_path;
    }
    // General: Polling etc
    const long DefaultPollMsec=1000;
    const long MAX_POLL_MSEC = 5000;
    long poll_msec = DefaultPollMsec;
    cfg.get_long("General","poll_msec",poll_msec);
    if ((poll_msec > MAX_POLL_MSEC) or (poll_msec < 1)) {
        LOG_ERROR(Lgr) << "poll_msec must be between 1 and " << MAX_POLL_MSEC;
        poll_msec = DefaultPollMsec;
        LOG_INFO(Lgr) << "poll_msec := " <<poll_msec;
    }
    m_poll_timespec.tv_sec = poll_msec/1000;
    poll_msec -= 1000 * m_poll_timespec.tv_sec;
    m_poll_timespec.tv_nsec = (poll_msec * 1'000'000);
    cfg.get_unsigned("General","poll_trace",m_poll_trace);
    LOG_DEBUG(Lgr) << "poll_timespec: "
                   << m_poll_timespec.tv_sec << "s + "
                   << m_poll_timespec.tv_nsec <<  "ns";
    // Other sections:
    init_rsked( cfg );
    init_cooling( cfg );
    init_snooze_button( cfg );
    init_panel_leds( cfg );
    //
    log_banner(true);
}


/// Initialize rsked functionality
///
void Cooling::init_rsked(  Config &cfg )
{
    cfg.get_bool("Rsked","rsked_enabled",m_rsked_enabled);
    if (not m_rsked_enabled) {
        return;  // rest of section is ignored if not enabled
    }
    cfg.get_string("Rsked","kill_pattern",m_kill_pattern);
    cfg.get_string("Rsked","mpd_stop_cmd",m_mpd_stop_cmd);
    //
    cfg.get_bool("Rsked","rsked_debug",m_rsked_debug);
    cfg.get_pathname("Rsked","rsked_cfg_path",FileCond::MustExist,
                     m_rsked_cfg_path);
    cfg.get_pathname("Rsked","rsked_bin_path",FileCond::MustExist,
                     m_rsked_bin_path);
    // init child manager for rsked
    m_rsked_cm = Child_mgr::create( m_rsked_bin_path );
    m_rsked_cm->set_name("rsked");
    m_rsked_cm->set_min_run(INT_MAX);
}

/// Respond to SIGUSR1 by reloading the configuration file, e.g. cooling.json.
/// The primary use of this is to adjust the temperature thresholds, polling,
/// and debug flag. However the following significant effects might occur:
/// - if rsked had been enabled and is now disabled, the running rsked is killed
/// - if rsked had been disabled and is now enabled, an rsked will be started
/// - if any rsked parameters changed, rsked is restarted
///
void Cooling::reload_config()
{
    bool     old_ren  { m_rsked_enabled };
    bool     old_rdb  { m_rsked_debug };
    fs::path old_rbp  { m_rsked_bin_path };
    fs::path old_rcp  { m_rsked_cfg_path };

    try {
        initialize(m_debug);
        // possibly stop running rsked
        if (m_rsked_enabled and old_ren) {
            if ((m_rsked_debug != old_rdb) or (m_rsked_bin_path != old_rbp)
                or (m_rsked_cfg_path != old_rcp))
            {
                LOG_WARNING(Lgr) << "Rsked must be restarted with new parameters";
                terminate_rsked();
            }
        }
        start_rsked();          // will stop rsked if it should not be running
    }
    catch (const std::exception &xxx) {
        std::cerr << "Cooling: Failed to reload configuration: " << xxx.what() << '\n';
        LOG_ERROR(Lgr) << "Failed to reload configuration: " << xxx.what();
    }
}

/// Log the version and compilation information.
/// If force==false, this will only print every HOUR=3600s.
/// with the intention of getting this info in every log file.
///
void Cooling::log_banner(bool force)
{
    constexpr const time_t BANNER_INTERVAL_SECS {3600};
    time_t now = time(0);
    if ( ((now - m_last_banner) < BANNER_INTERVAL_SECS) and not force ) {
        return;
    }
    LOG_INFO(Lgr) << AppName << " version "
                  << VERSION_STR " ("  __DATE__ " " __TIME__  << ")";
    LOG_INFO(Lgr) << "config: " << m_cfgversion << ", " << m_cfgdesc;
    m_last_banner = now;
}


/// Initialize fan control parameters and GPIO, if enabled.
/// \arg \c cfg  Configuration object
///
/// * May throw a Cooling_configuration_exception
///
void Cooling::init_cooling( Config &cfg )
{
    cfg.get_bool("Cooling", "enabled", m_fan_control_enabled);
    cfg.get_double("Cooling","cool_stop_temp",m_cool_stop_temp);
    cfg.get_double("Cooling","cool_start_temp",m_cool_start_temp);
    if (m_cool_start_temp <= m_cool_stop_temp) {
        LOG_ERROR(Lgr) << "cool_start_temp <= cool_stop_temp";
        throw Cooling_config_exception();
    }
    if (m_cool_stop_temp < Fan_lowest_stop_temp) {
        LOG_ERROR(Lgr) << "cool_stop_temp < 25C - unreasonable!";
        throw Cooling_config_exception();
    }
    if (m_cool_start_temp > Fan_highest_start_temp) {
        LOG_ERROR(Lgr) << "cool_start_temp > 80C - CPU would be toast";
        throw Cooling_config_exception();
    }
    //
    int mcs=static_cast<int>(m_min_cool_secs);
    cfg.get_int("Cooling","min_cool_secs",mcs);
    if (mcs < Lowest_cool_secs) {
        LOG_ERROR(Lgr) << "min_cool_secs must be at least " << Lowest_cool_secs;
        throw Cooling_config_exception();
    }
    m_min_cool_secs = static_cast<time_t>(mcs);
    //
    unsigned pin_num=4;
    cfg.get_unsigned("Cooling","fan_gpio",pin_num);

    cfg.get_pathname("Cooling","sensor",FileCond::MustExist,
                     m_temp_sensor_path);

    if (m_test_mode) return;
    // Claim the GPIO--but only if fan control is enabled
    if (m_fan_control_enabled) {
        if (not config_gpio_pin( m_fan_gpio, pin_num, Gpio::OUT, GPIO_OFF)) {
            m_fan_control_enabled = false;
            LOG_ERROR(Lgr) << "No fan control available this session";
        } else {
            LOG_INFO(Lgr) << "Fan control enabled on GPIO " << pin_num;
        }
    } else if (m_fan_gpio) {    // release the gpio if held
        m_fan_gpio.set_value(0); // turn it off
        m_fan_gpio.release();
    }
}

/// Initialize snooze button and GPIO, if enabled.
/// \arg \c cfg  Configuration object
///
void Cooling::init_snooze_button( Config &cfg )
{
    cfg.get_bool("SnoozeButton", "enabled", m_snooze_button_enabled);
    unsigned pin_num=18;
    cfg.get_unsigned("SnoozeButton","button_gpio",pin_num);

    if (m_test_mode) return;

    // Claim the GPIO--but only if snooze button control is enabled
    if (m_snooze_button_enabled) {
        if (not config_gpio_pin( m_pbutton_gpio, pin_num, Gpio::IN, GPIO_NC)) {
            m_fan_control_enabled = false;
            LOG_ERROR(Lgr) << "No GPIO snooze button available this session";
        } else {
            LOG_INFO(Lgr) << "Snooze button enabled on GPIO " << pin_num;
        }
        m_last_pbstate = m_pbutton_gpio.get_value();
        LOG_INFO(Lgr) << "Button is now "
                      << ((GPIO_OFF == m_last_pbstate) ? "down" : "up");
    } else if (m_pbutton_gpio) { // release the gpio if held
         m_pbutton_gpio.release();
    }
}

/// Initialize snooze button and GPIO, if enabled.
/// \arg \c cfg  Configuration object
///
void Cooling::init_panel_leds( Config &cfg )
{
    cfg.get_bool("PanelLEDs", "enabled", m_panel_leds_enabled);


    // Claim the GPIO--but only if panel LEDs are enabled
    if (m_panel_leds_enabled) {
        unsigned green_pin_num=17;
        unsigned red_pin_num=27;
        cfg.get_unsigned("PanelLEDs","red_gpio",red_pin_num);
        cfg.get_unsigned("PanelLEDs","green_gpio",green_pin_num);
        if (m_test_mode) return;
        //
        if (not config_gpio_pin( m_grn_gpio, green_pin_num, Gpio::OUT, GPIO_OFF)) {
            LOG_ERROR(Lgr) << "No green GPIO panel LED available this session";
        } else {
            LOG_INFO(Lgr) << "Green panel LED enabled on GPIO " << green_pin_num;
        }
        if (not config_gpio_pin( m_red_gpio, red_pin_num, Gpio::OUT, GPIO_OFF)) {
            LOG_ERROR(Lgr) << "No red GPIO panel LED available this session";
        } else {
            LOG_INFO(Lgr) << "Red panel LED enabled on GPIO " << red_pin_num;
        }
    }  else {                    // release the gpio if held
        if (m_grn_gpio) {
            m_grn_gpio.set_value(0); // turn it off
            m_grn_gpio.release();
        }
        if (m_red_gpio) {
            m_red_gpio.set_value(0); // turn it off
            m_red_gpio.release();
        }
    }
}

//////////////////////////// GPIO Operations /////////////////////////////

/// Start the fan (if we have control of it)
///
void Cooling::start_fan()
{
    if (not m_fan_control_enabled) return;
    if (m_fan_running) return;
    if (m_fan_gpio) {
        try {
            m_fan_gpio.set_value(GPIO_ON); // turn fan on
            LOG_INFO(Lgr) << "Started FAN at temp =" << m_degc;
            m_last_cool_start = time(0);
            m_fan_running = true;
        } catch ( std::exception &ex ) {
            LOG_ERROR(Lgr) << "Fan Start operation failed: " << ex.what();
            m_gpio_errors++;
        }
    }
}

/// Stop the fan (if we have control of it) and it has been running at
/// least m_min_cool_secs.
///
void Cooling::stop_fan()
{
    if (not m_fan_control_enabled) return;
    if (!m_fan_running) return;
    if (m_fan_gpio) {
        time_t tnow = time(0);
        if ((tnow - m_last_cool_start) < m_min_cool_secs) return;
        try {
            m_fan_gpio.set_value(GPIO_OFF);
            LOG_INFO(Lgr) << "Halted FAN at temp=" << m_degc;
            m_fan_running = false;
        } catch ( std::exception &ex ) {
            LOG_ERROR(Lgr) << "Fan Stop operation failed: " << ex.what();
            m_gpio_errors++;
        }
    }
}

/// Set the state of the Red LED to st (true=ON).
///
void Cooling::illuminate_red( bool st )
{
    try {
        if (m_red_gpio) {
            m_red_gpio.set_value( (st ? GPIO_ON : GPIO_OFF) );
        }
        m_red_state = st;
    }
    catch(std::exception &ex) {
        LOG_ERROR(Lgr) << "Cooling::illuminate_red led: " << ex.what();
    }
}

/// Toggle the state of the Red LED
void Cooling::toggle_red()
{
    illuminate_red( not m_red_state);
}

/// Set the state of the Green LED to st (true=ON).
///
void Cooling::illuminate_grn( bool st )
{
    try {
        if (m_grn_gpio) {
            m_grn_gpio.set_value( (st ? GPIO_ON : GPIO_OFF) );
        }
        m_grn_state = st;
    }
    catch(std::exception &ex) {
        LOG_ERROR(Lgr) << "cooling illuminate_grn led: " << ex.what();
    }
}

/// Toggle the state of the Green LED
void Cooling::toggle_grn()
{
    illuminate_grn( not m_grn_state);
}

/// Wired with a pull-up resistor so that it is normally high (+3.3V).
/// If pbutton goes low (off, ~0.05V) then it is being depressed.
/// Insists on seeing button release (high) after press (low).
///
void Cooling::check_buttons()
{
    namespace pt = boost::posix_time;
    if (! m_pbutton_gpio ) { return; } // disabled

    int s = m_pbutton_gpio.get_value();
    //
    if ((GPIO_OFF == m_last_pbstate) and (GPIO_ON == s)) {
        m_last_pbdown = pt::microsec_clock::universal_time();
        illuminate_grn( false );
        if (m_rsked_enabled) {
            // signal pause
            LOG_INFO(Lgr) << "snooze button released, signal pause";
            m_rsked_cm->signal_child(SIGUSR1);
        } else {
            LOG_INFO(Lgr) << "snooze button pressed (no effect)";
        }
    }
    else if ((GPIO_ON == m_last_pbstate) and (GPIO_OFF == s)) {
        // button depressed but not yet released: light up as feedback
        illuminate_grn( true );
    }

    m_last_pbstate = s;
}


/// Establish the direction and initial stat of a GPIO_pin.
/// This catches all std exceptions
/// \arg \c line ref of unique_ptr to the line to configure
/// \arg \x pnum the pin number
/// \arg \c dir either Gpio::IN or Gpio::OUT direction
/// \arg \c state initial value (ON, OFF, NC to indicate don't set).
/// \return false on failure true on success.
///
bool
Cooling::config_gpio_pin( gpiod::line &line, unsigned pnum,
                          Gpio dir, int state )
{
    try {
        // if the line is already configured correctly, just return
        if (line) {
            int cdir=((dir==Gpio::IN) ? gpiod::line_request::DIRECTION_INPUT
                      : gpiod::line_request::DIRECTION_OUTPUT);
            if (line.offset()==pnum and line.direction()==cdir) {
                LOG_DEBUG(Lgr) << "GPIO Pin " << pnum
                               << "is already configured correctly";
                // maybe set state to state if an output
                return true;
            }
            LOG_DEBUG(Lgr) << "GPIO Pin " << pnum << " must be reconfigured--release";
            line.release();
        }
        if (!m_gpio_chip) {     // chip required to access any lines...
            m_gpio_chip = std::make_unique<gpiod::chip>("gpiochip0");
        }
        line = m_gpio_chip->get_line( pnum );
        if (Gpio::OUT == dir) {
            gpiod::line_request config_o
            {AppName,gpiod::line_request::DIRECTION_OUTPUT,0};
            line.request( config_o, state );
            LOG_INFO(Lgr) << "GPIO " << pnum << " exported, direction OUTPUT"
                " Initially " << state;
        } else {
            gpiod::line_request config_i
               {AppName,gpiod::line_request::DIRECTION_INPUT,0};
            line.request( config_i );
            LOG_INFO(Lgr) << "GPIO " << pnum << " exported, direction INPUT";
        }
        return true;
    }
    catch ( std::exception &ex ) {
        LOG_ERROR(Lgr) << "GPIO " << pnum << " setup failed: " << ex.what();
        m_gpio_errors++;
    }
    return false;
}


/// Release GPIO pins
///
void Cooling::teardown_gpio()
{
    // NOTE: release is now automatic when the gpiod "chip" goes out of scope
    // if (m_fan_gpio) { m_fan_gpio->release(); }
    // if (m_red_gpio) { m_red_gpio->release(); }
    // if (m_grn_gpio) { m_grn_gpio->release(); }
    // if (m_pbutton_gpio) { m_pbutton_gpio->release(); }
}

/// return cpu temp in degrees C
///
double Cooling::get_cpu_temp()
{
  double ck = (-1000.0);
  fs::ifstream tztempf(m_temp_sensor_path,std::ifstream::in);
  if (tztempf.good()) {
    tztempf >> ck;
    tztempf.close();
    m_degc = (ck/1000.0);
    return m_degc;
  }
  tztempf.close();
  LOG_ERROR(Lgr) << "Failed to read temperature";
  return m_degc;                 // last temp
}


/// Turn the fan on or off based on cpu temperature.
///
void Cooling::control_temp()
{
    double cpu_temp = get_cpu_temp();

    if ((cpu_temp > m_cool_start_temp) && ! m_fan_running) {
        start_fan();
    }
    else if ((cpu_temp < m_cool_stop_temp) && m_fan_running) {
        stop_fan();
    }
}

//////////////////////////// child process ///////////////////////////////

/// If rsked is enabled, attempt to start rsked -- up to
/// Max_rsked_restarts times. If this fails, then mark rsked as
/// broken and turn on red LED and return false. Retry again after
/// Rsked_restart_cooldown_secs.  If rsked is disabled, then terminate
/// any running copy and return false;
///
bool Cooling::start_rsked()
{
    if (not m_rsked_enabled) {
        terminate_rsked();
        return false;
    }
    if (m_rsked_broken) {
        time_t tnow = time(0);
        if ((tnow - m_last_failed_start) < Rsked_restart_cooldown_secs) {
            return false;
        }
        m_rsked_broken = false; // suppose it is working again...
        m_last_failed_start = 0;
        LOG_INFO(Lgr) << "Time has passed, try restarting rsked again...";
        illuminate_red(false);
    }
    for (int i=Max_rsked_restarts; i>0; --i) {
        if (start_rsked_once()) return true;
    }
    mark_rsked_broken(time(0));
    LOG_ERROR(Lgr) << "Giving up restarts for a while--check rsked config";
    //
    return false;
}

/// This will kill any rsked owned by this process or any rogue copy
/// that can be identified by its pid file.  Shared memory is released.
/// Any auxiliary processes will also be killed.
///
void Cooling::terminate_rsked()
{
    // kill any rsked child process of this application
    if (m_rsked_cm->running()) {
        LOG_INFO(Lgr) << "Terminate running rsked and related processes";
        m_rsked_cm->kill_child();      // should not need to force
    }
    // kill any rogue rsked -- unlikely but not impossible
    pid_t r_pid = is_running("rsked");
    if (r_pid) {
        int rc = kill(r_pid, SIGTERM);
        if (0==rc) {
            LOG_WARNING(Lgr) << "signalled a rogue rsked to terminate";
        } else {
            // possibly died by itself before we could kill it
            LOG_ERROR(Lgr) << "failed to kill rsked pid=" << r_pid;
        }
    }
    // clobber any rogue players
    kill_aux_processes();

    // release shared memory
    teardown_shm();
}


/// Start the child rsked. Will kill any rsked already running, as
/// identified by its pid file.
///
/// \returns true if rsked started normally or was already running,
/// but returns false if:
/// - there is another rsked running that we cannot kill
/// - rsked fails to start
/// - rsked starts but dies quickly
///
bool Cooling::start_rsked_once()
{
    if (m_rsked_cm->running()) {
        LOG_WARNING(Lgr) << "rsked already running";
        return true;
    }
    terminate_rsked();
    //
    // Start child rsked with given shared memory key and config file.
    // Turn on debug if m_rsked_debug has been set to true.
    m_sh_token = ftok( m_rsked_bin_path.c_str(), 1 );
    LOG_DEBUG(Lgr) << "Shared memory token: " << m_sh_token;
    m_rsked_cm->set_binary( m_rsked_bin_path );
    m_rsked_cm->clear_args();
    if (m_rsked_debug) {
        m_rsked_cm->add_arg("--debug");
    }
    m_rsked_cm->add_arg("--shmkey");
    m_rsked_cm->add_arg( std::to_string(m_sh_token) );
    m_rsked_cm->add_arg("--config");
    m_rsked_cm->add_arg(m_rsked_cfg_path.native());
    try {
        m_rsked_cm->start_child();
        sleep( Wait_for_rsked_start_secs );
    } catch(...) {
        LOG_ERROR(Lgr) << "Exception: could not start application rsked";
        return false;
    }
    RunCond status { RunCond::okay };
    if (not m_rsked_cm->check_child(status)) {
        LOG_ERROR(Lgr) << "child rsked did not start normally: "
                       << Child_mgr::cond_name(status);
        return false;
    }
    LOG_INFO(Lgr) << "started child process rsked";
    setup_shmem();
    return true;
}

/// Mark rsked as broken; turn on the red light.
///
void Cooling::mark_rsked_broken(time_t tx)
{
    m_rsked_broken = true;
    m_last_failed_start = tx;
    m_last_rsked_crash = tx;
    illuminate_red(true);
    terminate_rsked();
    m_rsked_cm->clear_status(); // however, will remain in cmd_phase "running"
    LOG_ERROR(Lgr) << "Rsked is being marked as broken--no restarts for a while";
}

/// Verify that rsked (if enabled) is in the correct run state: running.
/// If rsked is gone, attempt to restart; restart might be
/// suppressed if attempts have failed recently.
///
void Cooling::check_rsked()
{
    RunCond status { RunCond::okay };

    if (not m_rsked_enabled) {
        return;
    }
    if (m_rsked_cm->check_child( status )) {
        return;
    }
    switch (m_rsked_cm->last_obs_phase()) {
    case ChildPhase::unknown:
    case ChildPhase::running:
        break;                  // (this should not occur)
    case ChildPhase::gone:
        maybe_restart_rsked();
        break;
    case ChildPhase::paused:
        LOG_ERROR(Lgr) << "rsked was suspended! Continuing...";
        m_rsked_cm->cont_child();
        break;
    }
}

/// Rsked died at some point. If it was already marked as broken, see if
/// it is eligible to be restarted--if not, then just return.  If it recently
/// had crashed, then mark it as broken and do not attempt to restart now.
///
void Cooling::maybe_restart_rsked()
{
    time_t tnow=time(0);
    if (m_rsked_broken) {       // was already marked as broken
        if ((tnow - m_last_rsked_crash) < Rsked_restart_cooldown_secs)  {
            return;            // it is too soon to attempt a restart
        }
    } else {                    // not yet marked as broken--possibly mark it so
        LOG_ERROR(Lgr) << "rsked seems to be dead";
        ++m_rsked_errors;
        if ((tnow - m_last_rsked_crash) < Min_intercrash_secs) {
            mark_rsked_broken(tnow);
            return;
        }
        m_last_rsked_crash = tnow;
    }
    start_rsked();              // attempt restart
}


////////////////////////////// shared memory ///////////////////////////////


/// Get the status word. This returns 0 if status info is not available.
/// Otherwise it will be one of RSK_INITIALIZING RSK_PLAYING, RSK_PAUSED
///
uint32_t Cooling::get_status()
{
    if (m_shm_word != nullptr) {
        return *m_shm_word;
    }
    return 0;
}

/// Green Off : OFF
/// Green Solid : rsked playing
/// Green Flashing : paused
///
/// Red Solid : rsked not running
/// Red Flashing : rsked error
///
void Cooling::update_leds()
{
    namespace pt = boost::posix_time;
    if (m_rsked_enabled) {
        switch (get_status())
        {
        case RSK_OFF:
            illuminate_grn(false);
            break;
        case RSK_PLAYING:
            illuminate_grn(true);
            break;
        case RSK_PAUSED:
        {
            pt::ptime tt(pt::microsec_clock::universal_time());
            pt::time_duration dt = (tt - m_last_blink);
            if (dt >= pt::seconds(2)) {
                toggle_grn();
                m_last_blink = tt;
            }
        }
        break;
        default:
            break;
        }
    }
    illuminate_red(not m_rsked_cm->running());
}


/// We share some memory with rsked: a status word. Attempt to set
/// this up, assuming rsked is running and has created the shared memory
/// with a key passed by cooling to rsked in its command line argument.
///
bool Cooling::setup_shmem()
{
    m_shm_id = shmget( m_sh_token, sizeof(uint32_t), 0 );
    if (m_shm_id==(-1)) {
        LOG_ERROR(Lgr) << "Failed to get shared memory: " << strerror(errno);
        return false;
    } else {
        errno=0;
        m_shm_word = (uint32_t*) shmat(m_shm_id,NULL,0); // RW
        if ((void*)(-1)==m_shm_word) {
            LOG_ERROR(Lgr) << "Failed to attach shared memory: "
                          << strerror(errno);
            m_shm_word = nullptr;
        } else {
            LOG_INFO(Lgr) << "Shared memory attached, value=" << *m_shm_word;
        }
    }
    illuminate_grn(true);
    return true;
}

/// Detach the shared memory, if any.
///
void Cooling::teardown_shm()
{
    if (m_shm_word) {
        shmdt((const void*) m_shm_word);
        m_shm_word = nullptr;
    }
}


////////////////////////////// Run Loop ///////////////////////////////


/// Loop until dismissed by SIGTERM, monitoring the temperature and
/// child program. Only one instance should run on a host
///
int Cooling::run()
{
    unsigned polls=0;
    if (is_running(AppName)) {
        throw Already_running_exception();
    }
    mark_running(AppName);
    setup_sigterm_handler();
    if (m_rsked_enabled) { start_rsked(); }
    for (;;) {
        nanosleep(&m_poll_timespec,nullptr);  // snooze; n.b. wakes on signal
        if (g_Terminate) { break; }
        if (g_ReloadReq) {
            reload_config();
            g_ReloadReq=false;
            continue;
        }
        if (m_rsked_enabled) { check_rsked(); }
        update_leds();
        check_buttons();
        control_temp();
        if (0==polls++) {       // trace
            LOG_INFO(Lgr) << "rsked " << rsk_modename( get_status())
                          << ", temperature " << m_degc << " C";
        }
        if (polls >= m_poll_trace) { polls=0; }
        log_banner(false);
    }
    LOG_INFO(Lgr) << AppName << " exits via signal " << g_TermSignal;
    mark_ended(AppName);
    return 0;
}
