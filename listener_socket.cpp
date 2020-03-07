// listener_socket.cpp
//
// Simple tcp/ip server that runs a filter.
//
// Copyright (c) 2019 Andrew Starritt
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

#include "listener_socket.h"
#include "utilities.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define BACKLOG               2

//------------------------------------------------------------------------------
//
int createListener (const int local_port)
{
   int fd = -1;
   char port_image [20];
   struct addrinfo hints;
   struct addrinfo* servinfo;
   struct addrinfo* p;
   int status;
   const int yes = 1;

   snprintf (port_image, sizeof (port_image), "%d", local_port);
   memset (&hints, 0, sizeof (hints));

   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;     // use my IP

   status = getaddrinfo (NULL, port_image, &hints, &servinfo);
   if (status != 0) {
      fprintf (stderr, "create_client.getaddrinfo (%s:%s) failed: %s\n",
               ownHostname (), port_image, gai_strerror (status));
      return -1;
   }

   // loop through all the results and connect to the first we can
   //
   int k = 0;
   for (p = servinfo; p != NULL; p = p->ai_next) {
      k++;
   }

   printf ("binding to %s:%s (%d instances)\n", ownHostname (), port_image, k);

   for (p = servinfo; p != NULL; p = p->ai_next) {

      fd = socket (p->ai_family, p->ai_socktype, p->ai_protocol);
      if (fd == -1) {
         perrorf ("createListener: socket (...)");
         continue;
      }

      status = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int));
      if (status == -1) {
         perrorf ("createListener: setsockopt (...)");
         return -1;
      }

      status = bind (fd, p->ai_addr, p->ai_addrlen);
      if (status == -1) {
         close (fd);
         perrorf ("createListener: bind (...)");
         continue;
      }

      // Got here and all good - therefore found it.
      //
      break;
   }

   if (p == NULL) {
      fprintf (stderr, "createListener: fail\n");
      return -1;
   }

   freeaddrinfo (servinfo);     // all done with this structure

   // mark the socket fd as a passive socket, that is, as a socket that will be
   // used to accept incoming connection requests using accept ().
   //
   status = listen (fd, BACKLOG);
   if (status == -1) {
      perrorf ("createListener: listen");
      return -1;
   }

   return fd;
}


// end

