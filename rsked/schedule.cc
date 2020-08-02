/**
 * Methods to load a JSON schedule and perform computations on it.
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
#include <fstream>
#include <jsoncpp/json/json.h>
#include <initializer_list>

// apt install libjsoncpp-dev

#include <time.h>

#include "schedule.hpp"
#include "configutil.hpp"


////////////////////////////////////////////////////////////////////

const char* DayNames[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};



//////////////////////////// Utility ////////////////////////////////////

size_t strnlen(const char*, size_t);

#include <ctype.h>
#define IS_DIGIT(_p,_n) if (!isdigit(_p[_n])) {s=(-1); break;}
#define IS_COLON(_p,_n) if (':' != _p[_n]) {s=(-1); break;}
#define IS_BELOW(_i,_n) if (_i >= _n) {s=(-1); break;}

/// Return the number of seconds since midnight represented by string.
/// Acceptable formats:  "01:34:67", "18:33", "08"
/// Throws Schedule_error if c is a bad string.
///
unsigned day_secs( const char*c )
{
    int s = 0, j;
    if (!c) {
        throw Schedule_error();
    }
    size_t n = strnlen(c, 10);
    switch (n) {
    case 8:
        IS_COLON(c,5);
        IS_DIGIT(c,6);
        IS_DIGIT(c,7);
        j = atoi(c+6);   /* sec */
        IS_BELOW(j,60);
        s += j;
        [[fallthrough]];
    case 5:
        IS_COLON(c,2);
        IS_DIGIT(c,3);
        IS_DIGIT(c,4);
        j = atoi(c+3);
        IS_BELOW(j,60);
        s += 60*j;      /* min */
        [[fallthrough]];
    case 2:
        IS_DIGIT(c,0);
        IS_DIGIT(c,1);
        j = atoi(c);
        IS_BELOW(j,24);
        s += 3600*j;    /* hrs */
        break;
    default:
        s = (-1);
    }
    if (s < 0) {
        throw Schedule_error();
    }
    return static_cast<unsigned>(s);
}

//////////////////////////// Play_slot /////////////////////////////////


/// Construct Play_slot from a Json value. Will not throw, but an invalid
/// json will set flag valid to false.
///
Play_slot::Play_slot( const Json::Value &slot )
{
    const std::string stime = slot["start"].asString();
    m_valid = true;
    m_complete = (-1);
    // Interpret time string as seconds into day if possible
    try {
        m_start_day_sec = day_secs( stime.c_str() );
    }
    catch(Schedule_error&) {
        LOG_ERROR(Lgr) << "Invalid time string in schedule: " << stime;
        m_valid = false;
    }
    const Json::Value &pname = slot["program"];
    const Json::Value &aname = slot["announce"];
    if (!aname.isNull() and pname.isNull()) {
        m_name = aname.asString();
        m_announce = true;
    } else if (aname.isNull() and !pname.isNull()) {
        m_name = pname.asString();
        m_announce = false;
    } else {
        LOG_ERROR(Lgr) << "Play slot must specify one of 'program' or 'announce'";
        m_valid = false;
        m_name.clear();
    }
}


/// Empty this play slot (used by ?) TODO: delete if not used!
///
void Play_slot::clear()
{
    m_start_day_sec=0;
    m_name = OFF_SOURCE;
    m_complete = (-1);
}

/// Mark the slot as played on the given day of year (0..365).
///
void Play_slot::set_complete(int doy)
{
    m_complete = doy;
}

/// Mark slot as played today (todays day of year).
///
void Play_slot::set_complete()
{
    struct tm now_tm;
    time_t now = time(nullptr);
    localtime_r( &now, &now_tm );
    m_complete = now_tm.tm_yday;
    LOG_DEBUG(Lgr) << "Setting play slot " << m_name
                   << "(" <<  this << ")"
                   << " complete for day " << m_complete;
}

/// Has the slot been played on the given day of this year?
///
bool Play_slot::is_complete(int doy) const
{
    return (m_complete == doy);
}


/// Has the slot been played today?
///
bool Play_slot::is_complete() const
{
    struct tm now_tm;
    time_t now = time(nullptr);
    localtime_r( &now, &now_tm );
    return (m_complete == now_tm.tm_yday);
}

/// Set the m_source pointer to the currently named source.  This
/// might resolve to the OFF source if the named source is not viable
/// and no alternate is viable. This is called whenever the Schedule
/// is queried for the current play slot. The source() accessor will
/// return the discovered Source.
///
void Play_slot::resolve_source( Schedule &schedule )
{
    m_source = schedule.find_viable_source( m_name );
}

