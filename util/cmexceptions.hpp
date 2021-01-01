#pragma once
/// Exceptions for Childmgr and friends


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

/// Thrown on problem with child processes
struct CM_exception : public std::exception {
    const char* what() const throw() { return "Child_mgr exception"; }
};

/// Occurs when there is no child pid but one is required.
struct CM_nochild_exception : public CM_exception {
    const char* what() const throw() {
        return "Child_mgr No child process exception";
    }
};

/// Occurs when we cannot send a signal to the child.
struct CM_signal_exception : public CM_exception {
    const char* what() const throw() {
        return "Child_mgr Signal delivery exception";
    }
};

/// Occurs when we cannot start a child process.
struct CM_start_exception : public CM_exception {
    const char* what() const throw() {
        return "Child_mgr failed to start child process";
    }
};


struct CM_no_pty_exception : public CM_exception {
    const char* what() const throw() {
        return "CM has not enabled pty"; }
};


/// Errors in child pty operations throw this or one of its subclasses.
///
struct Chpty_exception : public CM_exception {
    const char* what() const throw() {
        return "Child pty exception"; }
};

struct Chpty_open_exception : public Chpty_exception {
    const char* what() const throw() {
        return " Child pty open exception"; }
};

struct Chpty_termio_exception : public Chpty_exception {
    const char* what() const throw() {
        return " Child pty termio exception"; }
};

struct Chpty_dup2_exception : public Chpty_exception {
    const char* what() const throw() {
        return " Child pty dup2 exception"; }
};

struct Chpty_ioctl_exception : public Chpty_exception {
    const char* what() const throw() {
        return " Child pty ioctl exception"; }
};

struct Chpty_read_exception : public Chpty_exception {
    const char* what() const throw() {
        return " Child pty read exception"; }
};

struct Chpty_write_exception : public Chpty_exception {
    const char* what() const throw() {
        return " Child pty write exception"; }
};
