// utilities.cpp
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

#include "utilities.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXHOSTNAME           256

//------------------------------------------------------------------------------
// Allows improved perror reports
//
void perrorf (const char *format, ...)
{
   char buffer[200];
   va_list pvar;
   va_start (pvar, format);
   vsnprintf (buffer, sizeof (buffer), format, pvar);
   va_end (pvar);

   perror (buffer);
}

//------------------------------------------------------------------------------
//
const char* ownHostname ()
{
   static char static_hostname[MAXHOSTNAME + 1] = "";

   if (strlen (static_hostname) <= 0) {
      gethostname (static_hostname, MAXHOSTNAME);
   }

   return static_hostname;
}

//------------------------------------------------------------------------------
//
void delay (const double durationIn)
{
   double duration = durationIn;
   if (duration < 0.0) duration = 0.0;

   if (duration < 1.0E9) {
      // < 1000 seconds, use usec sleep.
      usleep (__useconds_t (1.0e+6*duration));
   } else {
      // Just use whole number of seconds.
      sleep (unsigned (duration));
   }
}

//------------------------------------------------------------------------------
//
void setNonBlocking (const int fd)
{
   long flags;
   int status;

   flags = (long) fcntl (fd, F_GETFL, 0);
   if (flags < 0) {
      perrorf ("fcntl (%d, F_GETFL)", fd);
      return;
   }
   flags = flags | O_NONBLOCK;
   status = fcntl (fd, F_SETFL, flags);
   if (status < 0) {
      perrorf ("fcntl (%d, F_SETFL)", fd);
   }
}

//------------------------------------------------------------------------------
//
double getTimeSinceStart ()
{
   static const __time_t epoch_sec = time (NULL);

   struct timespec spec;

   clock_gettime (CLOCK_REALTIME, &spec);

   double result = double (spec.tv_sec - epoch_sec) + (double (spec.tv_nsec) / 1.0e9);
   return result;
}

//------------------------------------------------------------------------------
// Allows casting of const char* const argv[]to a type acceptable to execvp
// as in execvp (argv[0], ARGV (argv));
//
typedef char* const*  ARGV;

//------------------------------------------------------------------------------
//
void createPreProcess (const char* const argv[])
{
    int fd_set [2];
    int status;
    pid_t pid;

    status = pipe (fd_set);
    if (status < 0) {
       perrorf ("createPreProcess.pipe()");
       _exit (4);   // terminates the child process
    }

    pid = fork ();
    if (pid < 0) {
       perrorf ("fork ()");
       _exit (4);   // terminates the child process
    }

    if (pid > 0) {
       // We are the parent process
       //
       // Close unsued end of pipe
       //
       close (fd_set [1]); // close output

       int fd;
       fd = dup2 (fd_set [0], STDIN_FILENO);
       if (fd != STDIN_FILENO) {
          perror ("dup2 (fd, STDIN_FILENO)");
          _exit (4);   // terminates the child process
       }

    } else {
       // We are the child process
       //
       // Close unsued end of pipe
       //
       close (fd_set [0]); // close input

       int fd;
       fd = dup2 (fd_set [1], STDOUT_FILENO);
       if (fd != STDOUT_FILENO) {
          perror ("dup2 (fd, STDOUT_FILENO)");
          _exit (4);   // terminates the child process
       }

       // Now exec to new pre process.
       //
       execvp (argv[0], ARGV (argv));

       // The exec call failed.
       //
       perrorf ("execvp (%s , ...)", argv[0]);
       _exit (4);   // terminates the child process
    }
}

//------------------------------------------------------------------------------
//
void createPostProcess (const char* const argv[])
{
   int fd_set [2];
   int status;
   pid_t pid;

   status = pipe (fd_set);
   if (status < 0) {
      perrorf ("createPreProcess.pipe()");
      _exit (4);   // terminates the child process
   }

   pid = fork ();
   if (pid < 0) {
      perrorf ("fork ()");
      _exit (4);   // terminates the child process
   }

   if (pid > 0) {
      // We are the parent process
      //
      // Close unused end of pipe
      //
      close (fd_set [0]); // close input

      int fd;
      fd = dup2 (fd_set [1], STDOUT_FILENO);
      if (fd != STDOUT_FILENO) {
         perror ("dup2 (fd, STDOUT_FILENO)");
         _exit (4);   // terminates the child process
      }

   } else {
      // We are the child process
      //
      // Close unused end of pipe
      //
      close (fd_set [1]); // close output

      int fd;
      fd = dup2 (fd_set [0], STDIN_FILENO);
      if (fd != STDIN_FILENO) {
         perror ("dup2 (fd, STDIN_FILENO)");
         _exit (4);   // terminates the child process
      }

      // Now exec to new post process.
      //
      execvp (argv[0], ARGV (argv));

      // The exec call failed.
      //
      perrorf ("execvp (%s , ...)", argv[0]);
      _exit (4);   // terminates the child process
   }
}

//------------------------------------------------------------------------------
// NOTE: This function does not return
//
void runChildProcess (const int connectionFd,
                      const char* const argv[],
                      const bool inputIsCompressed,
                      const bool doCompressOutput)
{
   // Connect standard IO to TCP/IP connection file descriptor fd
   //
   int fdin = dup2 (connectionFd, STDIN_FILENO);
   if (fdin != STDIN_FILENO) {
      perror ("dup2 (fd, STDIN_FILENO)");
      _exit (4);   // terminates the child process
   }

   int fdout = dup2 (connectionFd, STDOUT_FILENO);
   if (fdout != STDOUT_FILENO) {
      perror ("dup2 (fd, STDOUT_FILENO)");
      _exit (4);   // terminates the child process
   }

   // from posix/osdProcess.c
   // close all open files except for STDIO so they will not
   // be inherited by the spawned process.
   //
   // We "know" standard file descriptors are 0, 1 and 2
   //
   const int maxfd = sysconf (_SC_OPEN_MAX);
   for (int tfd = 3; tfd <= maxfd; tfd++) {
      close (tfd);
   }

   if (inputIsCompressed) {
      // Create a pre filter process to gunzip the input.
      //
      const char* const paargv[] = { "gunzip", NULL };
      createPreProcess (paargv);
   }

   if (doCompressOutput) {
      // Create a post filter process to gzip the output.
      //
      const char* const ppargv[] = { "gzip", NULL };
      createPostProcess (ppargv);
   }

   // Now exec to new process.
   //
   execvp (argv[0], ARGV (argv));

   // The exec call failed.
   //
   perrorf ("execvp (%s , ...)", argv[0]);

   // Don't run our parent's atexit() handlers
   //
   _exit (8);
}



// end