/// One line description of a play_slot to the log.
///
void Play_slot::describe() const
{
    unsigned ss = m_start_day_sec;
    unsigned hh = (ss / 3600);
    ss -=  3600*hh;
    unsigned mm = (ss / 60);
    ss -=  60*mm;
    LOG_INFO(Lgr) << "Play_slot start="
                  << std::setfill('0') << std::setw(2) << hh << ":"
                  << std::setfill('0') << std::setw(2) << mm << ":"
                  << std::setfill('0') << std::setw(2) << ss
                  << ", source=" << m_name
                  << ", announce=" << m_announce
                  << ", complete=" << m_complete;
}


//////////////////////////// Schedule ///////////////////////////////////

/// CTOR
///
Schedule::Schedule()
{
}

/// Read the sources into m_sources map. Note that duplicate keys are
/// allowed in JSON, and cannot be trapped here, so be careful that a
/// source is defined only once!  Do some basic checking of fields.
/// * May throw: Schedule_error
///
void Schedule::load_sources(Json::Value &root)
{
    const Json::Value jsrcs = root["sources"];

    // add an OFF source that implements off-mode
    m_sources[OFF_SOURCE] = std::make_shared<Source>( OFF_SOURCE );
    if (m_debug) {
        m_sources[OFF_SOURCE]->describe();
    }

    // For each key/val in sources, create a Source and add to map
    for (const std::string &name : jsrcs.getMemberNames()) {
        m_sources[name] = std::make_shared<Source>(name);
        m_sources[name]->load( jsrcs[name] );
        if (m_debug) {
            m_sources[name]->describe();
        }
    }
    // Check that the alternates defined by each source are actually
    // defined in the source map.
    for (auto&& [sname,sp] : m_sources) {
        if (not has_source( sp->alternate()) ) {
            LOG_ERROR(Lgr) << "Schedule: Alternate for source '" << sname
                           << "', '" << sp->alternate() << "', "
                              "has not been defined.";
            throw Schedule_error();
        }
    }
}


/// Load the weekmap, verify each generic day program mentioned therein.
///
/// * May throw Schedule_error
///
void Schedule::load_weekmap(Json::Value& root)
{
    // Inspect the weekmap
    const Json::Value jwkmap = root["weekmap"];
    if (not jwkmap.isArray() or jwkmap.size() != 7) {
        LOG_ERROR(Lgr) << "Error in schedule " << m_fname
                  << ": Weekmap does not specify 7 day array.";
        throw Schedule_error();
    }
    //
    for (unsigned d=Sun; d<=Sat; d++) {
        const std::string pname = jwkmap[d].asString();
        if (m_debug) {
            LOG_INFO(Lgr) << DayNames[d] << ": " << pname;
        }
        m_weekmap[d] = pname;
        if (!has_day_program(pname)) {
            LOG_ERROR(Lgr) << "Error in schedule " << m_fname
                      << ": Missing day program named '" << pname 
                      << "'";
            throw Schedule_error();
        }
    }
}    

/// Read all of the dayprograms from the given root into schedule
/// member map m_programs.
///
/// * May throw Schedule_error
///
void Schedule::load_dayprograms(Json::Value &root)
{
    const Json::Value dprogs = root[ "dayprograms" ];
    if (dprogs.isNull()){
        LOG_ERROR(Lgr) << "Error in schedule " << m_fname
                       << ": No dayprograms specified!";
        throw Schedule_error();
    }
    if (not dprogs.isObject()) {
        LOG_ERROR(Lgr) << "Error in schedule " << m_fname
                       << ": dayprograms is not an object";
        throw Schedule_error();
    }
    // load all named day programs--the members of the object
    for (auto it=dprogs.begin(); it!=dprogs.end(); it++) {
        load_a_dayprogram( it.name(), *it );
    }
}

/// Schedule reader.  Check the valid() status of the schedule after
/// calling to determine if a valid schedule was loaded.
///
/// * May throw Schedule_error
///
void Schedule::load(const boost::filesystem::path &fname)
{
    Json::Value root;
    try {
        boost::filesystem::ifstream sfile(fname);
        sfile >> root; // maybe a Json::RuntimeError
    }
    catch (std::exception &e) {
        LOG_ERROR(Lgr) << "Error reading schedule from "
                                    << fname << ": " << e.what();
        throw Schedule_error();
    }
    std::string schema_id = root["schema"].asString();
    if (schema_id != "1.0") {
        LOG_ERROR(Lgr) << "Unsupported schedule schema " << schema_id;
        throw Schedule_error();
    }
    m_version = root["version"].asString();
    m_fname = fname;
    m_valid = false;
    load_sources(root);
    load_dayprograms(root);
    load_weekmap(root);
    LOG_INFO(Lgr) << "Valid schedule, version <" << m_version 
                  << "> loaded from " << m_fname;
    m_valid = true;
}


