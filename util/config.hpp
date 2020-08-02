#pragma once


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

#include <boost/filesystem.hpp>
#include <jsoncpp/json/json.h>

/// Conditions for testing pathnames
enum class FileCond { NA, MustExist, MustNotExist, MustExistDir };

/// Defects in the configuration (other than schedule)
struct Config_error : std::exception {
    const char *what() const noexcept {
        return "Defective configuration file.";
    }
};

/// Error reading a config file.
///
struct Config_file_error : std::exception {
    const char *what() const noexcept {
        return "Missing or unreadable configuration file.";
    }
};

/// A pathname value did (not) exist.
///
struct Config_path_error : std::exception {
    const char *what() const noexcept {
        return "Pathname condition failed.";
    }
};

/* Global configuration object. Loads configuration from file
 * and makes parameters available by name within section.
 */
class Config {
private:
    std::time_t m_file_writetime {0};
    Json::Value m_croot {};
    std::string m_schema {};
    std::string m_version {};
    boost::filesystem::path m_config_path {};
public:
    // all of these return true iff the param exists in the Config
    // and all may throw if they encounter a "bad" value
    bool get_bool(const char*, const char*, bool&);
    bool get_double(const char*, const char*, double&);
    bool get_pathname( const char*, const char*,
                       FileCond,  boost::filesystem::path &);
    const std::string& get_schema() const { return m_schema; }
    bool get_string(const char*, const char*, std::string&);
    bool get_unsigned(const char*, const char *, unsigned &);
    bool get_int(const char*, const char *, int &);
    bool get_long(const char*, const char *, long &);
    //
    bool file_has_changed();
    time_t last_file_write() const { return m_file_writetime; }
    void log_about();
    void read_config();
    void set_config_path( const std::string& );
    //
    Config();
    Config(const char*); // config file pathname
};




