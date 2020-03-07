// listener_socket.h
//
// Simple tcp/ip server listener socket
//
// Copyright (c) 2020 Andrew Starritt
//
// The filter server program is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The filter server program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// the filter server program.  If not, see <http://www.gnu.org/licenses/>.
//
// Author: Andrew Starritt
// Contact details:  andrew.starritt@gmail.com
//

#ifndef LISTENER_SOCKET_H
#define LISTENER_SOCKET_H

// Creates a listemrr socket for the specified port number
// on the local host (but not 127.0.0.1).
// Return value:
// >= 0 - file descriptor
// <  0 - failed.
//
int createListener (const int local_port);

#endif  // LISTENER_SOCKET_H
