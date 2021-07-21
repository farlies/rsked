#pragma once


#include "rsked/respath.hpp"

/// We fake an Rsked class that has the global res path method in it
/// so that Vlc_player can look this up during initialization.
/// The music library is rebased to the test/Music directory.

class Rsked {
public:
    std::shared_ptr<ResPathSpec> m_rps {  std::make_shared<ResPathSpec>() };

    std::shared_ptr<const ResPathSpec> get_respathspec() const {
        return m_rps;
    }
    Rsked() {
        boost::filesystem::path base {"../test/Music"};
        m_rps->set_library_base( base );
    }
};


namespace Main {
    extern std::unique_ptr<Rsked> rsked;
}
