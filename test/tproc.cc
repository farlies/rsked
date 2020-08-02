/* Test the Child_mgr:
 * - check_running features
 *
 *   Part of the rsked package.
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


#include <signal.h>
#include <iostream>
#include "childmgr.hpp"
#include <boost/program_options.hpp>


/// binaries
const boost::filesystem::path BadBinaryPath {"/usr/local/bin/moggy_cat"};
const boost::filesystem::path OggPlayerPath {"/usr/bin/ogg123"};
const boost::filesystem::path NetcatPath { "/bin/nc" };


const boost::filesystem::path
TestOggFile {"/home/sharp/Music/Roedelius/Lustwandel/06-Harlekin.ogg"};

const boost::filesystem::path
BadOggFile {"/home/sharp/Music/Roedelius/Lustwandel/Nonexistent.ogg"};


//////////////////////////////////////////////////////////////////////////////

bool g_Terminate { false };


/// Signal handler function for various signals records the signal in
/// globals g_Terminate.  These are monitored in the test loop.
///
void my_sigterm_handler(int s)
{
    if ((s == SIGTERM) || (s == SIGINT) || (s==SIGQUIT)) {
        g_Terminate = true;
    }
}

/// Install a plain function that handles signals SIGTERM, SIGINT, SIGQUIT
///
void setup_sigterm_handler()
{
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = my_sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}


//////////////////////////////////////////////////////////////////////////////

/// Create an ogg player child, ready to run.
///
spCM setup_child1(bool badbin, bool baddarg, int min_run, int max_run)
{
    auto cm1 = Child_mgr::create( badbin ? BadBinaryPath : OggPlayerPath );
    if (badbin) { LOG_INFO(Lgr) << "Specifying a bad binary for cm1"; }

    cm1->set_name("ogg123");
    cm1->clear_args();

    cm1->add_arg( "--quiet" );
    if (baddarg) {
        LOG_INFO(Lgr) << "Play " << BadOggFile;
        cm1->add_arg( BadOggFile.c_str() );
    } else {
        LOG_INFO(Lgr) << "Play " << TestOggFile;
        cm1->add_arg( TestOggFile.c_str() );
    }
    if (min_run) {
        cm1->set_min_run( min_run );
        LOG_INFO(Lgr) << "Min_run=" << min_run
                      << "sec. Try kill -TERM from a terminal before this.";
    }
    if (max_run) {
        cm1->set_max_run( max_run );
        LOG_INFO(Lgr) << "Max_run=" << max_run
                      << " -- should complain after that";
    }
    return cm1;
}

/// Create an netcat listener child, ready to run.
///
spCM setup_child2( int port )
{
    std::string name {"nc"};
    name += std::to_string(port);
    auto cm = Child_mgr::create( NetcatPath );
    cm->set_name(name);
    cm->clear_args();

    LOG_INFO(Lgr) << "Netcat listen to port " << port;
    cm->add_arg( "-l" );
    cm->add_arg( "-p" );
    cm->add_arg( port );

    return cm;
}


/// Run child processes and test child mgr capabilities.
/// - cm1 : oggplayer will run until song is over or killed
/// - cm2, cm3 : netcat listeners will run until killed or error out
///
int main(int ac, char **av)
{
    int POLL_SECS {2};
    int maxrun {0};
    int minrun {0};
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help","option information")
        ("badarg","pass a bad argument to cm1 to make it die quickly")
        ("badbin","specify a bad binary for cm1 that won't execute")
        ("minrun",po::value<int>(&minrun),"shortest run time for cm1")
        ("maxrun",po::value<int>(&maxrun),"longest run time for cm1");

    po::variables_map vm;
    po::store( po::parse_command_line(ac,av,desc),vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        exit(0);
    }

    init_logging("tproc","tproc_%2N.log",LF_CONSOLE|LF_DEBUG);
    LOG_INFO(Lgr) << "Create Player Child '" << OggPlayerPath << "'";
    setup_sigterm_handler();

    try {
        bool badarg = (vm.count("badarg") > 0);
        bool badbin = (vm.count("badbin") > 0);
        auto cm1 = setup_child1(badbin,badarg,minrun,maxrun);
        auto cm2 = setup_child2(13001);
        auto cm3 = setup_child2(26002);
        //
        LOG_INFO(Lgr) << "Start Child 1";
        cm1->start_child();
        Child_mgr::ListInstances();

        LOG_INFO(Lgr) << "Start Child 2";
        cm2->start_child();
        Child_mgr::ListInstances();

        LOG_INFO(Lgr) << "Start Child 3";
        cm3->start_child();
        Child_mgr::ListInstances();

        RunCond status { RunCond::okay };
        while (1) {
            if (g_Terminate) {
                LOG_WARNING(Lgr) << "Test terminated by a signal";
                break;
            }
            if (not cm1->check_child( status ) ){
                LOG_WARNING(Lgr) << "Abnormal cm1 condition detected: "
                                 << Child_mgr::cond_name(status);
            }
            ChildPhase ph=cm1->last_obs_phase();
            if (ph==ChildPhase::gone) break;
            sleep(POLL_SECS);
#ifdef KILLTIME
            if (cm1->uptime() > KILLTIME) {
                cm1->kill_child();
            }
#endif
        }
        LOG_INFO(Lgr) << "cm1 handled " << cm1->updates() << " update(s)";
        LOG_INFO(Lgr) << "cm2 handled " << cm2->updates() << " update(s)";
        LOG_INFO(Lgr) << "cm3 handled " << cm3->updates() << " update(s)";
    } catch(std::exception &err) {
        LOG_ERROR(Lgr) << "Test threw an error: " << err.what();
    }
    Child_mgr::ListInstances();
    LOG_INFO(Lgr) << "Kill all child processes...";
    Child_mgr::kill_all();
    Child_mgr::ListInstances();
    finish_logging();
    return 0;
}
