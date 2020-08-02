#pragma once

/* Logging functions and macros used by all rsked applications.
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

/// Load this before any other boost logging headers

/// must define this to use the shared lib
#define BOOST_LOG_DYN_LINK 1

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>

namespace lt = boost::log::trivial;

typedef
   boost::log::sources::severity_logger< lt::severity_level >
   rsked_logger_t;

extern rsked_logger_t  Lgr;  // global log source

#define LOG_DEBUG(_logger) BOOST_LOG_SEV(_logger,lt::debug)
#define LOG_INFO(_logger) BOOST_LOG_SEV(_logger,lt::info)
#define LOG_WARNING(_logger) BOOST_LOG_SEV(_logger,lt::warning)
#define LOG_ERROR(_logger) BOOST_LOG_SEV(_logger,lt::error)

/* Bit flags to pass init_logging to enable FILE and/or CONSOLE backends
 * and to enable debug messages.
 */
#define LF_FILE    1
#define LF_CONSOLE 2
#define LF_DEBUG   4

void init_logging(const char* /*appname*/, const char* /* "rsked_%5N.log" */, 
                  int flags=LF_FILE);
void finish_logging();

