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

#include <string>
#include <boost/filesystem.hpp>
#include <memory>
#include "radio.hpp"

/// Audio content media types
enum class Medium {
    off, radio, mp3_stream, ogg_file, mp3_file
};

/// Printed representation of Medium
extern const char* media_name(Medium);

/// Resource types
enum class ResType {
    None, File, Frequency, Directory, Playlist, URL
};

/// Defects in the schedule itself
class Schedule_error : public std::exception {
public:
    const char *what() const noexcept {
        return "Defective schedule--check the JSON.";
    }
};

/// foreshadow but do not include Json::Value
namespace Json {
    class Value;
}

/// Distinguished source that is automatically added (not in config file).
constexpr const char* OFF_SOURCE {"OFF"};


/// Describe an audio source
///
class Source {
private:
    void extract_local_resource( const Json::Value& );
    std::string m_alternate {OFF_SOURCE};  // name of alternate source
    bool m_failedp {false};      // last seen in a failed state?
    freq_t m_freq_hz {0};        // frequency in Hz for radio
    time_t m_last_fail {0};      // time of last failure (or 0)
    std::string m_name;          // e.g. "ksjn"
    Medium m_medium {Medium::off}; // off, radio, stream, ...
    bool m_quiet_okay {false};   // dead air okay?
    bool m_repeatp {false};      // repeat indefinitely?
    bool m_dynamic {false};      // substitute date into resource string?
    boost::filesystem::path m_res_path {};  // expanded path for file/dir
    ResType m_res_type {ResType::None}; // type of resource, e.g. Playlist
    std::string m_resource {};    // e.g file, directory, url
    time_t m_src_retry_secs { 60*60 }; // 1 Hour
public:
    //
    const std::string &alternate() { return m_alternate; };
    void clear();
    void describe() const;
    bool dynamic() const { return m_dynamic; }
    bool failedp() const { return m_failedp; }
    freq_t freq_hz() const { return m_freq_hz; }
    time_t last_fail() const { return m_last_fail; }
    void load(const Json::Value &);
    bool localp() const;
    void mark_failed(bool fp=true);
    bool may_be_quiet() const { return m_quiet_okay; }
    Medium medium() const { return m_medium; };
    const std::string& name() const { return m_name; }
    bool repeatp() const {return m_repeatp; }
    bool res_path(boost::filesystem::path&);
    ResType res_type() const { return m_res_type; }
    const std::string& resource() const;
    bool viable();
    //
    Source( std::string ); // name
    Source( const std::string&, const boost::filesystem::path& ); // name, path
};

/// Shared Pointer to a Source
typedef std::shared_ptr<Source> spSource;

bool strtobool( const std::string& );
Medium strtomedium( const std::string &s );
const char* restype_name( ResType);
const char* media_name(Medium);
