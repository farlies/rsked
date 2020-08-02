#pragma once

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

/// Class does some book keeping for timing an interval
/// Resolution: 1 second.
///
class IntervalTimer {
private:
    time_t _timeout;            // seconds
    time_t _start_time {0};
    bool _running {false};
public:
    IntervalTimer( time_t to ) :  // seconds
        _timeout(to) { }
    //
    bool expired() {
        return (_running and ((time(0) - _start_time) > _timeout));
    }
    bool running() const {
        return _running;
    }
    time_t timeout_secs() const {
        return _timeout;
    }
    void set_timeout( time_t to ) {
        _timeout = to;
    }
    void start() {              // no effect if already running
        if (not _running) {
            // std::cout << "start timer" << std::endl;
            _start_time = time(0);
            _running = true;
        }
    }
    void stop() {               // no effect if already stopped
        _start_time = 0;
        // std::cout << "stop timer" << std::endl;
        _running = false;
    }
};
