/*
   ctdb database library

   Copyright (C) Ronnie sahlberg 2010
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

#ifndef _LIBCTDB_H
#define _LIBCTDB_H
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <tdb.h>
#include <netinet/in.h>
#include <ctdb_protocol.h>

extern int ctdb_log_level;
struct ctdb_request;
struct ctdb_db;
struct ctdb_lock;
struct io_elem;

typedef void (*ctdb_log_fn_t)(void *log_priv,
                              int severity, const char *format, va_list ap);

#define CTDB_SOCKET "/var/run/ctdb/ctdbd.socket"

struct libctdb_connection {
        /* Socket to ctdbd. */
        int fd;
        /* Currently our failure mode is simple; return -1 from ctdb_service */
        bool broken;
        /* Linked list of pending outgoings. */
        struct ctdb_request *outq;
        /* Finished outgoings (awaiting response) */
        struct ctdb_request *doneq;

        /* Current incoming. */
        struct io_elem *in;
        /* Queue of received pdus */
        struct io_elem *inqueue;

        /* Guess at a good reqid to try next. */
        uint32_t next_id;
        /* List of messages */
        struct message_handler_info *message_handlers;
        /* PNN of this ctdb: valid by the time we do our first db connection. */
        uint32_t pnn;
        /* Chain of locks we hold. */
        struct ctdb_lock *locks;
        /* Extra logging. */
        ctdb_log_fn_t log;
        void *log_priv;
};

/**
 * ctdb_callback_t - callback for completed requests.
 *
 * This would normally unpack the request using ctdb_*_recv().  You
 * must free the request using ctdb_request_free().
 *
 * Note that due to macro magic, actual your callback can be typesafe:
 * instead of taking a void *, it can take a type which matches the
 * actual private parameter.
 */
typedef void (*ctdb_callback_t)(struct libctdb_connection *ctdb,
				struct ctdb_request *req, void *private_data);



/**
 * ctdb_rrl_callback_t - callback for ctdb_readrecordlock_async
 *
 * This is not the standard ctdb_callback_t, because there is often no
 * request required to access a database record (ie. if it is local already).
 * So the callback is handed the lock directly: it might be NULL if there
 * was an error obtaining the lock.
 *
 * See Also:
 *	ctdb_readrecordlock_async(), ctdb_readrecordlock()
 */
typedef void (*ctdb_rrl_callback_t)(struct ctdb_db *ctdb_db,
				    struct ctdb_lock *lock,
				    TDB_DATA data,
				    void *private_data);

typedef void (*ctdb_message_fn_t)(struct libctdb_connection *,
				  uint64_t srvid, TDB_DATA data, void *);


/**
 * ctdb - a library for accessing tdbs controlled by ctdbd
 *
 * ctdbd (clustered tdb daemon) is a daemon designed to syncronize TDB
 * databases across a cluster.  Using this library, you can communicate with
 * the daemon to access the databases, pass messages across the cluster, and
 * control the daemon itself.
 *
 * The general API is event-driven and asynchronous: you call the
 * *_send functions, supplying callbacks, then when the ctdbd file
 * descriptor is usable, call ctdb_service() to perform read from it
 * and call your callbacks, which use the *_recv functions to unpack
 * the replies from ctdbd.
 *
 * There is also a synchronous wrapper for each function for trivial
 * programs; these can be found in the section marked "Synchronous API".
 */

/**
 * ctdb_log_fn_t - logging function for ctdbd
 * @log_priv: private (typesafe) arg via ctdb_connect
 * @severity: syslog-style severity
 * @format: printf-style format string.
 * @ap: arguments for formatting.
 *
 * The severity passed to log() are as per syslog(3).  In particular,
 * LOG_DEBUG is used for tracing, LOG_WARNING is used for unusual
 * conditions which don't necessarily return an error through the API,
 * LOG_ERR is used for errors such as lost communication with ctdbd or
 * out-of-memory, LOG_ALERT is used for library usage bugs, LOG_CRIT is
 * used for libctdb internal consistency checks.
 *
 * The log() function can be typesafe: the @log_priv arg to
 * ctdb_donnect and signature of log() should match.
 */
/*typedef void (*ctdb_log_fn_t)(void *log_priv,
			      int severity, const char *format, va_list ap);*/

/**
 * ctdb_connect - connect to ctdb using the specified domain socket.
 * @addr: the socket address, or NULL for default
 * @log: the logging function
 * @log_priv: the private argument to the logging function.
 *
 * Returns a ctdb context if successful or NULL.  Use ctdb_disconnect() to
 * release the returned ctdb_connection when finished.
 *
 * See Also:
 *	ctdb_log_fn_t, ctdb_log_file()
 */
struct libctdb_connection *ctdb_connect(const char *addr,
				     ctdb_log_fn_t log_fn, void *log_priv);





bool ctdb_getpublicips(struct libctdb_connection *ctdb,
		       uint32_t destnode, struct ctdb_public_ip_list_old **ips);


void ctdb_free_publicips(struct ctdb_public_ip_list_old *ips);
/**
 * ctdb_disconnect - close down a connection to ctdbd.
 * @ctdb: the ctdb connectio returned from ctdb_connect.
 *
 * The @ctdb arg will be freed by this call, and must not be used again.
 */
void ctdb_disconnect(struct libctdb_connection *ctdb);

void ctdb_log_file(FILE *outf, int priority, const char *format, va_list ap);
#endif
