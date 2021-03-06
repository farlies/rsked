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

#include <boost/filesystem.hpp>

/// Represent root directory paths for various types of local resources
///
class ResPathSpec {
    using path=boost::filesystem::path;
private:
    std::string m_home {""};      // HOME directory (used in defaults)
    path m_library_path {};       // Music Library
    path m_announcement_path {};  // Announcements
    path m_playlist_path {};      // Playlists
public:
    const path& get_libpath() const { return m_library_path; }
    const path& get_playlistpath() const { return m_playlist_path; }
    const path& get_announcepath() const { return m_announcement_path; }
    void resolve_library(const path&, path&) const;
    void resolve_announcement(const path&, path&) const;
    void resolve_playlist(const path&, path&) const;
    void set_library_base(const path&);
    void set_playlist_base(const path&);
    void set_announcement_base(const path&);
    //
    ResPathSpec();
};
