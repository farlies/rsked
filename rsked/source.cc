/**
 * Methods for the Source class
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

#include <jsoncpp/json/json.h>
#include "source.hpp"
#include "logging.hpp"
#include "configutil.hpp"

namespace fs = boost::filesystem;

//////////////////////////// Utility ////////////////////////////////////

/// String canonically naming medium. Return unknown if the medium
/// has not been registered here.
/// * Will not throw
///
const char* media_name( const Medium m )
{
    switch(m) {
    case Medium::off:  return "off";
    case Medium::radio: return "radio";
    case Medium::stream: return "stream";
    case Medium::file: return "file";
    case Medium::directory: return "directory";
    case Medium::playlist: return "playlist";
    default: return "unknown";
    };
}

/// Name an encoding as a string, returning "unknown" if
/// for some reason the argument has not been registered here.
/// * Will not throw
///
const char* encoding_name( const Encoding enc )
{
    switch(enc) {
    case Encoding::none: return "none";
    case Encoding::ogg: return "ogg";
    case Encoding::mp3: return "mp3";
    case Encoding::mp4: return "mp4";
    case Encoding::flac: return "flac";
    case Encoding::wfm: return "wfm";
    case Encoding::nfm: return "nfm";
    case Encoding::mixed: return "mixed";
    default:
        return "unknown";
    }
}


/// Convert a medium string to a mode enum. This is case *sensitive*.
/// * May throw Schedule_error
///
Medium strtomedium( const std::string &s )
{
    if (s == "off") {
        return Medium::off;
    } else if (s == "radio") {
        return Medium::radio;
    } else if (s == "stream") {
        return Medium::stream;
    } else if (s == "file") {
        return Medium::file;
    } else if (s == "directory") {
        return Medium::directory;
    } else if (s == "playlist") {
        return Medium::playlist;
    }
    LOG_ERROR(Lgr) << "Unknown medium '" << s << "'";
    throw Schedule_error();
}

/// Convert an encoding string to an Encoding enum. This is case *sensitive*.
/// * May throw Schedule_error
///
Encoding strtoencoding( const std::string &s )
{
    if (s == "none") {
        return Encoding::none;
    } else if (s == "ogg") {
        return Encoding::ogg;
    } else if (s == "mp3") {
        return Encoding::mp3;
    } else if (s == "mp4") {
        return Encoding::mp4;
    } else if (s == "flac") {
        return Encoding::flac;
    } else if (s == "wfm") {
        return Encoding::wfm;
    } else if (s == "nfm") {
        return Encoding::nfm;
    } else if (s == "mixed") {
        return Encoding::mixed;
    }
    LOG_ERROR(Lgr) << "Unknown encoding '" << s << "'";
    throw Schedule_error();
}

/// Retrieve a boolean from a Json Value, with a given default
/// * May throw some JSON exception, probably
///
static bool get_bool_option( const Json::Value &bval, bool dflt )
{
    if (bval.isNull()) {
        return dflt;
    }
    return bval.asBool();
}


//////////////////////////// Source ////////////////////////////////////

/// CTOR for Source
///  special instance named OFF_SOURCE is always quiet
///
Source::Source( const std::string &nom )
    : m_name(nom)
{
    if (OFF_SOURCE == nom) {
        m_quiet_okay = true;
    }
}



/// Is the source on the local disk.  >>UNUSED<<
///
bool Source::localp() const
{
     return ((m_medium == Medium::file)
             or (m_medium == Medium::directory)
             or (m_medium == Medium::playlist));
}


/// If argument is true, the Source failed to play for some reason.
/// Method notes the current time and sets the failedp flag to true.
/// If the argument is false, *clear* the failedp flag if it had been
/// set.
///
void Source::mark_failed(bool fp)
{
    if (fp) {
        m_failedp = true;
        m_last_fail = time(0);
        LOG_WARNING(Lgr) << "Source {" << m_name
                         << "} being marked as faulty";
    } else {
        if (m_failedp) {
            LOG_WARNING(Lgr) << "Faulty flag cleared for Source {" << m_name
                          << "}";
            m_failedp = false;
        }
    }
}

/// A source is viable if it is not failed, and any resource it needs
/// is currently locatable.  Moreover, if it was marked failed more
/// than m_src_retry_secs ago its failure mark is cleared.
///
bool Source::viable()
{
    if (m_failedp) {
        time_t dt = (time(0) - last_fail());
        if (dt > m_src_retry_secs) {
            LOG_INFO(Lgr) << "Schedule: time has passed...retry source {"
                          << m_name << "}";
            mark_failed(false);  // give it another chance
        }
    }
    if (m_failedp) return false;
    // resource pre-check (currently only for local files)
    if (localp()) {
        boost::filesystem::path eff_path;
        if (not res_path(eff_path)) {
            return false;
        }
    }
    //
    return true;
}


/// Describe the source in the log on a single line.
///
void Source::describe() const
{
    std::string ftime {""};
    if (m_failedp) {
        ftime = ctime(&m_last_fail);
        ftime.erase( ftime.end()-1, ftime.end() );
    }
    if (m_medium==Medium::radio) {     // Frequency
        LOG_INFO(Lgr)  << "Source " << m_name
                       << ": medium=" << media_name(m_medium)
                       << ", encoding=" << encoding_name(m_encoding)
                       << ", freq=" << static_cast<double>(m_freq_hz)/1000000.0
                       << ", alt='" << m_alternate
                       << "', ann=" << (m_announcementp ? 'y' : 'n')
                       << ", failed=" << (m_failedp ? "y @ " : "n")
                       << ftime;
    }
    else if (m_medium==Medium::stream) { // URLs
        LOG_INFO(Lgr)  << "Source " << m_name
                       << ": medium=" << media_name(m_medium)
                       << ", encoding=" << encoding_name(m_encoding)
                       << ", url='" << m_resource
                       << "', alt='" << m_alternate
                       << "', repeat=" << (m_repeatp ? 'y' : 'n')
                       << ", dynamic=" << (m_dynamic ? 'y' : 'n')
                       << ", ann=" << (m_announcementp ? 'y' : 'n')
                       << ", failed=" << (m_failedp ? "y @ " : "n")
                       << ftime;
    }
    else {
        LOG_INFO(Lgr)  << "Source " << m_name
                       << ": medium=" << media_name(m_medium)
                       << ", encoding=" << encoding_name(m_encoding)
                       << ", path=\"" << m_resource   /* relative path */
                       << "\", alt='" << m_alternate
                       << "', repeat=" << (m_repeatp ? 'y' : 'n')
                       << ", dur=" << m_duration
                       << ", dynamic=" << (m_dynamic ? 'y' : 'n')
                       << ", ann=" << (m_announcementp ? 'y' : 'n')
                       << ", failed=" << (m_failedp ? "y @ " : "n")
                       << ftime;
    }
}

