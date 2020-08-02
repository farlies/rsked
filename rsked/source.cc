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

//////////////////////////// Utility ////////////////////////////////////

/// String canonically naming medium.
///
const char* media_name(Medium m)
{
    switch(m) {
    case Medium::off:
        return "off";
        break;
    case Medium::radio:
        return "radio";
        break;
    case Medium::mp3_stream:
        return "mp3_stream";
        break;
    case Medium::ogg_file:
        return "ogg_file";
        break;
    case Medium::mp3_file:
        return "mp3_file";
        break;
    default:
        break;
    };
    return "Unknown";
}

/// name a resource type
///
const char* restype_name( ResType rt )
{
    switch(rt) {
    case ResType::None: return "None";
    case ResType::File: return "File";
    case ResType::Frequency: return "Frequency";
    case ResType::Directory: return "Directory";
    case ResType::Playlist: return "Playlist";
    case ResType::URL: return "URL";
    default:
        return "Unknown";
    }
}


/// Convert a medium string to a mode enum. This is case sensitive.
///
Medium strtomedium( const std::string &s )
{
    if (s == "off") {
        return Medium::off;
    } else if (s == "radio") {
        return Medium::radio;
    } else if (s == "ogg") {
        return Medium::ogg_file;
    } else if ((s == "stream") or (s == "mp3_stream")) {
        return Medium::mp3_stream;
    } else if (s == "mp3") {
        return Medium::mp3_file;
    }
    LOG_ERROR(Lgr) << "Unknown mode '" << s << "'";
    throw Schedule_error();
    return Medium::off;         // never
}


/// Retrieve a boolean from a Json Value, with a given default
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
Source::Source( std::string n )
 : m_name(n)
{
    if (OFF_SOURCE == n) {
        m_quiet_okay = true;
    }
}

/// CTOR for regular files given path.
/// TODO: presumed OGG for now!
///
Source::Source( const std::string &n, const boost::filesystem::path &p )
    : m_name(n), m_res_path(p)
{
    m_medium = Medium::ogg_file;
    m_res_type = ResType::File;
    m_resource = p.filename().string();
    m_quiet_okay = true;
    m_repeatp = false;
}


