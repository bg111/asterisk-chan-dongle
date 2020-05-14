/*
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/

#ifndef CHAN_DONGLE_SMSDB_H_INCLUDED
#define CHAN_DONGLE_SMSDB_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

#define SMSDB_PAYLOAD_MAX_LEN 4096
#define SMSDB_DST_MAX_LEN 256

EXPORT_DECL int smsdb_init();
EXPORT_DECL void smsdb_atexit();
EXPORT_DECL int smsdb_put(const char *id, const char *addr, int ref, int parts, int order, const char *msg, char *out);
EXPORT_DECL int smsdb_get_refid(const char *id, const char *addr);
EXPORT_DECL int smsdb_outgoing_add(const char *id, const char *addr, int cnt, int ttl, int srr, const char *payload, size_t len);
EXPORT_DECL ssize_t smsdb_outgoing_clear(int uid, char *dst, char *payload);
EXPORT_DECL ssize_t smsdb_outgoing_part_put(int uid, int refid, char *dst, char *payload);
EXPORT_DECL ssize_t smsdb_outgoing_part_status(const char *id, const char *addr, int mr, int st, int *status_all, char *payload);
EXPORT_DECL ssize_t smsdb_outgoing_purge_one(char *dst, char *payload);

#endif