/// Resets all the fields of Source to default values.
///
void Source::clear()
{
    m_name.clear();
    m_alternate = OFF_SOURCE;
    m_failedp = false;
    m_last_fail = 0;
    m_src_retry_secs = 60*60;
    //
    m_medium = Medium::off;
    m_encoding = Encoding::none;
    m_duration = 0;
    m_announcementp = false;
    m_text.clear();
    m_quiet_okay = true;
    m_repeatp = false;
    m_dynamic = false;
    m_freq_hz = 0;
    m_last_fail = 0;
    m_resource.clear();
    m_res_path.clear();
}

/// Return resource string:  a pathname or a URL.
///
const std::string& Source::resource() const
{
    // any adjustments needed here...?
    return m_resource;
}


/// Expand any strftime symbols in the path and return in dst.
/// Returns true on success, false on failure.
/// * will NOT throw
///
bool uri_expand_time( const std::string &src, std::string &dst )
{
    time_t rawtime;
    time(&rawtime);
    tm *timeinfo = localtime(&rawtime);
    char timebuf[320] = {0};
    if (strftime(timebuf,sizeof(timebuf)-1, src.c_str(), timeinfo)) {
        dst = std::string(timebuf);
        return true;
    } else {
        dst = src;
        LOG_WARNING(Lgr) << "failed to expand time in string--too long";
        return false;
    }
}

/// Set argument ref to the fully expanded pathname.
/// If dynamic we must expand any strftime symbols in the path
/// Return true if this path exists, false otherwise.  No throw.
///
bool Source::res_path(boost::filesystem::path &eff_path)
{
    if (not m_dynamic) {
        eff_path = m_res_path;  // '~' was already expanded to $HOME
    } else {
        time_t rawtime;
        time(&rawtime);
        tm *timeinfo = localtime(&rawtime);
        char timebuf[256] = {0};
        if (strftime(timebuf,sizeof(timebuf)-1, m_res_path.c_str(), timeinfo)) {
            eff_path = boost::filesystem::path(timebuf);
        }
    }
    bool pexists = exists(eff_path);
    if (not pexists) {
        LOG_WARNING(Lgr) << eff_path << " is not found";
    }
    return pexists;
}

