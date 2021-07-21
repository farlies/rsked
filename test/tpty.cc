/// Test chpty

/// Dynamically link boost test framework
#define BOOST_TEST_MODULE source_test
#ifndef BOOST_TEST_DYN_LINK
#define BOOST_TEST_DYN_LINK 1
#endif
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


#include "util/chpty.hpp"
#include <iostream>
#include <unistd.h>


const char* VlcBinary = "/usr/bin/vlc";


void do_child( Pty_controller& pty )
{
    pty.child_init();
    execlp( VlcBinary, "vlc-cli", "-Icli", (char*) NULL );
    perror("exec failed:");
}


void do_parent( Pty_controller& pty, int pid )
{
    std::cout << "Child pid=" << pid << '\n';
    sleep(1);   // allow child to wake up

    std::string result{};
    pty.read_nb(result,2048);
    std::cout << "Startup banner:\n" << result;
    result.clear();

    // Note: commands must end in newline
    pty.write_nb( "stop\n" "status\n" );
    pty.read_nb(result,2048);
    BOOST_TEST(result.npos != result.find("( state stopped )"));
    sleep(1);

    // tell child to exit
    pty.write_nb("shutdown\n");
    sleep(1);
}


BOOST_AUTO_TEST_CASE( run_vlc_cli )
{
    Pty_controller pty;
    pty.open_pty();
    std::cout << "Remote pty: " << pty.remote_name() << '\n';

    int pid = fork();

    BOOST_REQUIRE(-1 != pid);
    if (0 == pid) {
        do_child(pty);
    } else {
        do_parent(pty,pid);
    }
}
