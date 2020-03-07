// filter_server.cpp
//
// Simple tcp/ip server that runs a filter.
//
// Copyright (c) 2019-2020 Andrew Starritt
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "utilities.h"
#include "listener_socket.h"

#define MAXIMUM_CONNECTIONS   80
#define VERSION_STRING        "1.2.1"


//------------------------------------------------------------------------------
//
static void printUsage (FILE * stream)
{
   const char* const message =
         "\n"
         "usage: filter_server [OPTIONS] port command args...\n"
         "       filter_server [--help|-h]\n"
         "       filter_server [--version|-v]\n"
         "\n";

   fprintf (stream, message);
}

//------------------------------------------------------------------------------
//
static void printHelp ()
{
   const char* const prolog =
         "filter_server %s\n"
         "\n"
         "filter_server provides the means to run any arbitary command, script or program,\n"
         "that accepts input from standard input and writes its result to standard output\n"
         "as a forking TCP/IP service.\n";
         
   const char* const epilog =
         "Options:\n"
         "--sessions, -s The maximum number of allowed simultaneous session or connections.\n"
         "               This will be clamped to the range 1 to %d\n"
         "               The default is 20 sessions.\n"
         "\n"
         "--timeout, -t  The maximum time in seconds that a session is allowed to run for.\n"
         "               It may be qualified with m, h, d or w for minutes, hours, days\n"
         "               and weeks respectively. 'none' means no timeout.\n"
         "               The timeout will be adjusted to be >= 1.0 seconds if needs be.\n"
         "               The default is 1d.\n"
         "\n"
         "--unzip, -u    Decompress the input (using gunzip) sent to the filter command.\n"
         "\n"
         "--zip, -z      Compress output (using gzip) from the filter command.\n"
         "\n"
         "--version, -v  Show program version and exit.\n"
         "\n"
         "--help, -h     Show this help information and exit.\n"
         "\n"
         "Parameters:\n"
         "port           The port number on whuich the service will run.\n"
         "               Must be >= 1024 for non-root privileged users.\n"
         "\n"
         "command        The command to be run. This must be on the PATH and/or specified\n"
         "               using an absolute path.\n"
         "\n"
         "args...        Optional arguments passed to the command executable.\n"
         "\n"
         "\n"
         "Example (trivial):\n"
         "\n"
         "on server...\n"
         "   filter_server -- 4242 stdbuf -oL tr 'a-z' 'A-Z'\n"
         "\n"
         "   stdbuf is an easy way to modify (output) buffering.\n"
         "\n"
         "on client...\n"
         "   ncat server_hosts 4242\n"
         "\n"
         "   Any text typed on the command line will be converted to upper case.\n"
         "\n";

   fprintf (stdout, prolog, VERSION_STRING);
   printUsage (stdout);
   fprintf (stdout, epilog, MAXIMUM_CONNECTIONS);
}

//------------------------------------------------------------------------------
// Holds data about each child process,
//
enum ProcessState { psRunning, psTerminated, psKilled };
struct ProcessData {
   pid_t pid;
   ProcessState state;
   double expiryTime;
};

typedef struct ProcessData  ProcessList [MAXIMUM_CONNECTIONS];

//------------------------------------------------------------------------------
//
static void checkUpOnTheKids (ProcessList children, const int maximumSessions)
{
   const double timeNow = getTimeSinceStart ();

   for (int j = 0; j < maximumSessions; j++) {

      ProcessData* proc = &children [j];

      // This slot in use?
      //
      if (proc->pid >= 0) {
         int waitpid_code;
         int status;
          
         waitpid_code = waitpid (proc->pid, &status, WNOHANG);
         if (waitpid_code == -1) {
            perrorf ("waitpid (%d, &status, WNOHANG)", proc->pid);
            continue;
         }
         
         if (waitpid_code == proc->pid) {
             // child process is complete
             //
             fprintf (stdout, "Process %d is complete, exit code: %d.\n",
                               proc->pid, status >> 8);
             proc->pid = -1;   // clear slot
             continue;
         }

         if (timeNow >= proc->expiryTime) {

            switch (proc->state) {
               case psRunning:
                  fprintf (stdout, "Timeout: terminating process %d\n", proc->pid);
                  status = kill (proc->pid, SIGTERM);
                  if (status < 0) {
                     perrorf  ("kill (%d, SIGTERM)", proc->pid);
                  }
                  proc->state = psTerminated;
                  break;

               case psTerminated:
                  if (timeNow >= proc->expiryTime + 2.0) {
                     fprintf (stdout, "Timeout: killing process %d\n", proc->pid);
                     status = kill (proc->pid, SIGKILL);
                     if (status < 0) {
                        perrorf  ("kill (%d, SIGKILL)", proc->pid);
                     }
                     proc->state = psKilled;
                  }
                  break;

               case psKilled:
                  break;
            }
         }
      }
   }
}

