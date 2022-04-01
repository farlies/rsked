
#include "test/fake_rsked.hpp"

/// The fake Rsked class needs a .o file so newer compilers wont
/// optimize it all away....


Rsked::Rsked() {
  boost::filesystem::path base {"../test/Music"};
  m_rps->set_library_base( base );
}

Rsked::~Rsked() { }

std::shared_ptr<const ResPathSpec>
Rsked::get_respathspec() const {
  return m_rps;
}

namespace Main {
    // The Main rsked instance is sometimes needed by components to
    // retrieve environment information via Main::rsked->foo(...)
    // We use a mock Rsked class, instantiated by the LogFixture below.

    std::unique_ptr<Rsked> rsked;
}