/// Check if we have a Day_program named pname.
///
bool Schedule::has_day_program( const std::string &pname )
{
    auto itr = m_programs.find(pname);
    return (itr != m_programs.end());
}

/// Is there a source of the given name?
///
bool Schedule::has_source( const std::string &sn )
{
    return (m_sources.count(sn) > 0);
}

/// Create a play slot from a JSON value.
/// Return a shared pointer to the slot.
/// * May throw: Schedule_error
///
spPlay_slot Schedule::make_slot(unsigned ix, const Json::Value& slot,
                                unsigned last_time)
{
    spPlay_slot ps = std::make_shared<Play_slot>( slot );
    if (not ps->valid()) {
        throw Schedule_error();
    }
    // Ensure strictly increasing and valid start times
    if (ix > 0 and last_time >= ps->start_day_sec()) {
        LOG_ERROR(Lgr) << "Program times not monotonic: "
                       << ps->start_day_sec() << " <= " << last_time;
        throw Schedule_error();
    }
    if (!has_source(ps->name())) {
        LOG_ERROR(Lgr) << "Unknown source: '" << ps->name();
        throw Schedule_error();
    }
    if (m_debug) {
        ps->describe();
    }
    return ps;
}

/// Construct a Day_program from the prog node, add it to the
/// map under name pname.
/// * May throw: Schedule_error
///
void Schedule::load_a_dayprogram( const std::string &pname,
                                 const Json::Value &jprog )
{
    Day_program dp;

    if (0 == jprog.size()) {
        LOG_ERROR(Lgr) << "Day program " << pname
                       << " must have at least one play slot";
        throw Schedule_error();
    }
    dp.name = pname;
    unsigned last_time = 0;
    // Iterate through JSON array elements of jprog adding Play_slots.
    for ( unsigned index=0; index < jprog.size(); ++index ) {
        spPlay_slot ps = make_slot( index, jprog[index], last_time );
        dp.m_slots.push_back( ps );
        last_time = ps->start_day_sec();
    }
    if (0 != dp.m_slots[0]->start_day_sec()) {
        LOG_ERROR(Lgr) << "Start time of first slot is not 00:00 on program: "
                       << dp.name;
        throw Schedule_error();
    }
    if (dp.m_slots[0]->is_announcement()) {
        LOG_ERROR(Lgr) << "First slot may not be an announcement on program: "
                       << dp.name;
        throw Schedule_error();
    }
    m_programs[pname] = dp;
    if (m_debug) {
        LOG_INFO(Lgr) << "Loaded program '" << pname
              << "' (" << jprog.size() << " slots)";
    }
}


/// Retrieve the (Source) program that should be played right now.
/// If for some reason it cannot obtain the current time, a midnight
/// start date/time will be used instead.
/// May throw std::invalid_argument (unlikely).
///
spPlay_slot Schedule::play_now()
{
    struct tm ltm;
    time_t now = time(0);
    if (now == time_t(-1)) {    // unlikely
        explicit_bzero(&ltm,sizeof(ltm));
    } else {
        localtime_r(&now,&ltm);
    }
    if (m_debug) {
        LOG_DEBUG(Lgr) << "localtime "
                  << std::setfill('0') << std::setw(2) << ltm.tm_hour
                  << ":" << std::setfill('0') << std::setw(2) << ltm.tm_min
                  << ":" << std::setfill('0') << std::setw(2) << ltm.tm_sec;
    }
    return play_daytime( &ltm );
}

/// Convert a struct tm to seconds within a day. Additionally, it will
/// vet the tm struct and throw an std::invalid_argument exception if the
/// struct is invalid.  If m_debug is true, the time is printed
///
unsigned Schedule::tm_to_day_sec( const struct tm *loc_tm ) const
{
    if (!loc_tm) {
        throw std::invalid_argument("tm_to_day_sec: null loc_tm");
    }
    int yday=loc_tm->tm_yday;
    int wday=loc_tm->tm_wday;
    int hr =loc_tm->tm_hour;
    int min = loc_tm->tm_min;
    int sec = loc_tm->tm_sec;

    if (yday < 0 || yday > 365) {
        throw std::invalid_argument("Invalid day of year");
    }
    if (wday < Sun || wday > Sat) {
        throw std::invalid_argument("Invalid day of week index");
    }
    if (hr < 0 || hr > 23) {
        throw std::invalid_argument("Invalid hour");
    }
    if (min < 0 || min > 59) {
        throw std::invalid_argument("Invalid minute");
    }
    if (m_debug) {
        LOG_DEBUG(Lgr)
           << "Select program for: " <<  DayNames[wday] << "  @ "
           << std::setfill('0') << std::setw(2) << hr
           << ":" << std::setfill('0') << std::setw(2) << min
           << ":" << std::setfill('0') << std::setw(2) << sec;
    }
    return static_cast<unsigned>(60*(60*hr + min) + sec);
}    

