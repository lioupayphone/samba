/*
   core of libctdb

   Copyright (C) Rusty Russell 2010
   Copyright (C) Ronnie Sahlberg 2011

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
//#include "config.h"
#include <sys/time.h>
#include <sys/socket.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <time.h>
#include "libctdb_private.h"
#include "io_elem.h"
#include "messages.h"
#include "lib/util/dlinklist.h"

/* Remove type-safety macros. */
#undef ctdb_connect

struct ctdb_lock {
	struct ctdb_lock *next, *prev;

	struct ctdb_db *ctdb_db;
	TDB_DATA key;

	/* Is this a request for read-only lock ? */
	bool readonly;

	/* This will always be set by the time user sees this. */
	unsigned long held_magic;
	struct ctdb_ltdb_header *hdr;

	/* For convenience, we stash original callback here. */
	ctdb_rrl_callback_t callback;
};

struct ctdb_db {
	struct libctdb_connection *ctdb;
	bool persistent;
	uint32_t tdb_flags;
	uint32_t id;
	struct tdb_context *tdb;

	ctdb_callback_t callback;
	void *private_data;
};

/* FIXME: for thread safety, need tid info too. */
static bool holding_lock(struct libctdb_connection *ctdb)
{
	/* For the moment, you can't ever hold more than 1 lock. */
	return (ctdb->locks != NULL);
}

/* FIXME: Could be in shared util code with rest of ctdb */
static void close_noerr(int fd)
{
	int olderr = errno;
	close(fd);
	errno = olderr;
}

/* FIXME: Could be in shared util code with rest of ctdb */
static void free_noerr(void *p)
{
	int olderr = errno;
	free(p);
	errno = olderr;
}

/* FIXME: Could be in shared util code with rest of ctdb */
static void set_nonblocking(int fd)
{
	unsigned v;
	v = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, v | O_NONBLOCK);
}

/* FIXME: Could be in shared util code with rest of ctdb */
static void set_close_on_exec(int fd)
{
	unsigned v;
	v = fcntl(fd, F_GETFD, 0);
        fcntl(fd, F_SETFD, v | FD_CLOEXEC);
}

void ctdb_cancel(struct libctdb_connection *ctdb, struct ctdb_request *req)
{
        if (!req->next && !req->prev) {
                DEBUG(ctdb, LOG_ALERT,
                      "ctdb_cancel: request completed! ctdb_request_free? %p (id %u)",
                      req, req->hdr.hdr ? req->hdr.hdr->reqid : 0);
                ctdb_request_free(req);
                return;
        }

        DEBUG(ctdb, LOG_DEBUG, "ctdb_cancel: %p (id %u)",
              req, req->hdr.hdr ? req->hdr.hdr->reqid : 0);

        /* FIXME: If it's not sent, we could just free it right now. */
        req->callback = ctdb_cancel_callback;
}


void ctdb_request_free(struct ctdb_request *req)
{
	struct libctdb_connection *ctdb = req->ctdb;

	if (req->next || req->prev) {
		DEBUG(ctdb, LOG_ALERT,
		      "ctdb_request_free: request not complete! ctdb_cancel? %p (id %u)",
		      req, req->hdr.hdr ? req->hdr.hdr->reqid : 0);
		ctdb_cancel(ctdb, req);
		return;
	}
	if (req->extra_destructor) {
		req->extra_destructor(ctdb, req);
	}
	if (req->reply) {
		free_io_elem(req->reply);
	}
	free_io_elem(req->io);
	free(req);
}

static void set_pnn(struct libctdb_connection *ctdb,
		    struct ctdb_request *req,
		    void *unused)
{
	if (!ctdb_getpnn_recv(ctdb, req, &ctdb->pnn)) {
		DEBUG(ctdb, LOG_CRIT,
		      "ctdb_connect(async): failed to get pnn");
		ctdb->broken = true;
	}
	ctdb_request_free(req);
}

