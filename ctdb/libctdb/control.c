/*
   Misc control routines of libctdb

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
#include <string.h>
#include <stdint.h>
#include "libctdb_private.h"

/* Remove type-safety macros. */



bool ctdb_getpnn_recv(struct libctdb_connection *ctdb,
		     struct ctdb_request *req, uint32_t *pnn)
{
	struct ctdb_reply_control_old *reply;

	reply = unpack_reply_control(req, CTDB_CONTROL_GET_PNN);
	if (!reply) {
		return false;
	}
	if (reply->status == -1) {
		DEBUG(ctdb, LOG_ERR, "ctdb_getpnn_recv: status -1");
		return false;
	}
	/* Note: data is stashed in status - see ctdb_control_dispatch() */
	*pnn = reply->status;
	return true;
}

struct ctdb_request *ctdb_getpnn_send(struct libctdb_connection *ctdb,
				      uint32_t destnode,
				      ctdb_callback_t callback,
				      void *private_data)
{
	return new_ctdb_control_request(ctdb, CTDB_CONTROL_GET_PNN, destnode,
					NULL, 0, callback, private_data);
}


bool ctdb_getpublicips_recv(struct libctdb_connection *ctdb,
			    struct ctdb_request *req,
			    struct ctdb_public_ip_list_old **ips)
{
	struct ctdb_reply_control_old *reply;

	*ips = NULL;
	reply = unpack_reply_control(req, CTDB_CONTROL_GET_PUBLIC_IPS);
	if (!reply) {
		return false;
	}
	if (reply->status == -1) {
		DEBUG(ctdb, LOG_ERR, "ctdb_getpublicips_recv: status -1");
		return false;
	}
	if (reply->datalen == 0) {
		DEBUG(ctdb, LOG_ERR, "ctdb_getpublicips_recv: returned data is 0 bytes");
		return false;
	}

	*ips = malloc(reply->datalen);
	if (*ips == NULL) {
		DEBUG(ctdb, LOG_ERR, "ctdb_getpublicips_recv: failed to malloc buffer");
		return false;
	}
	memcpy(*ips, reply->data, reply->datalen);

	return true;
}
struct ctdb_request *ctdb_getpublicips_send(struct libctdb_connection *ctdb,
					    uint32_t destnode,
					    ctdb_callback_t callback,
					    void *private_data)
{
	return new_ctdb_control_request(ctdb, CTDB_CONTROL_GET_PUBLIC_IPS,
					destnode,
					NULL, 0, callback, private_data);
}

