#ifndef _LIBCTDB_MESSAGE_H
#define _LIBCTDB_MESSAGE_H
struct message_handler_info;
struct libctdb_connection;
struct ctdb_req_header;

void deliver_message(struct libctdb_connection *ctdb, struct ctdb_req_header *hdr);
void remove_message_handlers(struct libctdb_connection *ctdb);
#endif /* _LIBCTDB_MESSAGE_H */
