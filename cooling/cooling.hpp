#pragma once

/*   Part of the rsked package.
 *   Copyright 2022 Steven A. Harp   farlies(at)gmail.com
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

/* >> You may wish to adjust some of the compile-time parameters in << */
/* >> found at the start of cooling.cc                              << */

#include <sys/shm.h>
#include <time.h>
#include <memory>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <gpiod.hpp>
#include "util/configutil.hpp"
#include "util/childmgr.hpp"

class Config;

/// In main.cc:
extern const char *AppName;

/// Timing stuff
using myclock_t = std::chrono::steady_clock;
using  timept_t = std::chrono::time_point<myclock_t>;
using    nsec_t = std::chrono::nanoseconds;
using    msec_t = std::chrono::milliseconds;
using     sec_t = std::chrono::seconds;
const auto SCZERO = myclock_t::duration::zero();



/// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/// Constants for configuring/using GPIO lines
enum class Gpio { IN, OUT };
enum { GPIO_NC=(-1), GPIO_OFF=0, GPIO_ON=1 };

/// Exception Types

struct Cooling_config_exception  : public std::exception {
    const char *what() const noexcept {
        return "Defective configuration";
    }
};

struct Already_running_exception  : public std::exception {
    const char *what() const noexcept {
        return "An instance of this application is already running.";
    }
};

struct Cooling_runtime_exception  : public std::exception {
    const char *what() const noexcept {
        return "Unexpected runtime error";
    }
};

/// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/// This application is useful when running rsked on an embedded device
/// like RPi.  It will monitor core temperature and turn on an external
/// cooler via GPIO.  It will also assume responsibility for starting
/// rsked, and will restart it if it crashes. It can accept a button
/// GPIO input and use that to signal rsked to snooze (or resume play).
/// It can use a GPIO Output to indicate that rsked is in snooze mode
/// by strobing an LED.
///
class Cooling {
private:
    // GENERAL
    bool m_test_mode {false};    // true: no run or file logging, just test
    bool m_console_log {false};  // true: log to console
    bool m_debug {false};        // true: log debug level messages
    bool m_logging_up {false};   // true: logging has been initialized
    msec_t m_poll_interval {1000}; // polling period, milliseconds
    unsigned m_poll_trace {30};  // log every poll_trace polls
    timept_t m_last_banner {SCZERO}; // last time we logged the banner
    key_t m_sh_token {0};        // shared memory token
    int m_shm_id {0};            // id of shared memory
    uint32_t *m_shm_word {nullptr}; // address of shared memory word
    std::string m_cfgversion {"?"}; // version from config file
    std::string m_cfgdesc {"?"}; // description from config file
    //
    //////// GPIO ////////
    unsigned m_gpio_errors=0;    // count GPIO failures
    std::unique_ptr<gpiod::chip> m_gpio_chip {}; // the (sole) GPIO chip
    // FAN
    bool m_fan_control_enabled {true};     // do fan control?
    gpiod::line m_fan_gpio {};             // fan gpio line
    double m_degc {0.0};         // last temp reading
    boost::filesystem::path m_temp_sensor_path
                            {"/sys/class/thermal/thermal_zone0/temp"};
    bool m_fan_running {false};      // is the fan running
    sec_t m_min_cool_secs {240};  // run fan for at least this period
    double m_cool_start_temp { 59.0 }; // deg C
    double m_cool_stop_temp { 49.0 };  // deg C
    timept_t m_last_cool_start {SCZERO};
    // LEDS
    bool m_panel_leds_enabled {false};
    gpiod::line m_red_gpio {};  // gpio line for red LED
    gpiod::line m_grn_gpio {};  // gpio line for green LED
    bool m_red_state = false;    // commanded red LED state true=ON
    bool m_grn_state = false;    // commanded grn LED state true=ON
    timept_t m_last_blink;       //
    // BUTTON
    bool m_snooze_button_enabled {false};
    gpiod::line m_pbutton_gpio {}; // gpio line for pushbutton
    timept_t m_last_pbcheck {SCZERO};   // last time we checked buttons
    timept_t m_last_pbtime {SCZERO};    // last observed button press
    int m_last_pbstate {-1};        // 0=down 1=up
    //
    /////// RSKED ///////
    bool m_rsked_enabled {true}; // run rsked? disabling still runs fan
    bool m_rsked_debug {false}; // print debug messages in rsked log?
    unsigned m_rsked_errors=0;   // count times rsked fails
    bool m_rsked_broken {false};     // does rsked appear to be unstartable?
    timept_t m_last_failed_start {SCZERO}; // when we gave up restarts
    timept_t m_last_rsked_crash {SCZERO};  // when rsked last observed dead

    std::string m_kill_pattern { "'ogg123|mpg321|gqrx'" };
    std::string m_mpd_stop_cmd { "/usr/bin/mpc stop" };
    spCM m_rsked_cm {};
    boost::filesystem::path m_cfg_path;
    boost::filesystem::path m_rsked_bin_path;
    boost::filesystem::path m_rsked_cfg_path;
    //
    void button_pressed(int);
    void check_rsked();
    bool config_gpio_pin( gpiod::line&, unsigned, Gpio, int );
    void control_temp();
    unsigned error_count() const;
    double get_cpu_temp();
    uint32_t get_status();
    void init_cooling(Config&);
    void init_panel_leds(Config&);
    void init_rsked(Config&);
    void init_snooze_button(Config&);
    void kill_aux_processes();
    void mark_rsked_broken(timept_t);
    void maybe_restart_rsked();
    void reload_config();
    bool setup_gpio();
    bool setup_shmem();
    void setup_sigterm_handler();
    void start_fan();
    bool start_rsked();
    bool start_rsked_once();
    void stop_fan();
    void stop_mpd_player();
    void terminate_rsked();
    void illuminate_red(bool);
    void illuminate_grn(bool);
    void toggle_red();
    void toggle_grn();
    void teardown_gpio();
    void teardown_shm();
    void update_leds();
    unsigned wait_button();

public:
    void initialize(bool /*debug*/);
    void log_banner(bool force=false);
    int run();
    //
    Cooling(boost::program_options::variables_map &vm);
    Cooling(Cooling const&) = delete;
    void operator=(Cooling const&) = delete;
    ~Cooling();
};
