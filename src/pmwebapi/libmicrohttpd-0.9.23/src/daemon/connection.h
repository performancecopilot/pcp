/*
     This file is part of libmicrohttpd
     (C) 2007 Daniel Pittman and Christian Grothoff

     This library is free software; you can redistribute it and/or
     modify it under the terms of the GNU Lesser General Public
     License as published by the Free Software Foundation; either
     version 2.1 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public
     License along with this library; if not, write to the Free Software
     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @file connection.h
 * @brief  Methods for managing connections
 * @author Daniel Pittman
 * @author Christian Grothoff
 */

#ifndef CONNECTION_H
#define CONNECTION_H

#include "internal.h"


/**
 * Obtain the select sets for this connection.   The given
 * sets (and the maximum) are updated and must have 
 * already been initialized.
 *
 * @param connection connetion to get select sets for
 * @param read_fd_set read set to initialize
 * @param write_fd_set write set to initialize
 * @param except_fd_set except set to initialize (never changed)
 * @param max_fd where to store largest FD put into any set
 * @return MHD_YES on success
 */
int
MHD_connection_get_fdset (struct MHD_Connection *connection,
                          fd_set * read_fd_set,
                          fd_set * write_fd_set,
                          fd_set * except_fd_set, int *max_fd);


/**
 * Obtain the pollfd for this connection. The poll interface allows large
 * file descriptors. Select goes stupid when the fd overflows fdset (which
 * is fixed).
 *
 * @param connection connetion to get poll set for 
 * @param p where to store the polling information
 */
int 
MHD_connection_get_pollfd (struct MHD_Connection *connection,
			   struct MHD_Pollfd *p);


/**
 * Set callbacks for this connection to those for HTTP.
 *
 * @param connection connection to initialize
 */
void 
MHD_set_http_callbacks_ (struct MHD_Connection *connection);


/**
 * This function handles a particular connection when it has been
 * determined that there is data to be read off a socket. All
 * implementations (multithreaded, external select, internal select)
 * call this function to handle reads.
 *
 * @param connection connection to handle
 * @return always MHD_YES (we should continue to process the
 *         connection)
 */
int 
MHD_connection_handle_read (struct MHD_Connection *connection);


/**
 * This function was created to handle writes to sockets when it has
 * been determined that the socket can be written to. All
 * implementations (multithreaded, external select, internal select)
 * call this function
 *
 * @param connection connection to handle
 * @return always MHD_YES (we should continue to process the
 *         connection)
 */
int 
MHD_connection_handle_write (struct MHD_Connection *connection);


/**
 * This function was created to handle per-connection processing that
 * has to happen even if the socket cannot be read or written to.  All
 * implementations (multithreaded, external select, internal select)
 * call this function.
 *
 * @param connection connection to handle
 * @return MHD_YES if we should continue to process the
 *         connection (not dead yet), MHD_NO if it died
 */
int 
MHD_connection_handle_idle (struct MHD_Connection *connection);


/**
 * Close the given connection and give the
 * specified termination code to the user.
 *
 * @param connection connection to close
 * @param termination_code termination reason to give
 */
void 
MHD_connection_close (struct MHD_Connection *connection,
		      enum MHD_RequestTerminationCode termination_code);


#endif