/// Retrieve the Source for a given weekday, hour, minute, and second
/// (defaults to 00).  If m_debug, will log the selected program.  The
/// Day_program loader guarantees that there will be at least one
/// Play_slot (the one starting at 00:00:00, which may NOT be an
/// announcement) on any given day. If given a valid schedule (one where
/// there is always a valid slot) it will always return a smart pointer
/// to a valid slot.
///
/// * May throw std::runtime_error, std::invalid_argument, Schedule_error  
///
spPlay_slot Schedule::play_daytime( const struct tm *loc_tm )
{
    unsigned sec_of_day = tm_to_day_sec(loc_tm); // can throw
    std::size_t wd = static_cast<std::size_t>(loc_tm->tm_wday);
    auto dp_itr = m_programs.find( m_weekmap[wd] );
    if (dp_itr == m_programs.end()) {
        throw std::invalid_argument("Missing program day");
    }
    int yday = loc_tm->tm_yday;
    std::vector<spPlay_slot> &the_pslots = dp_itr->second.m_slots;

    // Scan the slots until we get to a start time past the current
    // time.  The current slot will be the previous one in the list.
    // However, announcements are handled specially: If the indicated
    // slot for a time is an *announcement* and that slot is marked as
    // *completed* for today, then the nearest slot before it that is
    // not a completed announcement is selected.

    auto nps = the_pslots.size();
    if (0==nps) {
        LOG_ERROR(Lgr) << "No play slots available today!";
        throw Schedule_error();
    }
    auto u = 0U;          // the index of the to-be-selected play slot
    if (nps > 1) {        // (if there is just one slot, use it regardless)
        while (u < nps) { // otherwise search forward from slot 0
            if (sec_of_day < the_pslots[u]->start_day_sec()) {
                if (u > 0) { 
                    --u;
                } else {
                    LOG_ERROR(Lgr) << "play_daytime: no playable slots";
                    throw Schedule_error();
                }
                if ( the_pslots[u]->is_compann(yday) ) {
                    // Back up to the last non-announcement, marking as
                    // complete any earlier announcements
                    while( the_pslots[--u]->is_announcement() ) {
                        the_pslots[u]->set_complete(yday);
                    }
                }
                break;
            }
            u++;
        }
        // Past the last slot. Backup, marking all announcements.
        if (u >= nps) {
            while( u and the_pslots[--u]->is_announcement() ) {
                the_pslots[u]->set_complete(yday);
            }
        }
    }
    LOG_DEBUG(Lgr) << "Selected slot " << u << ", " << the_pslots[u]->name()
                   <<  "(" << the_pslots[u].get() << ")"
                   << " complete=" << the_pslots[u]->get_complete()
                   << ", yday=" << yday;
    //
    the_pslots[u]->resolve_source(*this);
    return the_pslots[u];
}



/// Return a shared pointer to the source given its name; if that
/// source is marked as failed, return its alternate; if the alternate
/// also failed, pursue its alternate, and so forth.  However if the
/// last failure for any source was long enough ago (Src_retry_secs),
/// then clear the failure mark so we can try again.  The source of
/// last resort is OFF_SOURCE--it cannot fail.
///
spSource
Schedule::find_viable_source( const std::string& sn )
{
    unsigned maxit=4;   // avoid circular reference chains.
    spSource src = m_sources[ sn ];
    if (nullptr==src) {
        LOG_ERROR(Lgr) << "Missing source: '" << sn <<"'";
        return m_sources[OFF_SOURCE];
    }
    bool viablep;
    while (src and maxit) {
        viablep = src->viable();
        if (viablep) break;
        src = m_sources[ src->alternate() ];
        --maxit;
    }
    if (src and viablep and maxit) {
        return src;
    }
    LOG_ERROR(Lgr) << "Schedule: Fallback to OFF mode";
    return m_sources[OFF_SOURCE];
}

///////////////////////////////////////////////////////////////////////
