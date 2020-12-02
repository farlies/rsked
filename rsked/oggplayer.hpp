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

#include "baseplayer.hpp"


/// Runs ogg123 on a file or directory. No support yet for ogg streams,
/// (which exist but are relatively rare.)
///
class Ogg_player : public Base_player {
private:
    void cap_init();
public:
    Ogg_player();
    Ogg_player(const char *, time_t);
    virtual ~Ogg_player();
    Ogg_player(const Ogg_player&) = delete;
    void operator=(Ogg_player const&) = delete;
    //
    virtual void initialize( Config&, bool ) override;
    virtual void play( spSource ) override;
};