struct libctdb_connection *ctdb_connect(const char *addr,
				     ctdb_log_fn_t log_fn, void *log_priv)
{
	struct libctdb_connection *ctdb;
	struct sockaddr_un sun;

	ctdb = malloc(sizeof(*ctdb));
	if (!ctdb) {
		/* With no format string, we hope it doesn't use ap! */
		va_list ap;
		memset(&ap, 0, sizeof(ap));
		errno = ENOMEM;
		log_fn(log_priv, LOG_ERR, "ctdb_connect: no memory", ap);
		goto fail;
	}
	ctdb->pnn = -1;
	ctdb->outq = NULL;
	ctdb->doneq = NULL;
	ctdb->in = NULL;
	ctdb->inqueue = NULL;
	ctdb->message_handlers = NULL;
	ctdb->next_id = 0;
	ctdb->broken = false;
	ctdb->log = log_fn;
	ctdb->log_priv = log_priv;
	ctdb->locks = NULL;

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (!addr)
		addr = CTDB_SOCKET;
	strncpy(sun.sun_path, addr, sizeof(sun.sun_path)-1);
	ctdb->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ctdb->fd < 0)
		goto free_fail;

	if (connect(ctdb->fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		goto close_fail;

	set_nonblocking(ctdb->fd);
	set_close_on_exec(ctdb->fd);

	/* Immediately queue a request to get our pnn. */
	if (!ctdb_getpnn_send(ctdb, CTDB_CURRENT_NODE, set_pnn, NULL))
		goto close_fail;

	return ctdb;

close_fail:
	close_noerr(ctdb->fd);
free_fail:
	free_noerr(ctdb);
fail:
	return NULL;
}

void ctdb_disconnect(struct libctdb_connection *ctdb)
{
	struct ctdb_request *i;

	DEBUG(ctdb, LOG_DEBUG, "ctdb_disconnect");

	while ((i = ctdb->outq) != NULL) {
		DLIST_REMOVE(ctdb->outq, i);
		ctdb_request_free(i);
	}

	while ((i = ctdb->doneq) != NULL) {
		DLIST_REMOVE(ctdb->doneq, i);
		ctdb_request_free(i);
	}

	if (ctdb->in)
		free_io_elem(ctdb->in);

	remove_message_handlers(ctdb);

	close(ctdb->fd);
	/* Just in case they try to reuse */
	ctdb->fd = -1;
	free(ctdb);
}

int ctdb_get_fd(struct libctdb_connection *ctdb)
{
	return ctdb->fd;
}

int ctdb_which_events(struct libctdb_connection *ctdb)
{
	int events = POLLIN;

	if (ctdb->outq)
		events |= POLLOUT;
	return events;
}

struct ctdb_request *new_ctdb_request(struct libctdb_connection *ctdb, size_t len,
				      ctdb_callback_t cb, void *cbdata)
{
	struct ctdb_request *req = malloc(sizeof(*req));
	if (!req)
		return NULL;
	req->io = new_io_elem(len);
	if (!req->io) {
		free(req);
		return NULL;
	}
	req->ctdb = ctdb;
	req->hdr.hdr = io_elem_data(req->io, NULL);
	req->reply = NULL;
	req->callback = cb;
	req->priv_data = cbdata;
	req->extra = NULL;
	req->extra_destructor = NULL;
	return req;
}

/* Sanity-checking wrapper for reply. */
static struct ctdb_reply_call_old *unpack_reply_call(struct ctdb_request *req,
						 uint32_t callid)
{
	size_t len;
	struct ctdb_reply_call_old *inhdr = io_elem_data(req->reply, &len);

	/* Library user error if this isn't a reply to a call. */
	if (req->hdr.hdr->operation != CTDB_REQ_CALL) {
		errno = EINVAL;
		DEBUG(req->ctdb, LOG_ALERT,
		      "This was not a ctdbd call request: operation %u",
		      req->hdr.hdr->operation);
		return NULL;
	}

	if (req->hdr.call->callid != callid) {
		errno = EINVAL;
		DEBUG(req->ctdb, LOG_ALERT,
		      "This was not a ctdbd %u call request: %u",
		      callid, req->hdr.call->callid);
		return NULL;
	}

	/* ctdbd or our error if this isn't a reply call. */
	if (len < sizeof(*inhdr) || inhdr->hdr.operation != CTDB_REPLY_CALL) {
		errno = EIO;
		DEBUG(req->ctdb, LOG_CRIT,
		      "Invalid ctdbd call reply: len %zu, operation %u",
		      len, inhdr->hdr.operation);
		return NULL;
	}

	return inhdr;
}

/* Sanity-checking wrapper for reply. */
struct ctdb_reply_control_old *unpack_reply_control(struct ctdb_request *req,
						enum ctdb_controls control)
{
	size_t len;
	struct ctdb_reply_control_old *inhdr = io_elem_data(req->reply, &len);

	/* Library user error if this isn't a reply to a call. */
	if (len < sizeof(*inhdr)) {
		errno = EINVAL;
		DEBUG(req->ctdb, LOG_ALERT,
		      "Short ctdbd control reply: %zu bytes", len);
		return NULL;
	}
	if (req->hdr.hdr->operation != CTDB_REQ_CONTROL) {
		errno = EINVAL;
		DEBUG(req->ctdb, LOG_ALERT,
		      "This was not a ctdbd control request: operation %u",
		      req->hdr.hdr->operation);
		return NULL;
	}

	/* ... or if it was a different control from what we expected. */
	if (req->hdr.control->opcode != control) {
		errno = EINVAL;
		DEBUG(req->ctdb, LOG_ALERT,
		      "This was not an opcode %u ctdbd control request: %u",
		      control, req->hdr.control->opcode);
		return NULL;
	}

	/* ctdbd or our error if this isn't a reply call. */
	if (inhdr->hdr.operation != CTDB_REPLY_CONTROL) {
		errno = EIO;
		DEBUG(req->ctdb, LOG_CRIT,
		      "Invalid ctdbd control reply: operation %u",
		      inhdr->hdr.operation);
		return NULL;
	}

	return inhdr;
}

static void handle_incoming(struct libctdb_connection *ctdb, struct io_elem *in)
{
	struct ctdb_req_header *hdr;
	size_t len;
	struct ctdb_request *i;

	hdr = io_elem_data(in, &len);
	/* FIXME: use len to check packet! */

	if (hdr->operation == CTDB_REQ_MESSAGE) {
		deliver_message(ctdb, hdr);
		if (in)
			free_io_elem(in);
		return;
	}

	for (i = ctdb->doneq; i; i = i->next) {
		if (i->hdr.hdr->reqid == hdr->reqid) {
			DLIST_REMOVE(ctdb->doneq, i);
			i->reply = in;
			i->callback(ctdb, i, i->priv_data);
			return;
		}
	}
	DEBUG(ctdb, LOG_WARNING,
	      "Unexpected ctdbd request reply: operation %u reqid %u",
	      hdr->operation, hdr->reqid);
	free_io_elem(in);
}

/* Remove "harmless" errors. */
static ssize_t real_error(ssize_t ret)
{
	if (ret < 0 && (errno == EINTR || errno == EWOULDBLOCK))
		return 0;
	return ret;
}

bool ctdb_service(struct libctdb_connection *ctdb, int revents)
{
	if (ctdb->broken) {
		return false;
	}

	if (holding_lock(ctdb)) {
		DEBUG(ctdb, LOG_ALERT, "Do not block while holding lock!");
	}

	if (revents & POLLOUT) {
		while (ctdb->outq) {
			if (real_error(write_io_elem(ctdb->fd,
						     ctdb->outq->io)) < 0) {
				DEBUG(ctdb, LOG_ERR,
				      "ctdb_service: error writing to ctdbd");
				ctdb->broken = true;
				return false;
			}
			if (io_elem_finished(ctdb->outq->io)) {
				struct ctdb_request *done = ctdb->outq;
				DLIST_REMOVE(ctdb->outq, done);
				/* We add at the head: any dead ones
				 * sit and end. */
				DLIST_ADD(ctdb->doneq, done);
			}
		}
	}

	while (revents & POLLIN) {
		int ret;
		int num_ready = 0;

		if (ioctl(ctdb->fd, FIONREAD, &num_ready) != 0) {
			DEBUG(ctdb, LOG_ERR,
			      "ctdb_service: ioctl(FIONREAD) %d", errno);
			ctdb->broken = true;
			return false;
		}
		if (num_ready == 0) {
			/* the descriptor has been closed or we have all our data */
			break;
		}


		if (!ctdb->in) {
			ctdb->in = new_io_elem(sizeof(struct ctdb_req_header));
			if (!ctdb->in) {
				DEBUG(ctdb, LOG_ERR,
				      "ctdb_service: allocating readbuf");
				ctdb->broken = true;
				return false;
			}
		}

		ret = read_io_elem(ctdb->fd, ctdb->in);
		if (real_error(ret) < 0 || ret == 0) {
			/* They closed fd? */
			if (ret == 0)
				errno = EBADF;
			DEBUG(ctdb, LOG_ERR,
			      "ctdb_service: error reading from ctdbd");
			ctdb->broken = true;
			return false;
		} else if (ret < 0) {
			/* No progress, stop loop. */
			break;
		} else if (io_elem_finished(ctdb->in)) {
			io_elem_queue(ctdb, ctdb->in);
			ctdb->in = NULL;
		}
	}


	while (ctdb->inqueue != NULL) {
		struct io_elem *io = ctdb->inqueue;

		io_elem_dequeue(ctdb, io);
		handle_incoming(ctdb, io);
	}

	return true;
}

/* This is inefficient.  We could pull in idtree.c. */
static bool reqid_used(const struct libctdb_connection *ctdb, uint32_t reqid)
{
	struct ctdb_request *i;

	for (i = ctdb->outq; i; i = i->next) {
		if (i->hdr.hdr->reqid == reqid) {
			return true;
		}
	}
	for (i = ctdb->doneq; i; i = i->next) {
		if (i->hdr.hdr->reqid == reqid) {
			return true;
		}
	}
	return false;
}

uint32_t new_reqid(struct libctdb_connection *ctdb)
{
	while (reqid_used(ctdb, ctdb->next_id)) {
		ctdb->next_id++;
	}
	return ctdb->next_id++;
}

struct ctdb_request *new_ctdb_control_request(struct libctdb_connection *ctdb,
					      uint32_t opcode,
					      uint32_t destnode,
					      const void *extra_data,
					      size_t extra,
					      ctdb_callback_t callback,
					      void *cbdata)
{
	struct ctdb_request *req;
	struct ctdb_req_control_old *pkt;

	req = new_ctdb_request(
		ctdb, offsetof(struct ctdb_req_control_old, data) + extra,
		callback, cbdata);
	if (!req)
		return NULL;

	io_elem_init_req_header(req->io,
				CTDB_REQ_CONTROL, destnode, new_reqid(ctdb));

	pkt = req->hdr.control;
	pkt->pad = 0;
	pkt->opcode = opcode;
	pkt->srvid = 0;
	pkt->client_id = 0;
	pkt->flags = 0;
	pkt->datalen = extra;
	memcpy(pkt->data, extra_data, extra);
	DLIST_ADD(ctdb->outq, req);
	return req;
}

void ctdb_cancel_callback(struct libctdb_connection *ctdb,
			  struct ctdb_request *req,
			  void *unused)
{
	ctdb_request_free(req);
}
