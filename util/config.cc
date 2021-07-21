/**
 * Methods for global configuration class Config
 * This uses a simplified JSON with a 2-level structure 
 * consisting of (1) section, and (2) parameter within section.
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

#include <ctime>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <jsoncpp/json/json.h>

#include "logging.hpp"
#include "config.hpp"
#include "configutil.hpp"

namespace fs = boost::filesystem;
namespace lt = boost::log::trivial;


////////////////////////////////////////////////////////////////////////



/// CTOR for Config establishes defaults
///
Config::Config()
{ }

/// CTOR with config path
///
Config::Config(const char* pathname)
{
    set_config_path(pathname);
}

      
/// Sets the configuration path.  This does *not* verify that
/// the file exists.
///
void Config::set_config_path( const std::string& p )
{
    m_config_path = expand_home(p);
}


/// Read configuration parameters for rsked per m_config_path
/// and setup working parameters.
///
void Config::read_config()
{
    if (not fs::exists(m_config_path)) {
        LOG_ERROR(Lgr)
            << "Config no such file: " << m_config_path;
        throw Config_file_error();
    }
    try {
        fs::ifstream sfile(m_config_path);
        sfile >> m_croot;       // n.b. may throw a Json::RuntimeError
    }
    catch (std::exception &e) {
        LOG_ERROR(Lgr) << "Config error reading from "
                       << m_config_path << ": " << e.what();
        throw Config_file_error();
    }
    m_file_writetime = last_write_time(m_config_path);
    //
    const Json::Value &sch=m_croot["schema"];
    if (not sch.isNull()) {
        m_schema = sch.asString();
    } else {
        m_schema = "unknown";
    }
}


/// may be called after load to log some information about the
/// config file: name, last write time, declared schema
///
void Config::log_about()
{
    LOG_INFO(Lgr) << "Config loaded file " << m_config_path;
    std::string lwt=std::ctime(&m_file_writetime);
    lwt.pop_back();
    LOG_INFO(Lgr) << "Config schema " << m_schema 
                  << ", last written " << lwt;
}

/// Check the disk file for possible changes since last load.
/// @returns true if the file has changed since the last load,
///   or false if the file has never been loaded.
///
bool Config::file_has_changed()
{
    if (m_file_writetime <= 0) { return false; } // never loaded
    std::time_t mtime = last_write_time(m_config_path);
    return (mtime > m_file_writetime);
}


/// Retrieve a bool value from section/param and deposit it in value,
/// returning true.  If the section/path cannot be found then
/// do not change value but return false.  May throw.
/// 
bool Config::get_bool(const char *section, const char *param, bool &value)
{
    const Json::Value &val=m_croot[section][param];
    if (val.isNull()) {
        return false;
    }
    value = val.asBool();
    LOG_INFO(Lgr) << "Config " << section << "." << param << "="
                  << (value ? "true" : "false");
    return true;
}

/// Retrieve a double value from section/param and deposit it in value,
/// returning true.  If the section/path cannot be found then set
/// value to dflt instead, returning false.
/// 
bool Config::get_double(const char *section, const char *param, double &value)
{
    const Json::Value &val=m_croot[section][param];
    if (val.isNull()) {
        return false;
    }
    value = val.asDouble();
    LOG_INFO(Lgr) << "Config " << section << "." << param << "=" 
                  << std::showpoint << value;
    return true;
}

/// Retrieve an unsigned value from section/param and deposit it in value,
/// returning true.  If the section/path cannot be found then set
/// value to dflt instead, returning false. May throw.
/// 
bool Config::get_unsigned(const char *section, const char *param,
                          unsigned &value)
{
    const Json::Value &val=m_croot[section][param];
    if (val.isNull()) {
        return false;
    }
    value = val.asUInt();
    LOG_INFO(Lgr) << "Config " << section << "." << param << "=" 
                  << value;
    return true;
}

/// Retrieve a Signed Integer value from section/param and deposit it
/// in value, returning true.  If the section.path cannot be found
/// then  value remains unchanged and this function returns false.
/// 
bool Config::get_int(const char *section, const char *param, int &value)
{
    const Json::Value &val=m_croot[section][param];
    if (val.isNull()) {
        return false;
    }
    value = val.asInt();
    LOG_INFO(Lgr) << "Config " << section << "." << param << "=" 
                  << value;
    return true;
}

/// Retrieve a Long Signed integer value from section/param and deposit it
/// in value, returning true.  If the section.path cannot be found
/// then  value remains unchanged and this function returns false.
/// 
bool Config::get_long(const char *section, const char *param, long &value)
{
    const Json::Value &val=m_croot[section][param];
    if (val.isNull()) {
        return false;
    }
    value = val.asInt();
    LOG_INFO(Lgr) << "Config " << section << "." << param << "=" 
                  << value;
    return true;
}

/// Retrieve a Json::Value at the given section/parameter location
/// returning true iff such a value is found.  This is an escape
/// mechanism to permit arbitrarily complex JSON content at the location.
///
bool
Config::get_jvalue(const char *section, const char *param, Json::Value &val)
{
    val=m_croot[section][param];
    if (val.isNull()) {
        return false;
    }
    return true;
}

/// Retrieve a string value from section/param and deposit it in value,
/// returning true.  If the section/path cannot be found then
/// value is unchanged and this function returns false. May throw.
/// 
bool Config::get_string(const char *section, const char *param,
                        std::string &value)
{
    const Json::Value &val=m_croot[section][param];
    if (val.isNull()) {
        return false;
    }
    value = val.asString();
    LOG_INFO(Lgr) << "Config " << section << "." << param << "=" << value;
    return true;
}

/// Retrieve a pathname from section.param and deposit it in value,
/// returning true.  If the section.param cannot be found then value
/// remains unchanged and function returns false. Additionally, test
/// whether the FileCond constraint is satisfied.  May throw.
/// 
bool Config::get_pathname( const char *section, const char *param,
                           FileCond cond, boost::filesystem::path &value)
{
    bool found=true;
    std::string pathstr {};
    const Json::Value &val=m_croot[section][param];
    if (val.isNull()) {
        found = false;
        // maybe expand home on the default value????
    } else {
        pathstr = val.asString();
        value = expand_home( pathstr );
    }
    if (cond == FileCond::MustExist) {
        if (not fs::exists(value)
            or not fs::is_regular_file(value)) {
            LOG_ERROR(Lgr) << "Config " << section << "." << param
                           << ", File not found: " << value;
            throw Config_path_error();
        }
    } else if (cond == FileCond::MustNotExist) {
        if (fs::exists(value)) {
            LOG_ERROR(Lgr) << "Config " << section << "." << param
                           << ", File already exists: " << value;
            throw Config_path_error();
        }
    } else if (cond == FileCond::MustExistDir) {
        if (not fs::exists(value)
            or not fs::is_directory(value)) {
            LOG_ERROR(Lgr) << "Config " << section << "." << param
                           << ", File not found: " << value;
            throw Config_path_error();
        }
    }
    LOG_INFO(Lgr) << "Config " << section << "." << param << "=" << value;
    return found;
}


