/*
   core of libctdb

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

#include <sys/socket.h>
#include "libctdb_private.h"
#include "messages.h"
#include "io_elem.h"
#include <tdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "libctdb_private.h"
#include <samba-4.0/dlinklist.h>

/* Remove type-safety macros. */
#undef ctdb_set_message_handler_send
#undef ctdb_set_message_handler_recv
#undef ctdb_remove_message_handler_send

struct message_handler_info {
	struct message_handler_info *next, *prev;

	uint64_t srvid;
	ctdb_message_fn_t handler;
	void *handler_data;
};

void deliver_message(struct libctdb_connection *ctdb, struct ctdb_req_header *hdr)
{
	struct message_handler_info *i;
	struct ctdb_req_message_old *msg = (struct ctdb_req_message_old *)hdr;
	TDB_DATA data;
	bool found;

	data.dptr = msg->data;
	data.dsize = msg->datalen;

	/* Note: we want to call *every* handler: there may be more than one */
	for (i = ctdb->message_handlers; i; i = i->next) {
		if (i->srvid == msg->srvid) {
			i->handler(ctdb, msg->srvid, data, i->handler_data);
			found = true;
		}
	}
	if (!found) {
		DEBUG(ctdb, LOG_WARNING,
		      "ctdb_service: messsage for unregistered srvid %llu",
		      (unsigned long long)msg->srvid);
	}
}

void remove_message_handlers(struct libctdb_connection *ctdb)
{
	struct message_handler_info *i;

	/* ctdbd should unregister automatically when we close fd, so we don't
	   need to do that here. */
	while ((i = ctdb->message_handlers) != NULL) {
		DLIST_REMOVE(ctdb->message_handlers, i);
		free(i);
	}
}