//------------------------------------------------------------------------------
// Find an empty slot if available.
//
static int findSlot (const ProcessList children, const int maximumSessions)
{
   int result = -1;

   for (int j = 0; j < maximumSessions; j++) {
      if (children [j].pid == -1) {
         result = j;
         break;
      }
   }

   return result;
}


//------------------------------------------------------------------------------
//
int main (int argc, char** argv)
{
   // Set default option values.
   //
   bool inputIsCompressed = false;
   bool doCompressOutput = false;
   int maximumSessions = 20;
   double maximumTime = 24.0 * 3600.0;  // 1 day

   // Process options
   //
   while (true) {
      int option_index = 0;
      ssize_t value = 0;

      // All long options also have a short option
      //
      static const struct option long_options[] = {
         {"help", no_argument, NULL, 'h'},
         {"version", no_argument, NULL, 'v'},
         {"unzip", required_argument, NULL, 'u'},
         {"zip", required_argument, NULL, 'z'},
         {"sessions", required_argument, NULL, 's'},
         {"timeout", required_argument, NULL, 't'},
         {NULL, 0, NULL, 0}
      };

      const int c = getopt_long (argc, argv, "hvuzs:t:", long_options, &option_index);
      if (c == -1)
         break;

      switch (c) {

         case 0:
            // All long options also have a short option
            //
            fprintf (stderr, "unexpected option %s", long_options[option_index].name);
            if (optarg)
               fprintf (stderr, " with arg %s", optarg);
            fprintf (stderr, "\n");
            printUsage (stderr);
            return 1;
            break;

         case 'h':
            printHelp ();
            return 0;
            break;

         case 'v':
            fprintf (stdout, "Filter Server. Version: %s\n", VERSION_STRING);
            return 0;
            break;

         case 'u':
            inputIsCompressed = true;
            break;

         case 'z':
            doCompressOutput = true;
            break;

         case 't':
            {
               if (strcmp (optarg, "none") == 0) {
                  maximumTime = 1.0E+20;  //  life of universe plus alot more ;-)
                  break;
               }

               char xx = ' ';
               int n = sscanf (optarg, "%ld%c", &value, &xx);
               if (n < 1) {
                  // We expect atleat a value.
                  //
                  printUsage (stderr);
                  return 1;
               }

               if (n == 1) {
                  maximumTime = double (value);
                  break;
               }

               if (n == 2) {
                  if (xx == ' ') {
                     maximumTime = double (value);
                  } else if (xx == 'm') {
                     maximumTime = 60.0 * double (value);
                  } else if (xx == 'h') {
                     maximumTime = 3600.0 * double (value);
                  } else if (xx == 'd') {
                     maximumTime = 86400.0 * double (value);
                  } else if (xx == 'w') {
                     maximumTime = 604800.0 * double (value);
                  } else {
                     fprintf (stderr, "usage - timeout modifier %c\n", xx);
                     printUsage (stderr);
                     return 1;
                  }
               }
            }
            break;

         case 's':
            maximumSessions = atoi (optarg);
            break;

         case '?':
            // invalid option
            //
            printUsage (stderr);
            return 1;
            break;

         default:
            fprintf (stderr, "?? getopt returned character code 0%o ??\n", c);
            break;

      }
   }

   // Sanitise options.
   //
   if (maximumSessions > MAXIMUM_CONNECTIONS) {
      maximumSessions = MAXIMUM_CONNECTIONS;
   } else if (maximumSessions < 1) {
      maximumSessions = 1;
   }

   if (maximumTime < 1.0) {
      maximumTime = 1.0;
   }

   // Process parameters

   const int numberArgs = argc - optind;
   if (numberArgs < 2) {
      fprintf (stderr, "Too few arguments\n");
      printUsage (stderr);
      return 1;
   }

   const int port = atoi (argv [optind++]);

   // Adjust argc/argv such that it represnts the command and arguments to to tun.
   //
   argc -= optind;
   argv += optind;

   // Verify sensible port number.
   //
   if ((port < 1) || (port > 65535)) {
      fprintf (stderr, "port number must be in range 1 to 65535\n");
      return 2;
   }

   if (port < 1024) {
      fprintf (stderr, "warning: port %d requires root priviledge\n", port);
   }

   if (strcmp (argv[0], "") == 0) {
      fprintf (stderr, "command is empty\n");
      return 2;
   }

   // Report settings
   //
   fprintf (stdout, "port :             %d\n", port);
   fprintf (stdout, "maximum sessions : %d\n", maximumSessions);
   fprintf (stdout, "maximum time :     %.5g s\n", maximumTime);
   fprintf (stdout, "decompress input : %s\n", inputIsCompressed ? "yes" : "no");
   fprintf (stdout, "compress output :  %s\n", doCompressOutput ? "yes" : "no");


   fprintf (stdout, "command:           ");
   for (int j = 0 ; j < argc; j++) {
      fprintf (stdout, "%s ", argv[j]);
   }
   fprintf (stdout, "\n");


   ProcessList childProcessList;
   for (int j = 0; j < MAXIMUM_CONNECTIONS; j++) {
      ProcessData* proc = &childProcessList [j];
      proc->pid = -1;
   }

   // construct lister socket bound to the specified port.
   //
   int listenFd = createListener (port);
   if (listenFd < 0) {
      // createListener does all the perror stuff required.
      //
      return 4;
   }

   setNonBlocking (listenFd);

   fprintf (stdout, "%s %d waiting for connections.\n", ownHostname (), port);

   while (true) {
      struct sockaddr_storage address;
      struct sockaddr* pAddress = (struct sockaddr *) &address;
      socklen_t size;

      // Mange current child processes if any.
      // wait pid nohang the child processes.
      // terminate if timeout exceeded.
      //
      checkUpOnTheKids (childProcessList, maximumSessions);

      // Check max connections
      //
      const int slot = findSlot (childProcessList, maximumSessions);
      if (slot < 0) {
         // Too busy to accept any more connections for now.
         // Sleep a bit and try again
         //
         delay (0.005);
         continue;
      }

      int connectionFd = accept (listenFd, pAddress, &size);
      if (connectionFd < 0) {
         // We are none blocking - check not "real" errors.
         //
         if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            perrorf ("accept (%d, ...)", listenFd);
         }

         // Sleep a bit and try again
         //
         delay (0.005);
         continue;
      }

      #define OCTET(n) int (pAddress->sa_data[n] >= 0 ? pAddress->sa_data[n] : pAddress->sa_data[n] + 256)

      fprintf (stdout, "Accept okay - we have a connection from: %d.%d.%d.%d\n",
               OCTET(2), OCTET(3), OCTET(4), OCTET(5));

      #undef OCTET

      // Use forking to create a child process that will do all the work.
      //
      pid_t pid = fork ();
      if (pid < 0) {
         perrorf ("fork ()");
         delay (0.005);
         continue;
      }

      if (pid > 0) {
         // We are the parent process
         // Close the incomming socket connection - we leave that to the child.
         //
         close (connectionFd);

         // Register child process details.
         //
         ProcessData* proc = &childProcessList [slot];
         proc->pid = pid;
         proc->expiryTime = getTimeSinceStart () + maximumTime;
         proc->state = psRunning;

         fprintf (stdout, "Process %s,%d starting.\n", argv[0], pid);

      } else {
         // We are the child process
         // Close the listening socket connection - we leave that to the parent.
         //
         close (listenFd);

         runChildProcess (connectionFd, argv,   // Does not return.
                          inputIsCompressed,    //
                          doCompressOutput);    //
         return 16;                             // belts 'n' braces
      }
   }

   close (listenFd);
   fprintf (stdout, "filter server complete\n");
   return 0;
}

// end
