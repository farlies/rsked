/**
 * Boost logging setup and teardown functions.
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


/// must define this to use the shared lib
#define BOOST_LOG_DYN_LINK 1

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/move/utility_core.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>


namespace logging = boost::log;
namespace triv = boost::log::trivial;
namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;

#include "logging.hpp"
#include "configutil.hpp"

/// Application name to include
const char* LogAppName=nullptr;

/// Global log source for rsked:
rsked_logger_t  Lgr;

/// rsked log formatter : prints severity and timestamp
///
void rlog_formatter(logging::record_view const& rec,
                    logging::formatting_ostream& strm)
{
    auto date_time_formatter = expr::stream
        << expr::format_date_time< boost::posix_time::ptime >
               ("TimeStamp","%Y-%m-%d %H:%M:%S");
    date_time_formatter(rec, strm);
    strm << " <" << rec[lt::severity] << "> [" << LogAppName << "] ";
    strm << rec[expr::smessage];
}

using sink_t = sinks::synchronous_sink< sinks::text_file_backend >;

/// Setup for logs to be collected into a directory.
///
static void
init_file_collecting(boost::shared_ptr< sink_t > sink)
{
    auto realpath = expand_home("~/logs_old");

    sink->locked_backend()->set_file_collector(sinks::file::make_collector(
        keywords::target = realpath.c_str(),
        keywords::max_size = 16 * 1024 * 1024, // 16 MB limit
        keywords::min_free_space = 100 * 1024 * 1024, // leave this many MB free
        keywords::max_files = 7
    ));
}

/// Log to files, rotating at midnight daily or if the size grows to
/// more than 5MB.  file_pattern indicates the log filename pattern.
/// autoflush this backend so we can capture errors asap
///
static void
init_file_logging( boost::shared_ptr< logging::core > core,
                   const char* file_pattern )
{
    boost::shared_ptr< sinks::text_file_backend > backend =
        boost::make_shared< sinks::text_file_backend >(
            keywords::file_name = file_pattern,
            keywords::rotation_size = (5 * 1024 * 1024),
            keywords::time_based_rotation
               = sinks::file::rotation_at_time_point(0, 0, 0)
            );
    // Flush after every log message--not clear this is effective...
    backend->auto_flush(true);

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    boost::shared_ptr< sink_t > sink(new sink_t(backend));

    // Upon restart, scan the directory for files matching file_pattern
    init_file_collecting(sink);
    sink->locked_backend()->scan_for_files();

    // Add a formatter
    sink->set_formatter(&rlog_formatter);

    core->add_sink(sink);

}


/// Create a console ostream backend and attach std::clog to it
/// This code is not required for headless operation.
///
static void
init_console_logging( boost::shared_ptr< logging::core > core )
{
    boost::shared_ptr< sinks::text_ostream_backend > os_backend =
        boost::make_shared< sinks::text_ostream_backend >();

    os_backend->add_stream(
        boost::shared_ptr< std::ostream >(&std::clog, boost::null_deleter()));

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    using os_sink_t = sinks::synchronous_sink< sinks::text_ostream_backend >;
    boost::shared_ptr< os_sink_t > os_sink(new os_sink_t(os_backend));
    // Add a formatter
    os_sink->set_formatter(&rlog_formatter);
    core->add_sink(os_sink);
}


/// Init the logger with console and/or text_file_backends
/// Typical file_pattern: "rsked_%5N.log"
///
void init_logging(const char* appname, const char* file_pattern, int flags)
{
    LogAppName = appname;
    boost::shared_ptr< logging::core > core = logging::core::get();
    if (flags & LF_FILE) {
        init_file_logging(core,file_pattern);
    }
    if (flags & LF_CONSOLE) {
        init_console_logging(core);
    }
    logging::add_common_attributes();

    // Set level filter ignore debug messages unless LF_DEBUG flag
    if (0 == (flags & LF_DEBUG)) {
        core->set_filter(triv::severity >= triv::info);
    }

}

/// Terminate the logger, cleaning up any files.
///
void finish_logging()
{
    logging::core::get()->remove_all_sinks();
}
