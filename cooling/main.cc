/**
 * The cooling application for rsked:
 *  - starts rsked, and restarts if necessary
 *  - turns the cooling fan on/off as needed
 *  - signals rsked when the GPIO button is pressed
 *  - monitors rsked state and alters GPIO LEDs t reflect
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
#include <boost/program_options.hpp>
#include "cooling.hpp"
#include "version.h"

/// Official name used in pid file
const char *AppName = "cooling";


/////////////////////////////////////////////////////////////////////////////

/// Create and (optionally) run a cooling object.
///
int main (int ac, char **av)
{
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help","option information")
        ("config",po::value<std::string>(),"use a particular config file")
        ("console","echo log to console in addition to log file")
        ("debug","show debug level messages in logs")
        ("test","test configuration and exit without running")
        ("version","print version identifier and exit without running");
    
    po::variables_map vm;
    po::store( po::parse_command_line(ac,av,desc),vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        exit(0);
    }
    if (vm.count("version")) {
        std::cout << AppName << " version "
                  << VERSION_STR "  built "  __DATE__ " " __TIME__  << "\n";
        exit(0);
    }
    bool debugp  { vm.count("debug") > 0 };
    bool testp   { vm.count("test") > 0 };
    try {
        auto cooling( std::make_unique<Cooling>(vm) );
        cooling->initialize(debugp);       // this will start the logger
        if (not testp) {
            cooling->run();
        }
    } catch (std::exception &ex) {
        std::cerr << AppName << " threw a fatal error: " << ex.what() << "\n";
        return 1;
    }
    Child_mgr::kill_all();
    return 0;
}

