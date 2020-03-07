// utilities.h
//
// Simple tcp/ip server utilities
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

#ifndef UTILITIES_H
#define UTILITIES_H

// Allows improved perror reports
//
void perrorf (const char *format, ...);

const char* ownHostname ();

void delay (const double duration);

// Sets a file descriptor non blocking.
//
void setNonBlocking (const int fd);

// Provides current time approx relative to program start.
//
double getTimeSinceStart ();

// argv[0] | filter
//
void createPreProcess (const char* const argv[]);

// filter | argv[0]
//
void createPostProcess (const char* const argv[]);

// NOTE: This function does not return.
//
void runChildProcess (const int connectionFd,
                      const char* const argv[],
                      const bool inputIsCompressed,
                      const bool doCompressOutput);

#endif // UTILITIES_H
