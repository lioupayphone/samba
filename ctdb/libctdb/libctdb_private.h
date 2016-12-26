#ifndef _LIBCTDB_PRIVATE_H
#define _LIBCTDB_PRIVATE_H
//#include <dlinklist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <libctdb.h>
#include <syslog.h>
#include <tdb.h>
#include <stddef.h>

#ifndef offsetof
#define offsetof(t,f) ((unsigned int)&((t *)0)->f)
#endif

#ifndef COLD_ATTRIBUTE
#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
#define COLD_ATTRIBUTE __attribute__((cold))
#else
#define COLD_ATTRIBUTE
#endif
#endif /* COLD_ATTRIBUTE */

#define DEBUG(ctdb, lvl, format, args...) do { if (lvl <= ctdb_log_level) { ctdb_do_debug(ctdb, lvl, format , ## args ); }} while(0)

struct message_handler_info;
struct ctdb_reply_call;

struct ctdb_request {
	struct libctdb_connection *ctdb;
	struct ctdb_request *next, *prev;
	bool cancelled;

	struct io_elem *io;
	union {
		struct ctdb_req_header *hdr;
		struct ctdb_req_call_old *call;
		struct ctdb_req_control_old *control;
		struct ctdb_req_message_old *message;
	} hdr;

	struct io_elem *reply;

	ctdb_callback_t callback;
	void *priv_data;

	/* Extra per-request info. */
	void (*extra_destructor)(struct libctdb_connection *,
				 struct ctdb_request *);
	void *extra;
};


/* ctdb.c */
struct ctdb_request *new_ctdb_request(struct libctdb_connection *ctdb, size_t len,
				      ctdb_callback_t cb, void *cbdata);
struct ctdb_request *new_ctdb_control_request(struct libctdb_connection *ctdb,
					      uint32_t opcode,
					      uint32_t destnode,
					      const void *extra_data,
					      size_t extra,
					      ctdb_callback_t, void *);
uint32_t new_reqid(struct libctdb_connection *ctdb);

struct ctdb_reply_control_old *unpack_reply_control(struct ctdb_request *req,
						enum ctdb_controls control);
void ctdb_cancel_callback(struct libctdb_connection *ctdb,
			  struct ctdb_request *req,
			  void *unused);

/* logging.c */

void ctdb_do_debug(struct libctdb_connection *, int, const char *format, ...)
	PRINTF_ATTRIBUTE(3, 4) COLD_ATTRIBUTE;

#endif /* _LIBCTDB_PRIVATE_H */
