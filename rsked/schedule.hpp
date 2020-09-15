#pragma once

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

#include <time.h>
#include <vector>
#include <map>
#include <array>
#include <stdexcept>
#include <iomanip>
#include <boost/filesystem.hpp>

#include "logging.hpp"
#include "source.hpp"
#include "radio.hpp"

/// Days of the week per tm_wday
enum Day { Sun=0, Mon, Tue, Wed, Thu, Fri, Sat, DaysPerWeek };
extern const char* DayNames[];
int daynameToIndex( const std::string& );

class Schedule;

/**
 * Indicates the source starting at a particular time of day
 * expressed as a non-negative offset in seconds from midnight.
 */
class Play_slot {
private:
    unsigned m_start_day_sec {0}; // seconds from 00:00
    std::string m_name {"OFF"};   // name of the primary source
    spSource m_source {};         // shared pointer to actual source
    bool m_announce { false };    // if true, this is an announcement
    int m_complete { -1 };        // played completed this day of year
    bool m_valid { true };
public:
    //
    void clear();
    void describe() const;
    int get_complete() const { return m_complete; }
    bool is_announcement() const { return m_announce; }
    bool is_complete(int) const;
    bool is_complete() const; // today
    bool is_compann(int y) const {
        return is_announcement() and is_complete(y); }
    void resolve_source( Schedule &);
    void set_complete(int);
    void set_complete();  // today
    spSource source() const { return m_source; }
    const std::string& name() const { return m_name; }
    unsigned start_day_sec() const { return m_start_day_sec; }
    bool valid() const { return m_valid; }
    //
    Play_slot() {}
    Play_slot( const Json::Value& );
};

using spPlay_slot = std::shared_ptr<Play_slot>;

/**
 * A Day Program is a vector of shared pointers to Play_slots. For
 * example, the Day_progam named "weekday" might have 8 slots spanning
 * midnight to midnight next day.
 */
struct Day_program {
    std::string name {};
    std::vector<spPlay_slot> m_slots {};
};


/**
 * Map from time to item to play. This object reflects the structure of
 * the json schedule file.
 *  m_programs:  day_index_int -> Day_program
 *  m_sources:   source name -> spSource
 */
class Schedule {
private:
    bool m_valid {false};
    bool m_debug {false};
    std::string m_version {};
    std::array<Day_program,7> m_programs {};
    std::map<std::string,spSource> m_sources {};
    boost::filesystem::path m_fname {};
    std::shared_ptr<ResPathSpec> m_rps;
    //
    bool has_source( const std::string& );
    void load_a_dayprogram( const std::string &, const Json::Value &);
    void load_dayprograms(Json::Value&);
    void load_rps(Json::Value&);
    void load_sources(Json::Value&);
    spPlay_slot make_slot( unsigned, const Json::Value&, unsigned);
    unsigned tm_to_day_sec( const struct tm* ) const;

public:
    void debug(bool p) { m_debug = p; }
    spSource find_viable_source( const std::string& );
    void load( const boost::filesystem::path& );
    spPlay_slot play_daytime(const struct tm*);
    spPlay_slot play_now();
    bool valid() const { return m_valid; }
    //
    Schedule();
};