/// Is the source on a local disk.
///
bool Source::localp() const
{
    return ((m_medium == Medium::mp3_file)
            or (m_medium == Medium::ogg_file));
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


/// Describe the source in the log.
///
void Source::describe() const
{
    std::string ftime {""};
    if (m_failedp) {
        ftime = ctime(&m_last_fail);
        ftime.erase( ftime.end()-1, ftime.end() );
    }
    if (m_medium==Medium::radio) {     // Frequency
        LOG_INFO(Lgr) << "Source " << m_name
                      << ": medium=" << media_name(m_medium)
                      << ", freq=" << static_cast<double>(m_freq_hz)/1000000.0
                      << ", alt='" << m_alternate
                      << "', failed=" << (m_failedp ? "y @ " : "n")
                      << ftime;
    }
    else if (m_medium==Medium::mp3_stream) { // URLs
        LOG_INFO(Lgr) << "Source " << m_name
                      << ": medium=" << media_name(m_medium)
                      << ", " << restype_name(m_res_type)
                      << "='" << m_resource
                      << "', alt='" << m_alternate
                      << "', repeat=" << (m_repeatp ? 'y' : 'n')
                      << ", dynamic=" << (m_dynamic ? 'y' : 'n')
                      << ", failed=" << (m_failedp ? "y @ " : "n")
                      << ftime;
    }
    else {
        LOG_INFO(Lgr) << "Source " << m_name   // Local Pathname
                      << ": medium=" << media_name(m_medium)
                      << ", " << restype_name(m_res_type)
                      << "=" << m_res_path
                      << ", alt='" << m_alternate
                      << "', repeat=" << (m_repeatp ? 'y' : 'n')
                      << ", dynamic=" << (m_dynamic ? 'y' : 'n')
                      << ", failed=" << (m_failedp ? "y @ " : "n")
                      << ftime;
    }
}

/// clears the fields of Source
///
void Source::clear()
{
    m_name = "";
    m_medium = Medium::off;
    m_quiet_okay = true;
    m_resource = "";
    m_freq_hz = 0;
    m_alternate = OFF_SOURCE;
    m_failedp = false;
    m_last_fail = 0;
}

/// Return resource string:  a pathname or a URL.
///
const std::string& Source::resource() const
{
    // any adjustments needed here...?
    return m_resource;
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
    LOG_INFO(Lgr) << eff_path << (pexists ? " exists" : " is not found");
    return pexists;
}


/// True iff exactly one of the boolean arguments is true.
///
static bool exactly_one( std::initializer_list<bool> list )
{
    bool one=false;
    for( bool elem : list )
    {
        if (elem) {
            if (one) return false;
            one = true;
        }
    }
    return one;
}

/// An ogg or mp3 player expects exactly one of these keywords:
///   "file"
///   "directory"
///   "playlist"   (always along with "path")
///   "url"
///
void Source::extract_local_resource( const Json::Value &slot )
{
    const Json::Value &fstr = slot["file"];
    const Json::Value &dstr = slot["directory"];
    const Json::Value &pstr = slot["playlist"];
    const Json::Value &ustr = slot["url"];
    if (not exactly_one({ !fstr.isNull(), !dstr.isNull(),
                          !pstr.isNull(), !ustr.isNull()})) {
        LOG_ERROR(Lgr) << "Source {" << m_name
        << "} needs exactly ONE of the keys: file, directory, playlist, url";
        throw Schedule_error();
    }

    if (!fstr.isNull()) {       // file
        m_resource = fstr.asString();
        m_res_path = expand_home( m_resource );
        m_res_type = ResType::File;
        return;
    }
    if (!dstr.isNull()) {       // directory
        m_resource = dstr.asString();
        m_res_path = expand_home( m_resource );
        m_res_type = ResType::Directory;
        return;
    }
    // playlist (named playlist in MPD)
    if (!pstr.isNull()) {
        m_resource = pstr.asString();
        const Json::Value &ppath = slot["path"];
        if (ppath.isNull()) {
            LOG_ERROR(Lgr) << "playlist '" << m_resource
                           << "' does not specify a 'path' entry " << ppath;
            throw Schedule_error();
        }
        m_res_path = expand_home( ppath.asString() );
        m_res_type = ResType::Playlist;
        return;
    }
    if (!ustr.isNull()) {       // URL
        m_resource = ustr.asString();
        m_res_type = ResType::URL;
        return;
    }
}


/// Load Source from JSON value.  May throw a Schedule_error.
///
void Source::load( const Json::Value &slot )
{
    // boolean options
    m_repeatp = get_bool_option( slot["repeat"], false );
    m_dynamic = get_bool_option( slot["dynamic"], false );

    m_medium = strtomedium( slot["mode"].asString() );
    if (Medium::radio == m_medium) {
        constexpr const unsigned HZ_PER_MHZ = 1'000'000;
        const Json::Value &fstr = slot["frequency"];
        if (fstr.isNull()) {
            LOG_ERROR(Lgr) << "Source {" << m_name
                           << "}: Missing frequency for radio mode";
            throw Schedule_error();
        }
        m_freq_hz = unsigned(HZ_PER_MHZ * std::stod(fstr.asString()));
        m_res_type = ResType::Frequency;
    }
    else if (Medium::mp3_stream == m_medium) {
        const Json::Value &ustr = slot["url"];
        if (ustr.isNull()) {
            LOG_ERROR(Lgr) << "Source {" << m_name
                           << "}: Missing url for stream mode";
            throw Schedule_error();
        }
        m_resource = ustr.asString();
        m_res_type = ResType::URL;
    }
    else if ((Medium::ogg_file == m_medium)||(Medium::mp3_file == m_medium)) {
        extract_local_resource(slot);
        try { // Check for existence of the file resource if that is possible
            // symbolic playlists will have an empty path
            // dynamic paths might not exist on any given day
            if (not m_res_path.empty() and not m_dynamic) {
                verify_readable( m_res_path );
            }
        }
        catch( std::exception &ex ) {
            LOG_ERROR(Lgr) << "Source '" << m_name << "': " << ex.what();
            throw Schedule_error();
        }
    }

    // A *file* source that does not repeat might be silent for a while if
    // play completes before the next program is scheduled to start.
    //
    if (((Medium::ogg_file == m_medium)||(Medium::mp3_file == m_medium))
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
}
