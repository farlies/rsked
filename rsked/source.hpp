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
#include "common.hpp"
#include "radio.hpp"
#include "respath.hpp"

/// Audio media types
enum class Medium {
    off,       // the void, the sound of silence
    radio,     // signals over electromagnetic waves
    stream,    // digital transmission over the internet
    file,      // a local file with encoded audio
    directory, // a local directory with audio files
    playlist   // a local file containing names of LOCAL audio files
};

/// Audio encoding types
enum class Encoding {
    none,  // used only by the "off" medium
    ogg,   // short for ogg container with vorbis content (.ogg)
    mp3,   // MPEG-1 Audio Layer III (.mp3)
    mp4,   // MPEG-4 Part 3 in Part 14 box (.mp4, .m4a, .m4b, .aac)
    flac,  // FLAC, free lossless audio compression in a FLAC box
    wfm,   // wide FM,  typical commercial radio stations
    nfm,   // narrow FM--weather radio, ham radio
    mixed  // arbitrary mixture of ogg/mp3/mp4/flac, e.g. in a playlist
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
    std::string m_name;          // source name, e.g. "ksjn"
    std::string m_alternate {OFF_SOURCE};  // name of alternate source
    bool m_failedp {false};      // last seen in a failed state?
    time_t m_last_fail {0};      // time of last failure (or 0)
    time_t m_src_retry_secs { 60*60 }; // delay before retries, 1 Hour
    // characteristics:
    Medium m_medium {Medium::off}; // how we get to it, e.g. radio
    Encoding m_encoding {Encoding::none};   // to identify codec
    double m_duration {0.0};      // seconds and fractions thereof
    bool m_announcementp {false}; // true: is an announcement
    std::string m_text {};        // for announcements
    bool m_quiet_okay {false};    // dead air okay?
    bool m_repeatp {false};       // repeat indefinitely?
    bool m_dynamic {false};   // substitute date into resource string?
    freq_t m_freq_hz {0};     // frequency in Hz for radio
    std::string m_resource {};  // filename, directory name, url, ...
    boost::filesystem::path m_res_path {};  // full expanded pathname
    //
    void extract_local_resource( const Json::Value& );
    void extract_required_props( const Json::Value& );
public:
    const std::string &alternate() { return m_alternate; };
    bool announcement() { return m_announcementp; };
    void clear();
    void describe() const;
    bool dynamic() const { return m_dynamic; }
    Encoding encoding() const { return m_encoding; }
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
    const std::string& resource() const;
    void validate(const ResPathSpec&);
    bool viable();
    //
    Source( const std::string& ); // name
};

/// Shared Pointer to a Source
using spSource = std::shared_ptr<Source>;


/// Prototypes
extern bool strtobool( const std::string& );
extern Medium strtomedium( const std::string & );
extern Encoding strtoencoding( const std::string & );
extern const char* encoding_name(const Encoding);
extern const char* media_name(const Medium);
extern bool uri_expand_time( const std::string&, std::string&);
