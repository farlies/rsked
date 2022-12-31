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

#pragma once

#include <memory>

class Rsked;

namespace Main {
    // The (unique) rsked instance is sometimes needed by components to
    // retrieve environment information.  Do via Main::rsked->foo(...)
    extern std::unique_ptr<Rsked> rsked;
    extern const char *AppName;
    extern ::std::string DefaultConfigPath;
    extern bool Button1;
    extern bool ReloadReq;
    extern bool Terminate;
    extern int  gTermSignal;

    void log_banner(bool);
}
