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

/// Some elementary definitions used in multiple sources...


/// Modes a player may be in at any given time:
///   Stopped : not playing anything right now
///   Playing : actively playing a Source
///   Paused  : stopped, but may resume where it was interrupted
///   Broken  : not currently usable on account of error
///   Disabled: disallowed by choice, but potentially usable
///
enum class PlayerState {
    Stopped,
    Playing,
    Paused,
    Broken,
    Disabled
};


/// Thrown on problem with players. Specialized in other sources.
///
struct Player_exception : public std::exception {
    const char* what() const throw() { return "Generic Player exception"; }
};