/// Given m_medium has been established, location is one of:
/// - stream:  url,
/// - file:
///   - relative pathname for a file in the 'resource/' or 'motd/' directory
///   - relative pathname for a file in the Music directory,
/// - directory:
///   - relative pathname for an album in the Music directory,
/// - playlist: pathname for a playlist in the playlists directory,
///
void Source::extract_local_resource( const Json::Value &slot )
{
    const Json::Value &loc = slot["location"];

    if (loc.isNull()) {
        LOG_ERROR(Lgr) << "null location for source '" << m_name << "'";
        throw Schedule_error();
    }
    m_resource = loc.asString();
    m_res_path.clear();  // to be set after call to validate()
}

/// Compute, check, and record in m_res_path member the full pathname
/// of a host local resource. (Not applicable to radio or streams.)
/// The resource is generally a relative path. A relative path is
/// combined with one of the directory arguments:
///   - lib_path : the music library directory
///   - ann_path : the announcements directory
///   - pl_path : the playlist directory
///
/// Checks for existence of a file, directory, or playlist resource *if*
/// that is possible. Note that dynamic sources have pathnames might not
/// exist on any given day.  If a directory, it must have a readable file.
///
/// * Throws Schedule_error if it finds a provably invalid source.
///
void Source::validate(const ResPathSpec &rps ) // rpspec
{
    switch (m_medium) {
    case Medium::off:
    case Medium::radio:
    case Medium::stream:
        return;
    case Medium::file:
    case Medium::directory:
        if (m_announcementp) {
            rps.resolve_announcement(m_resource, m_res_path);
        } else {
            rps.resolve_library(m_resource, m_res_path);
        }
        break;
    case Medium::playlist:
        rps.resolve_playlist(m_resource, m_res_path);
        break;
    };
    try {
        if (not m_res_path.empty() and not m_dynamic) {
            verify_readable( m_res_path );
        }
    }
    catch( std::exception &ex ) {
        LOG_ERROR(Lgr) << "Source '" << m_name << "': " << ex.what();
        throw Schedule_error();
    }
}

/// Read: encoding, medium, location.  All must be present and valid.
///
void Source::extract_required_props( const Json::Value &slot )
{
    const Json::Value &medstr = slot["medium"];
    if (medstr.isNull()) {
        LOG_ERROR(Lgr) << "Source {" << m_name << "} does not specify medium";
        throw Schedule_error();
    }
    m_medium = strtomedium( medstr.asString() );
    //
    const Json::Value &encstr = slot["encoding"];
    if (encstr.isNull()) {
        LOG_ERROR(Lgr) << "Source {" << m_name << "} does not specify encoding";
        throw Schedule_error();
    }
    m_encoding = strtoencoding( encstr.asString() );
    //
    const Json::Value &lstr = slot["location"];
    if (lstr.isNull()) {
        LOG_ERROR(Lgr) << "Source {" << m_name << "} is missing a location";
        throw Schedule_error();
    }
    if (Medium::radio == m_medium) {
        constexpr const unsigned HZ_PER_MHZ = 1'000'000;
        double mhz = lstr.asDouble();
        if ((mhz < MinRadioFreqMHz) or (mhz > MaxRadioFreqMHz)) {
            LOG_ERROR(Lgr) << "Source {" << m_name
                           << "} invalid frequency: " << mhz;
            throw Schedule_error();
        }
        m_freq_hz = unsigned(HZ_PER_MHZ * mhz);
    }
    else if (Medium::stream == m_medium) {
        m_resource = lstr.asString();
    }
    else if ((Medium::file == m_medium) or (Medium::playlist == m_medium)
             or (Medium::directory == m_medium)) {
        extract_local_resource(slot);
    }
}

/// Load Source from JSON value.  May throw a Schedule_error.
/// This checks syntax, but does not do resource validation.
/// *  May throw Schedule_error
///
void Source::load( const Json::Value &slot )
{
    m_repeatp = get_bool_option( slot["repeat"], false );
    m_dynamic = get_bool_option( slot["dynamic"], false );
    m_announcementp = get_bool_option( slot["announcement"], false );
    //
    extract_required_props(slot);
    // A local recording that does *not* repeat might be silent for a
    // while if play completes before the next program is scheduled to
    // start. We force the m_quiet_okay flag to true in this case.
    //
    m_quiet_okay = get_bool_option( slot["quiet"], false );
    if (((Medium::file == m_medium) or (Medium::directory == m_medium)
         or (Medium::playlist == m_medium))
        and not m_repeatp) {
        m_quiet_okay = true;
    }
    // alternate - we cannot check these until all sources are loaded
    // The implicit default alternate source is OFF.
    const Json::Value &astr = slot["alternate"];
    if (!astr.isNull()) {
        m_alternate = astr.asString();
    } else {
        m_alternate = OFF_SOURCE;
    }
    // duration
    const Json::Value &durval = slot["duration"];
    if (! durval.isNull()) {
        m_duration = durval.asDouble();
    }

}
