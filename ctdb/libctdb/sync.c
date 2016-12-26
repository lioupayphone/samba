/*
   synchronous wrappers for libctdb

   Copyright (C) Rusty Russell 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <sys/time.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include "libctdb_private.h"

/* Remove type-safety macros. */

/* On failure, frees req and returns NULL. */
static struct ctdb_request *synchronous(struct libctdb_connection *ctdb,
					struct ctdb_request *req,
					bool *done)
{
	struct pollfd fds;

	/* Pass through allocation failures. */
	if (!req)
		return NULL;

	fds.fd = ctdb_get_fd(ctdb);
	while (!*done) {
		fds.events = ctdb_which_events(ctdb);
		if (poll(&fds, 1, -1) < 0) {
			/* Signalled is OK, other error is bad. */
			if (errno == EINTR)
				continue;
			ctdb_cancel(ctdb, req);
			DEBUG(ctdb, LOG_ERR, "ctdb_synchronous: poll failed");
			return NULL;
		}
		if (!ctdb_service(ctdb, fds.revents)) {
			/* It can have failed after it completed request. */
			if (!*done)
				ctdb_cancel(ctdb, req);
			else
				ctdb_request_free(req);
			return NULL;
		}
	}
	return req;
}

static void set(struct libctdb_connection *ctdb,
		struct ctdb_request *req, void *done)
{
	*(bool *)done = true;
}

bool ctdb_getpublicips(struct libctdb_connection *ctdb,
		       uint32_t destnode, struct ctdb_public_ip_list_old **ips)
{
	struct ctdb_request *req;
	bool done = false;
	bool ret = false;

	*ips = NULL;
        req = ctdb_getpublicips_send(ctdb, destnode, set, (void *)&done);
        if(req == NULL)
                return ret;
	req = synchronous(ctdb,req,&done);
	if (req != NULL) {
		ret = ctdb_getpublicips_recv(ctdb, req, ips);
		ctdb_request_free(req);
	}
	return ret;
}

void ctdb_free_publicips(struct ctdb_public_ip_list_old *ips)
{
	if (ips == NULL) {
		return;
	}
	free(ips);
}
