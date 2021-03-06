/**
 * Represent and use pathnames to locate various types of local audio
 * resources.
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

#include <string>

#include "logging.hpp"
#include "respath.hpp"
#include "util/configutil.hpp"

namespace fs = boost::filesystem;

/////////////////////////// ResPathSpec ////////////////////////////////

/// CTOR: establish standard default paths
ResPathSpec::ResPathSpec()
{
    const char *hptr = getenv("HOME");
    if (hptr==nullptr) {
        LOG_WARNING(Lgr) << "ResPathSpec: HOME not set in environment";
        m_home = "/";
    } else {
        m_home = hptr;
        m_home += '/';
    }
    m_library_path = m_home + "Music/";
    m_announcement_path = m_home + ".config/rsked/";
    m_playlist_path = m_home + ".config/mpd/playlists/";
    LOG_INFO(Lgr) << "Default Music library: " << m_library_path;
    LOG_INFO(Lgr) << "Default Announcements: " << m_announcement_path;
    LOG_INFO(Lgr) << "Default Play lists:    " << m_playlist_path;
}

/// Specify the music library directory.
/// * May throw if cannot canonicalize
///
void ResPathSpec::set_library_base(const fs::path &base)
{
    fs::path xbase = expand_home( base );
    try {
        m_library_path = fs::canonical( xbase );
    }
    catch(const fs::filesystem_error &fse) {
        LOG_ERROR(Lgr) << "ResPathSpec bad music library base " << base;
        throw;
    }
    LOG_INFO(Lgr) << "Music library: " << m_library_path;
}

/// Specify the playlist directory.
/// * May throw if cannot canonicalize
///
void ResPathSpec::set_playlist_base(const fs::path &base)
{
    fs::path xbase = expand_home( base );
    try {
        m_playlist_path = fs::canonical( xbase );
    }
    catch(const fs::filesystem_error &fse) {
        LOG_ERROR(Lgr) << "ResPathSpec bad playlist base " << base;
        throw;
    }
    LOG_INFO(Lgr) << "Play lists:    " << m_playlist_path;
}

/// Specify the announcement directory.
/// * May throw if cannot canonicalize
///
void ResPathSpec::set_announcement_base(const fs::path &base)
{
    fs::path xbase = expand_home( base );
    try {
        m_announcement_path = fs::canonical( xbase );
    }
    catch(const fs::filesystem_error &fse) {
        LOG_ERROR(Lgr) << "ResPathSpec bad announcement base " << base;
        throw;
    }
    LOG_INFO(Lgr) << "Announcements: " << m_announcement_path;
}


/// If input path inp is relative, base it in the base directory.
/// If inp is already absolute, just return it.
///
static void
maybe_base(const fs::path &inp, const fs::path &basedir, fs::path &outp)
{
    if (inp.is_relative()) {
        outp = fs::absolute( inp, basedir );
    } else {
        outp = inp;
    }
}

/// If input path is relative, base it in the Music library directory.
/// If it is absolute, just return it.
///
void ResPathSpec::resolve_library(const fs::path &inp, fs::path &outp) const
{
    maybe_base( inp, m_library_path, outp );
}


/// If input path is relative, base it in the announcement directory.
/// If it is absolute, just return it.
///
void ResPathSpec::resolve_announcement(const fs::path &inp, fs::path &outp) const
{
    maybe_base( inp, m_announcement_path, outp );
}

/// If input path is relative, base it in the playlist directory.
/// If it is absolute, just return it.
///
void ResPathSpec::resolve_playlist(const fs::path &inp, fs::path &outp) const
{
    maybe_base( inp,  m_playlist_path, outp );
}
