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


#include <string>
#include <stdio.h>
#include "configutil.hpp"

namespace fs = boost::filesystem;
using namespace std;

////////////////////////////////////////////////////////////////////////


/// Expand a leading '~' to the home directory in the argument path,
/// and return the result.  This relies on *nix specific environment
/// variable HOME. It emulates the shell in a limited way.
///
/// TODO: Fix-me. This is not quite bullet-proof. It should
/// handle various cases cleanly:  "~", "~bob/foo".
/// Will throw invalid_argument error if the pattern is malformed.
///
fs::path expand_home(fs::path inpath)
{
    if (inpath.size() < 1) return inpath;
    string out { inpath.c_str() };
    if (out[0] == '~') {
        // TODO: other checks
        char const* phome = getenv("HOME");
        if (nullptr == phome) {
            throw invalid_argument("HOME not set in environment.");
        }
        out.replace(0, 1, phome);
        return fs::path(out);
    }
    return inpath;
}

////////////////////////////////////////////////////////////////////////

/// Check by reading a byte from the named file, or verifying
/// that the directory may be enumerated and is *non-empty*.
/// Returns if readable; throws an exception if not.
///
bool verify_readable( const boost::filesystem::path &p )
{
    using namespace boost::filesystem;
    if (exists(p)) {
        if (is_directory(p)) {
            // try opening and finding any readable file
            for (directory_entry& x : directory_iterator(p)) {
                if (verify_readable(x.path())) { return true; }
            }
            throw invalid_argument(string("No readable files in ")+p.native());
        }
        else if (is_regular_file(p)) {
            // try reading a byte
            FILE *sfile = fopen(p.c_str(),"r");
            if (sfile) {
                fclose(sfile);
                return true;
            }
            throw invalid_argument(string("Cannot read file ")+p.native());
        }
        else {
            throw invalid_argument("Cannot verify path "+ p.native()); // symlink?
        }
    } else {
        throw invalid_argument(string("No such path ")+p.native());
    }
    return false;
}
